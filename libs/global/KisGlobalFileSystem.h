/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KISGLOBALFILESYSTEM_H
#define KISGLOBALFILESYSTEM_H

#include <PkString.h>

#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>

/**
 * Small, Qt-free filesystem helpers used inside kritaglobal.
 *
 * This is deliberately not a Pk compatibility class.  It only centralizes
 * the UTF-8/PkString boundary and the handful of writable locations needed by
 * global's file-backed facilities.
 */
namespace KisGlobalFileSystem
{

namespace fs = std::filesystem;

enum class Location {
    AppData,
    GenericData,
    GenericConfig
};

inline fs::path toPath(const PkString &path)
{
    return fs::u8path(path.PkToUtf8());
}

inline PkString fromPath(const fs::path &path)
{
    const std::string utf8 = path.u8string();
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

namespace detail
{

inline fs::path environmentPath(const char *name)
{
#ifdef _WIN32
    std::wstring wideName;
    for (const unsigned char character : std::string(name)) {
        wideName.push_back(static_cast<wchar_t>(character));
    }
    const wchar_t *value = ::_wgetenv(wideName.c_str());
    return value && *value ? fs::path(value) : fs::path();
#else
    const char *value = std::getenv(name);
    return value && *value ? fs::u8path(value) : fs::path();
#endif
}

inline fs::path homePath()
{
#ifdef _WIN32
    fs::path home = environmentPath("USERPROFILE");
    if (home.empty()) {
        home = environmentPath("HOMEPATH");
    }
#else
    fs::path home = environmentPath("HOME");
#endif
    if (!home.empty()) {
        return home;
    }

    std::error_code error;
    const fs::path current = fs::current_path(error);
    return error ? fs::path() : current;
}

} // namespace detail

inline fs::path writableLocation(Location location)
{
    const fs::path home = detail::homePath();

#ifdef _WIN32
    fs::path roaming = detail::environmentPath("APPDATA");
    if (roaming.empty()) {
        roaming = home;
    }
    switch (location) {
    case Location::AppData:
        return roaming / "krita";
    case Location::GenericData:
    case Location::GenericConfig:
        return roaming;
    }
#elif defined(__APPLE__)
    const fs::path library = home / "Library";
    switch (location) {
    case Location::AppData:
        return library / "Application Support" / "krita";
    case Location::GenericData:
        return library / "Application Support";
    case Location::GenericConfig:
        return library / "Preferences";
    }
#elif defined(__ANDROID__)
    fs::path applicationHome = detail::environmentPath("ANDROID_APP_DATA");
    if (applicationHome.empty()) {
        applicationHome = detail::environmentPath("HOME");
    }
    if (applicationHome.empty()) {
        const fs::path temporary = detail::environmentPath("TMPDIR");
        if (!temporary.empty()) {
            applicationHome = temporary.parent_path() / "files";
        }
    }
    if (applicationHome.empty()) {
        applicationHome = home;
    }
    switch (location) {
    case Location::AppData:
    case Location::GenericData:
    case Location::GenericConfig:
        return applicationHome;
    }
#else
    switch (location) {
    case Location::AppData: {
        fs::path data = detail::environmentPath("XDG_DATA_HOME");
        return (data.empty() ? home / ".local" / "share" : data) / "krita";
    }
    case Location::GenericData: {
        const fs::path data = detail::environmentPath("XDG_DATA_HOME");
        return data.empty() ? home / ".local" / "share" : data;
    }
    case Location::GenericConfig: {
        const fs::path config = detail::environmentPath("XDG_CONFIG_HOME");
        return config.empty() ? home / ".config" : config;
    }
    }
#endif

    return home;
}

} // namespace KisGlobalFileSystem

#endif // KISGLOBALFILESYSTEM_H
