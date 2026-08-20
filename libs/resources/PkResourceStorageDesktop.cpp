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
#include <limits>
#include <system_error>
#include <type_traits>
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
    const char *value = std::getenv(name);
    return value && *value ? PkString(value) : PkString();
}

bool isHidden(const fs::path &path)
{
    const std::string name = path.filename().string();
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

bool isReadable(const fs::path &path)
{
#ifdef _WIN32
    const HANDLE handle = ::CreateFileW(path.c_str(), GENERIC_READ,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr, OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    ::CloseHandle(handle);
    return true;
#else
    int flags = O_RDONLY;
#ifdef O_CLOEXEC
    flags |= O_CLOEXEC;
#endif
    const int descriptor = ::open(path.c_str(), flags);
    if (descriptor < 0) {
        return false;
    }
    struct stat status {};
    const bool statusOk = ::fstat(descriptor, &status) == 0;
    ::close(descriptor);
    if (!statusOk) {
        return false;
    }
    if (!S_ISDIR(status.st_mode)) {
        return true;
    }

    // Opening "directory/." is a real read + traversal probe performed with
    // the process's effective credentials. Unlike access(), it never switches
    // to the real uid/gid, and it does not create or modify filesystem state.
    const int traversal = ::open((path / ".").c_str(), flags);
    if (traversal < 0) {
        return false;
    }
    ::close(traversal);
    return true;
#endif
}

bool caseFold(const PkString &value, std::vector<UChar32> &result)
{
    const std::u16string utf16 = value.PkToU16();
    std::vector<UChar> source;
    source.reserve(utf16.size());
    for (char16_t unit : utf16) {
        source.push_back(static_cast<UChar>(unit));
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

bool globMatches(const PkString &patternValue, const PkString &textValue)
{
    std::vector<UChar32> pattern;
    std::vector<UChar32> text;
    if (!caseFold(patternValue, pattern) || !caseFold(textValue, text)) {
        return false;
    }

    constexpr std::size_t noStar = std::numeric_limits<std::size_t>::max();
    std::size_t patternOffset = 0;
    std::size_t textOffset = 0;
    std::size_t star = noStar;
    std::size_t retry = 0;
    while (textOffset < text.size()) {
        if (patternOffset < pattern.size() &&
            (pattern[patternOffset] == '?' || pattern[patternOffset] == text[textOffset])) {
            ++patternOffset;
            ++textOffset;
        } else if (patternOffset < pattern.size() && pattern[patternOffset] == '*') {
            star = patternOffset++;
            retry = textOffset;
        } else if (star != noStar) {
            patternOffset = star + 1;
            textOffset = ++retry;
        } else {
            return false;
        }
    }
    while (patternOffset < pattern.size() && pattern[patternOffset] == '*') {
        ++patternOffset;
    }
    return patternOffset == pattern.size();
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
        if (isHidden(entry.path()) || !isReadable(entry.path())) {
            return false;
        }
        std::error_code ec;
        const bool rightKind = m_kind == PkResourceStorage::EntryKind::Files
            ? entry.is_regular_file(ec) : entry.is_directory(ec);
        if (ec || !rightKind) {
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
