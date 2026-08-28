/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef LCMS_PROFILE_DISCOVERY_H
#define LCMS_PROFILE_DISCOVERY_H

#include <PkStringList.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

namespace LcmsProfileDiscovery
{
namespace fs = std::filesystem;

inline PkString stringFromPath(const fs::path &path)
{
    const std::string utf8 = path.generic_u8string();
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

inline fs::path pathFromString(const PkString &path)
{
    return fs::u8path(path.PkToUtf8());
}

inline PkString environmentPath(const char *name)
{
#ifdef _WIN32
    std::wstring wideName;
    for (const unsigned char character : std::string(name)) {
        wideName.push_back(static_cast<wchar_t>(character));
    }
    const wchar_t *value = ::_wgetenv(wideName.c_str());
    return value && *value ? stringFromPath(fs::path(value)) : PkString();
#else
    const char *value = std::getenv(name);
    return value && *value ? PkString(value) : PkString();
#endif
}

inline PkString homePath()
{
#ifdef _WIN32
    PkString home = environmentPath("USERPROFILE");
    if (!home.isEmpty()) {
        return home;
    }

    const PkString drive = environmentPath("HOMEDRIVE");
    const PkString path = environmentPath("HOMEPATH");
    return path.isEmpty() ? PkString() : drive + path;
#else
    const PkString environmentHome = environmentPath("HOME");
    if (!environmentHome.isEmpty()) {
        fs::path home = pathFromString(environmentHome).lexically_normal();
        std::string cleaned = home.generic_string();
        while (cleaned.size() > 1 && cleaned.back() == '/') {
            cleaned.pop_back();
        }
        return PkString::PkFromUtf8(cleaned.data(), static_cast<int>(cleaned.size()));
    }
    return PkString("/");
#endif
}

inline bool isIccProfileFilename(const fs::path &path)
{
    const std::string extension = path.extension().generic_u8string();
    return extension == ".icm" || extension == ".icc"
        || extension == ".ICM" || extension == ".ICC";
}

inline bool isReadableRegularFile(const fs::path &path)
{
    std::error_code error;
    if (!fs::is_regular_file(path, error) || error) {
        return false;
    }
    std::ifstream stream(path, std::ios::binary);
    return stream.is_open();
}

inline bool isBlacklisted(const PkString &filename)
{
    const PkString lower = filename.toLower();
    return lower == PkString("panhexro.icm")
        || lower == PkString("ctpctdmed.icc");
}

inline PkStringList profileEntries(const PkString &directory)
{
    struct Entry {
        PkString name;
        PkString lowerName;
    };

    std::vector<Entry> entries;
    std::error_code error;
    for (fs::directory_iterator it(pathFromString(directory), error), end;
         !error && it != end;
         it.increment(error)) {
        const fs::path path = it->path();
        if (!isIccProfileFilename(path) || !isReadableRegularFile(path)) {
            continue;
        }
        const PkString name = stringFromPath(path.filename());
        if (!isBlacklisted(name)) {
            entries.push_back({name, name.toLower()});
        }
    }

    std::sort(entries.begin(), entries.end(), [](const Entry &lhs, const Entry &rhs) {
        if (lhs.lowerName != rhs.lowerName) {
            return lhs.lowerName < rhs.lowerName;
        }
        return lhs.name < rhs.name;
    });

    PkStringList result;
    for (const Entry &entry : entries) {
        result.append(entry.name);
    }
    return result;
}
} // namespace LcmsProfileDiscovery

#endif // LCMS_PROFILE_DISCOVERY_H
