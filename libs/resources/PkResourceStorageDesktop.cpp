/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "PkResourceStorageDesktop.h"

#include <PkString.h>

#include <unicode/ustring.h>
#include <unicode/stringoptions.h>
#include <unicode/utf16.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <system_error>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

PkString fromPath(const fs::path &path)
{
    const std::string text = path.u8string();
    return PkString::PkFromUtf8(text.c_str(), static_cast<int>(text.size()));
}

fs::path toPath(const PkString &path)
{
    return fs::u8path(path.PkToUtf8());
}

int64_t lastModifiedMs(const fs::path &path)
{
    std::error_code ec;
    const fs::file_time_type writeTime = fs::last_write_time(path, ec);
    if (ec) {
        return 0;
    }
    const auto systemTime = std::chrono::time_point_cast<std::chrono::milliseconds>(
        writeTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    return systemTime.time_since_epoch().count();
}

PkString environmentPath(const char *name)
{
#ifdef _WIN32
    std::wstring wideName;
    for (const unsigned char character : std::string(name)) {
        wideName.push_back(static_cast<wchar_t>(character));
    }
    const wchar_t *value = ::_wgetenv(wideName.c_str());
    return value && *value ? fromPath(fs::path(value)) : PkString();
#else
    const char *value = std::getenv(name);
    return value && *value ? PkString(value) : PkString();
#endif
}

bool isHidden(const fs::path &path)
{
    const std::string name = path.filename().u8string();
    if (!name.empty() && name.front() == '.') {
        return true;
    }
#ifdef _WIN32
    const DWORD attributes = ::GetFileAttributesW(path.c_str());
    return attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
    return false;
#endif
}

bool hasEntryKind(const fs::directory_entry &entry, PkResourceStorage::EntryKind kind)
{
    std::error_code ec;
    fs::file_status status = entry.symlink_status(ec);
    if (ec) {
        return false;
    }
    if (fs::is_symlink(status)) {
        status = entry.status(ec);
        if (ec) {
            return false;
        }
    }
    return kind == PkResourceStorage::EntryKind::Files
        ? fs::is_regular_file(status) : fs::is_directory(status);
}

bool isReadable(const fs::path &path, PkResourceStorage::EntryKind kind)
{
#ifdef _WIN32
    const HANDLE handle = ::CreateFileW(path.c_str(), GENERIC_READ,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    BY_HANDLE_FILE_INFORMATION information {};
    const bool informationOk = ::GetFileInformationByHandle(handle, &information) != 0;
    ::CloseHandle(handle);
    if (!informationOk) {
        return false;
    }
    const bool isDirectory = (information.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    return kind == PkResourceStorage::EntryKind::Directories ? isDirectory : !isDirectory;
#else
    int flags = O_RDONLY;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
#ifdef O_NONBLOCK
    flags |= O_NONBLOCK;
#endif
#ifdef O_DIRECTORY
    if (kind == PkResourceStorage::EntryKind::Directories) {
        flags |= O_DIRECTORY;
    }
#endif
    const int descriptor = ::open(path.c_str(), flags);
    if (descriptor < 0) {
        return false;
    }
    struct stat status {};
    const bool statusOk = ::fstat(descriptor, &status) == 0;
    const bool rightKind = statusOk &&
        (kind == PkResourceStorage::EntryKind::Files
             ? S_ISREG(status.st_mode) : S_ISDIR(status.st_mode));
    if (!rightKind) {
        ::close(descriptor);
        return false;
    }

    if (kind != PkResourceStorage::EntryKind::Directories) {
        ::close(descriptor);
        return true;
    }

    // Resolve "." relative to the already validated directory descriptor to
    // require traversal permission without reopening a raceable path.
    const int traversal = ::openat(descriptor, ".", flags);
    ::close(descriptor);
    if (traversal < 0) {
        return false;
    }
    ::close(traversal);
    return true;
#endif
}

bool decodeUnicodeScalars(const PkString &value, std::vector<UChar32> &result)
{
    const std::u16string utf16 = value.PkToU16();
    result.clear();
    for (int32_t offset = 0; offset < static_cast<int32_t>(utf16.size());) {
        UChar32 character = 0;
        U16_NEXT(utf16.data(), offset, static_cast<int32_t>(utf16.size()), character);
        if (character < 0) {
            result.clear();
            return false;
        }
        result.push_back(character);
    }
    return true;
}

bool caseFoldScalars(const std::vector<UChar32> &value, std::vector<UChar32> &result)
{
    std::vector<UChar> source;
    source.reserve(value.size() * 2u);
    for (UChar32 character : value) {
        if (!U_IS_UNICODE_CHAR(character)) {
            return false;
        }
        if (U16_LENGTH(character) == 1) {
            source.push_back(static_cast<UChar>(character));
        } else {
            source.push_back(U16_LEAD(character));
            source.push_back(U16_TRAIL(character));
        }
    }

    UErrorCode error = U_ZERO_ERROR;
    const int32_t required = u_strFoldCase(nullptr, 0, source.data(),
                                           static_cast<int32_t>(source.size()),
                                           U_FOLD_CASE_DEFAULT, &error);
    if (error != U_BUFFER_OVERFLOW_ERROR && U_FAILURE(error)) {
        return false;
    }
    error = U_ZERO_ERROR;
    std::vector<UChar> folded(static_cast<std::size_t>(required) + 1u);
    const int32_t length = u_strFoldCase(folded.data(), required + 1,
                                         source.data(), static_cast<int32_t>(source.size()),
                                         U_FOLD_CASE_DEFAULT, &error);
    if (U_FAILURE(error)) {
        return false;
    }

    result.clear();
    for (int32_t offset = 0; offset < length;) {
        UChar32 character = 0;
        U16_NEXT(folded.data(), offset, length, character);
        if (character < 0) {
            result.clear();
            return false;
        }
        result.push_back(character);
    }
    return true;
}

struct GlobToken {
    enum class Kind {
        Literal,
        Question,
        Star
    };

    Kind kind;
    std::vector<UChar32> foldedLiteral;
};

bool tokenizeGlob(const PkString &patternValue, std::vector<GlobToken> &tokens)
{
    std::vector<UChar32> pattern;
    if (!decodeUnicodeScalars(patternValue, pattern)) {
        return false;
    }

    tokens.clear();
    std::vector<UChar32> literal;
    const auto flushLiteral = [&tokens, &literal]() {
        if (literal.empty()) {
            return true;
        }
        GlobToken token{GlobToken::Kind::Literal, {}};
        if (!caseFoldScalars(literal, token.foldedLiteral)) {
            return false;
        }
        tokens.push_back(std::move(token));
        literal.clear();
        return true;
    };

    for (UChar32 character : pattern) {
        if (character != '*' && character != '?') {
            literal.push_back(character);
            continue;
        }
        if (!flushLiteral()) {
            return false;
        }
        const GlobToken::Kind kind = character == '*'
            ? GlobToken::Kind::Star : GlobToken::Kind::Question;
        if (kind != GlobToken::Kind::Star || tokens.empty() ||
            tokens.back().kind != GlobToken::Kind::Star) {
            tokens.push_back(GlobToken{kind, {}});
        }
    }
    return flushLiteral();
}

bool globMatches(const PkString &patternValue, const PkString &textValue)
{
    std::vector<GlobToken> pattern;
    std::vector<UChar32> text;
    if (!tokenizeGlob(patternValue, pattern) || !decodeUnicodeScalars(textValue, text)) {
        return false;
    }

    std::vector<std::vector<UChar32>> foldedText(text.size());
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (!caseFoldScalars(std::vector<UChar32>{text[i]}, foldedText[i])) {
            return false;
        }
    }

    const auto consumeLiteral = [&foldedText](const std::vector<UChar32> &literal,
                                               std::size_t start,
                                               std::size_t &end) {
        std::size_t foldedOffset = 0;
        for (std::size_t textOffset = start; textOffset < foldedText.size(); ++textOffset) {
            for (UChar32 character : foldedText[textOffset]) {
                if (foldedOffset >= literal.size() || literal[foldedOffset] != character) {
                    return false;
                }
                ++foldedOffset;
            }
            if (foldedOffset == literal.size()) {
                end = textOffset + 1;
                return true;
            }
        }
        return false;
    };

    const std::size_t rowSize = text.size() + 1u;
    std::vector<signed char> memo((pattern.size() + 1u) * rowSize, -1);
    std::function<bool(std::size_t, std::size_t)> matchFrom =
        [&](std::size_t patternOffset, std::size_t textOffset) {
            signed char &cached = memo[patternOffset * rowSize + textOffset];
            if (cached >= 0) {
                return cached != 0;
            }

            bool matched = false;
            if (patternOffset == pattern.size()) {
                matched = textOffset == text.size();
            } else {
                const GlobToken &token = pattern[patternOffset];
                switch (token.kind) {
                case GlobToken::Kind::Star:
                    matched = matchFrom(patternOffset + 1u, textOffset) ||
                        (textOffset < text.size() && matchFrom(patternOffset, textOffset + 1u));
                    break;
                case GlobToken::Kind::Question:
                    matched = textOffset < text.size() &&
                        matchFrom(patternOffset + 1u, textOffset + 1u);
                    break;
                case GlobToken::Kind::Literal: {
                    std::size_t end = textOffset;
                    matched = consumeLiteral(token.foldedLiteral, textOffset, end) &&
                        matchFrom(patternOffset + 1u, end);
                    break;
                }
                }
            }
            cached = matched ? 1 : 0;
            return matched;
        };

    return matchFrom(0, 0);
}

class DesktopEntryIterator final : public PkResourceStorage::EntryIterator
{
public:
    DesktopEntryIterator(const fs::path &root,
                         const std::vector<PkString> &nameFilters,
                         PkResourceStorage::EntryKind kind,
                         bool recursive)
        : m_filters(nameFilters)
        , m_kind(kind)
        , m_recursive(recursive)
    {
        std::error_code ec;
        if (recursive) {
            m_iterator.emplace<fs::recursive_directory_iterator>(
                root, fs::directory_options::skip_permission_denied, ec);
        } else {
            m_iterator.emplace<fs::directory_iterator>(
                root, fs::directory_options::skip_permission_denied, ec);
        }
        if (!ec) {
            findNext();
        }
    }

    bool hasNext() const override { return m_hasNext; }

    void next() override
    {
        if (!m_hasNext) {
            return;
        }
        m_currentPath = currentEntry().path();
        m_currentLastModified = lastModifiedMs(m_currentPath);
        increment();
        findNext();
    }

    PkString url() const override { return fromPath(m_currentPath); }
    int64_t lastModified() const override { return m_currentLastModified; }

private:
    using Iterator = std::variant<fs::directory_iterator, fs::recursive_directory_iterator>;

    const fs::directory_entry &currentEntry() const
    {
        return std::visit([](const auto &it) -> const fs::directory_entry & { return *it; }, m_iterator);
    }

    bool atEnd() const
    {
        return std::visit([](const auto &it) { return it == std::decay_t<decltype(it)>(); }, m_iterator);
    }

    void increment()
    {
        std::visit([](auto &it) {
            using Iter = std::decay_t<decltype(it)>;
            if constexpr (std::is_same_v<Iter, fs::recursive_directory_iterator>) {
                if (it != Iter() && isHidden(it->path())) {
                    it.disable_recursion_pending();
                }
            }
            std::error_code ec;
            it.increment(ec);
            if (ec) {
                it = std::decay_t<decltype(it)>();
            }
        }, m_iterator);
    }

    bool matches(const fs::directory_entry &entry) const
    {
        if (isHidden(entry.path()) || !hasEntryKind(entry, m_kind)) {
            return false;
        }
        if (!isReadable(entry.path(), m_kind)) {
            return false;
        }
        if (m_filters.empty()) {
            return true;
        }
        const PkString name = fromPath(entry.path().filename());
        for (const PkString &filter : m_filters) {
            if (globMatches(filter, name)) {
                return true;
            }
        }
        return false;
    }

    void findNext()
    {
        while (!atEnd() && !matches(currentEntry())) {
            increment();
        }
        m_hasNext = !atEnd();
    }

    std::vector<PkString> m_filters;
    PkResourceStorage::EntryKind m_kind;
    bool m_recursive;
    Iterator m_iterator;
    bool m_hasNext = false;
    fs::path m_currentPath;
    int64_t m_currentLastModified = 0;
};

} // namespace

std::unique_ptr<PkResourceStorage::EntryIterator>
PkResourceStorageDesktop::listEntries(const PkString &path,
                                      const std::vector<PkString> &nameFilters,
                                      EntryKind kind,
                                      bool recursive) const
{
    return std::unique_ptr<EntryIterator>(
        new DesktopEntryIterator(toPath(path), nameFilters, kind, recursive));
}

bool PkResourceStorageDesktop::exists(const PkString &path) const
{
    std::error_code ec;
    return fs::exists(toPath(path), ec) && !ec;
}

bool PkResourceStorageDesktop::mkpath(const PkString &path) const
{
    std::error_code ec;
    const fs::path nativePath = toPath(path);
    return fs::create_directories(nativePath, ec) || (!ec && fs::is_directory(nativePath, ec));
}

bool PkResourceStorageDesktop::remove(const PkString &path) const
{
    std::error_code ec;
    const fs::path nativePath = toPath(path);
    if (!fs::is_regular_file(nativePath, ec) || ec) {
        return false;
    }
    return fs::remove(nativePath, ec) && !ec;
}

PkString PkResourceStorageDesktop::absolutePath(const PkString &path) const
{
    std::error_code ec;
    const fs::path canonical = fs::canonical(toPath(path), ec);
    return ec ? PkString() : fromPath(canonical);
}

PkString PkResourceStorageDesktop::platformDir(PlatformDir kind) const
{
    const PkString home = environmentPath("HOME");
    const auto underHome = [&home](const char *suffix) {
        return home.isEmpty() ? PkString() : PkResourceStorage::joinPath(home, PkString(suffix));
    };

    switch (kind) {
    case PlatformDir::AppData: {
        const PkString xdg = environmentPath("XDG_DATA_HOME");
        return xdg.isEmpty() ? underHome(".local/share") : xdg;
    }
    case PlatformDir::AppLocalData:
        return underHome(".local/share");
    case PlatformDir::GenericData:
    {
        const PkString xdg = environmentPath("XDG_DATA_HOME");
        return xdg.isEmpty() ? underHome(".local/share") : xdg;
    }
    case PlatformDir::GenericConfig: {
        const PkString xdg = environmentPath("XDG_CONFIG_HOME");
        return xdg.isEmpty() ? underHome(".config") : xdg;
    }
    case PlatformDir::Cache: {
        const PkString xdg = environmentPath("XDG_CACHE_HOME");
        return xdg.isEmpty() ? underHome(".cache") : xdg;
    }
    case PlatformDir::Home:
        return home;
    case PlatformDir::Pictures:
        return underHome("Pictures");
    }
    return PkString();
}

int64_t PkResourceStorageDesktop::lastModified(const PkString &path) const
{
    return lastModifiedMs(toPath(path));
}
