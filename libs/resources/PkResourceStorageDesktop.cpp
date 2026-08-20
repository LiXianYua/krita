/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "PkResourceStorageDesktop.h"

#include <PkString.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fnmatch.h>
#include <system_error>
#include <variant>

namespace {

namespace fs = std::filesystem;

PkString fromPath(const fs::path &path)
{
    const std::string text = path.string();
    return PkString::PkFromUtf8(text.c_str(), static_cast<int>(text.size()));
}

fs::path toPath(const PkString &path)
{
    return fs::u8path(path.PkToUtf8());
}

PkString environmentPath(const char *name)
{
    const char *value = std::getenv(name);
    return value && *value ? PkString(value) : PkString();
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
        std::error_code ec;
        const fs::file_time_type writeTime = currentEntry().last_write_time(ec);
        if (ec) {
            m_currentLastModified = 0;
        } else {
            const auto systemTime = std::chrono::time_point_cast<std::chrono::milliseconds>(
                writeTime - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
            m_currentLastModified = systemTime.time_since_epoch().count();
        }
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
            std::error_code ec;
            it.increment(ec);
            if (ec) {
                it = std::decay_t<decltype(it)>();
            }
        }, m_iterator);
    }

    bool matches(const fs::directory_entry &entry) const
    {
        std::error_code ec;
        const bool rightKind = m_kind == PkResourceStorage::EntryKind::Files
            ? entry.is_regular_file(ec) : entry.is_directory(ec);
        if (ec || !rightKind) {
            return false;
        }
        if (m_filters.empty()) {
            return true;
        }
        const std::string name = entry.path().filename().string();
        for (const PkString &filter : m_filters) {
            if (::fnmatch(filter.PkToUtf8().c_str(), name.c_str(), 0) == 0) {
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
