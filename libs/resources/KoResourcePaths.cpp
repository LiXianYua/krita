/*
 * SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include "KoResourcePaths.h"

#include "ResourceDebug.h"

#include <PkConfigGroup.h>
#include <PkMap.h>
#include <PkMutex.h>
#include <PkResourceStorage.h>
#include <PkSharedConfig.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

#ifdef __HAIKU__
#include <FindDirectory.h>
#include <StorageDefs.h>
#endif

namespace fs = std::filesystem;

PkString KoResourcePaths::s_overrideAppDataLocation;

namespace {

const PkString kResourceLocationKey("ResourceDirectory");

PkString fromPath(const fs::path &path)
{
    const std::string text = path.generic_string();
    return PkString::PkFromUtf8(text.data(), static_cast<int>(text.size()));
}

fs::path toPath(const PkString &path)
{
    return fs::u8path(path.PkToUtf8());
}

PkString environment(const char *name)
{
    const char *value = std::getenv(name);
    return value ? PkString(value) : PkString();
}

PkString homePath()
{
#ifdef _WIN32
    PkString value = environment("USERPROFILE");
    if (value.isEmpty()) {
        value = environment("HOMEPATH");
    }
#else
    PkString value = environment("HOME");
#endif
    if (!value.isEmpty()) {
        return value;
    }
    std::error_code ec;
    return fromPath(fs::current_path(ec));
}

#ifdef __HAIKU__
PkString haikuDirectory(directory_which which, const PkString &fallback)
{
    char path[B_PATH_NAME_LENGTH] = {};
    if (find_directory(which, -1, true, path, sizeof(path)) == B_OK) {
        return PkString(path);
    }
    return fallback;
}
#endif

PkString platformDir(PkResourceStorage::PlatformDir kind)
{
    const PkString home = homePath();
#ifdef _WIN32
    switch (kind) {
    case PkResourceStorage::PlatformDir::AppData:
    case PkResourceStorage::PlatformDir::AppLocalData: {
        const PkString appData = environment("APPDATA");
        return PkResourceStorage::joinPath(appData.isEmpty() ? home : appData, PkString("krita"));
    }
    case PkResourceStorage::PlatformDir::GenericData:
    case PkResourceStorage::PlatformDir::GenericConfig: {
        const PkString appData = environment("APPDATA");
        return appData.isEmpty() ? home : appData;
    }
    case PkResourceStorage::PlatformDir::Cache: {
        const PkString local = environment("LOCALAPPDATA");
        return PkResourceStorage::joinPath(local.isEmpty() ? home : local, PkString("krita/cache"));
    }
    case PkResourceStorage::PlatformDir::Home:
        return home;
    case PkResourceStorage::PlatformDir::Pictures:
        return PkResourceStorage::joinPath(home, PkString("Pictures"));
    }
#elif defined(__APPLE__)
    const PkString library = PkResourceStorage::joinPath(home, PkString("Library"));
    switch (kind) {
    case PkResourceStorage::PlatformDir::AppData:
    case PkResourceStorage::PlatformDir::AppLocalData:
        return PkResourceStorage::joinPath(
            PkResourceStorage::joinPath(library, PkString("Application Support")), PkString("krita"));
    case PkResourceStorage::PlatformDir::GenericData:
        return PkResourceStorage::joinPath(library, PkString("Application Support"));
    case PkResourceStorage::PlatformDir::GenericConfig:
        return PkResourceStorage::joinPath(library, PkString("Preferences"));
    case PkResourceStorage::PlatformDir::Cache:
        return PkResourceStorage::joinPath(
            PkResourceStorage::joinPath(library, PkString("Caches")), PkString("krita"));
    case PkResourceStorage::PlatformDir::Home:
        return home;
    case PkResourceStorage::PlatformDir::Pictures:
        return PkResourceStorage::joinPath(home, PkString("Pictures"));
    }
#elif defined(__ANDROID__)
    PkString applicationHome = environment("ANDROID_APP_DATA");
    if (applicationHome.isEmpty()) {
        applicationHome = environment("HOME");
    }
    if (applicationHome.isEmpty()) {
        const PkString temporary = environment("TMPDIR");
        if (!temporary.isEmpty()) {
            applicationHome = fromPath(toPath(temporary).parent_path() / "files");
        }
    }
    if (applicationHome.isEmpty()) {
        applicationHome = home;
    }
    switch (kind) {
    case PkResourceStorage::PlatformDir::AppData:
    case PkResourceStorage::PlatformDir::AppLocalData:
    case PkResourceStorage::PlatformDir::GenericData:
    case PkResourceStorage::PlatformDir::GenericConfig:
        return applicationHome;
    case PkResourceStorage::PlatformDir::Cache: {
        const PkString temporary = environment("TMPDIR");
        return temporary.isEmpty() ? PkResourceStorage::joinPath(home, PkString("cache")) : temporary;
    }
    case PkResourceStorage::PlatformDir::Home:
        return home;
    case PkResourceStorage::PlatformDir::Pictures: {
        const PkString external = environment("EXTERNAL_STORAGE");
        return PkResourceStorage::joinPath(external.isEmpty() ? home : external, PkString("Pictures"));
    }
    }
#elif defined(__HAIKU__)
    switch (kind) {
    case PkResourceStorage::PlatformDir::AppData:
    case PkResourceStorage::PlatformDir::AppLocalData:
        return PkResourceStorage::joinPath(
            haikuDirectory(B_USER_NONPACKAGED_DATA_DIRECTORY, home), PkString("krita"));
    case PkResourceStorage::PlatformDir::GenericData:
        return haikuDirectory(B_USER_NONPACKAGED_DATA_DIRECTORY, home);
    case PkResourceStorage::PlatformDir::GenericConfig:
        return haikuDirectory(B_USER_SETTINGS_DIRECTORY, home);
    case PkResourceStorage::PlatformDir::Cache:
        return PkResourceStorage::joinPath(
            haikuDirectory(B_USER_CACHE_DIRECTORY, home), PkString("krita"));
    case PkResourceStorage::PlatformDir::Home:
        return haikuDirectory(B_USER_DIRECTORY, home);
    case PkResourceStorage::PlatformDir::Pictures:
        return PkResourceStorage::joinPath(
            haikuDirectory(B_USER_DIRECTORY, home), PkString("Pictures"));
    }
#else
    switch (kind) {
    case PkResourceStorage::PlatformDir::AppData:
    case PkResourceStorage::PlatformDir::AppLocalData: {
        const PkString base = environment("XDG_DATA_HOME");
        return PkResourceStorage::joinPath(base.isEmpty()
                                               ? PkResourceStorage::joinPath(home, PkString(".local/share"))
                                               : base,
                                           PkString("krita"));
    }
    case PkResourceStorage::PlatformDir::GenericData: {
        const PkString base = environment("XDG_DATA_HOME");
        return base.isEmpty() ? PkResourceStorage::joinPath(home, PkString(".local/share")) : base;
    }
    case PkResourceStorage::PlatformDir::GenericConfig: {
        const PkString base = environment("XDG_CONFIG_HOME");
        return base.isEmpty() ? PkResourceStorage::joinPath(home, PkString(".config")) : base;
    }
    case PkResourceStorage::PlatformDir::Cache: {
        const PkString base = environment("XDG_CACHE_HOME");
        return PkResourceStorage::joinPath(base.isEmpty()
                                               ? PkResourceStorage::joinPath(home, PkString(".cache"))
                                               : base,
                                           PkString("krita"));
    }
    case PkResourceStorage::PlatformDir::Home:
        return home;
    case PkResourceStorage::PlatformDir::Pictures:
        return PkResourceStorage::joinPath(home, PkString("Pictures"));
    }
#endif
    return home;
}

fs::path resourceConfigFilePath()
{
    return toPath(PkResourceStorage::joinPath(
        platformDir(PkResourceStorage::PlatformDir::GenericConfig), PkString("kritarc")));
}

bool readPersistentResourceLocation(PkString *location)
{
    std::ifstream input(resourceConfigFilePath(), std::ios::binary);
    if (!input) {
        return false;
    }

    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty() && line.front() == '[') {
            break;
        }
        static const std::string prefix = "ResourceDirectory=";
        if (line.compare(0, prefix.size(), prefix) == 0) {
            *location = PkString::PkFromUtf8(line.data() + prefix.size(),
                                             static_cast<int>(line.size() - prefix.size()));
            return true;
        }
    }
    return false;
}

bool persistResourceLocation(const PkString &location)
{
    const fs::path configPath = resourceConfigFilePath();
    std::error_code ec;
    fs::create_directories(configPath.parent_path(), ec);
    if (ec) {
        return false;
    }

    std::vector<std::string> lines;
    {
        std::ifstream input(configPath, std::ios::binary);
        std::string line;
        while (std::getline(input, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            lines.push_back(line);
        }
    }

    const std::string entry = std::string("ResourceDirectory=") + location.PkToUtf8();
    bool replaced = false;
    std::size_t firstGroup = lines.size();
    for (std::size_t i = 0; i < lines.size(); ++i) {
        if (!lines[i].empty() && lines[i].front() == '[') {
            firstGroup = i;
            break;
        }
        if (lines[i].compare(0, std::string("ResourceDirectory=").size(), "ResourceDirectory=") == 0) {
            lines[i] = entry;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(firstGroup), entry);
    }

    fs::path temporaryPath = configPath;
    temporaryPath += ".tmp";
    {
        std::ofstream output(temporaryPath, std::ios::binary | std::ios::trunc);
        if (!output) {
            return false;
        }
        for (const std::string &line : lines) {
            output << line << '\n';
        }
        output.flush();
        if (!output) {
            return false;
        }
    }
    fs::rename(temporaryPath, configPath, ec);
#ifdef _WIN32
    if (ec) {
        ec.clear();
        fs::remove(configPath, ec);
        ec.clear();
        fs::rename(temporaryPath, configPath, ec);
    }
#endif
    if (ec) {
        fs::remove(temporaryPath, ec);
        return false;
    }
    return true;
}

PkString configuredResourceLocation(PkConfigGroup &config)
{
    if (config.hasKey(kResourceLocationKey)) {
        const PkString location = config.readEntry(kResourceLocationKey, PkString());
        persistResourceLocation(location);
        return location;
    }

    PkString location;
    if (readPersistentResourceLocation(&location)) {
        config.writeEntry(kResourceLocationKey, location);
    }
    return location;
}

void updateConfiguredResourceLocation(PkConfigGroup &config, const PkString &location)
{
    config.writeEntry(kResourceLocationKey, location);
    persistResourceLocation(location);
}

PkStringList splitEnvironmentPaths(const char *name)
{
    PkStringList result;
    const PkString raw = environment(name);
    if (raw.isEmpty()) {
        return result;
    }
#ifdef _WIN32
    const char16_t separator = u';';
#else
    const char16_t separator = u':';
#endif
    for (const PkString &part : raw.split(separator)) {
        if (!part.isEmpty()) {
            result.append(part);
        }
    }
    return result;
}

PkStringList platformSearchDirs(PkResourceStorage::PlatformDir kind)
{
    PkStringList result;
    result.append(platformDir(kind));
#if defined(__APPLE__)
    if (kind == PkResourceStorage::PlatformDir::AppData ||
        kind == PkResourceStorage::PlatformDir::AppLocalData) {
        result.append(PkString("/Library/Application Support/krita"));
        result.append(PkString("/Network/Library/Application Support/krita"));
    } else if (kind == PkResourceStorage::PlatformDir::GenericData) {
        result.append(PkString("/Library/Application Support"));
        result.append(PkString("/Network/Library/Application Support"));
    }
#elif defined(__HAIKU__)
    if (kind == PkResourceStorage::PlatformDir::AppData ||
        kind == PkResourceStorage::PlatformDir::AppLocalData) {
        result.append(PkResourceStorage::joinPath(
            haikuDirectory(B_SYSTEM_NONPACKAGED_DATA_DIRECTORY, PkString()), PkString("krita")));
    } else if (kind == PkResourceStorage::PlatformDir::GenericData) {
        result.append(haikuDirectory(B_SYSTEM_NONPACKAGED_DATA_DIRECTORY, PkString()));
    }
#elif !defined(_WIN32) && !defined(__ANDROID__)
    if (kind == PkResourceStorage::PlatformDir::AppData ||
        kind == PkResourceStorage::PlatformDir::AppLocalData) {
        PkStringList system = splitEnvironmentPaths("XDG_DATA_DIRS");
        if (system.isEmpty()) {
            system = PkStringList{PkString("/usr/local/share"), PkString("/usr/share")};
        }
        for (const PkString &dir : system) {
            result.append(PkResourceStorage::joinPath(dir, PkString("krita")));
        }
    } else if (kind == PkResourceStorage::PlatformDir::GenericData) {
        PkStringList system = splitEnvironmentPaths("XDG_DATA_DIRS");
        if (system.isEmpty()) {
            system = PkStringList{PkString("/usr/local/share"), PkString("/usr/share")};
        }
        result += system;
    }
#endif
    result.removeDuplicates();
    return result;
}

PkString clean(const PkString &path)
{
    return path.isEmpty() ? path : PkResourceStorage::cleanPath(path);
}

bool hasTrailingSlash(const PkString &path)
{
    return !path.isEmpty() && path.at(path.size() - 1) == u'/';
}

PkString withTrailingSlash(const PkString &path)
{
    const PkString cleaned = clean(path);
    if (cleaned.isEmpty()) {
        return cleaned;
    }
    return hasTrailingSlash(cleaned) ? cleaned : cleaned + PkString("/");
}

PkString normalizedRelativePath(const PkString &path)
{
    const std::string text = path.PkToUtf8();
    const std::size_t firstRelativeCharacter = text.find_first_not_of("/\\");
    if (firstRelativeCharacter == std::string::npos) {
        return PkString();
    }
    return clean(PkString(text.substr(firstRelativeCharacter).c_str()));
}

PkString normalizedResourceAlias(const PkString &alias)
{
    return withTrailingSlash(normalizedRelativePath(alias));
}

bool startsWithPath(const PkString &path, const PkString &prefix)
{
    return withTrailingSlash(clean(path)).startsWith(withTrailingSlash(clean(prefix)));
}

PkStringList cleanedPaths(const PkStringList &paths, bool addTrailingSlash)
{
    PkStringList result;
    const PkString defaultLocation = platformDir(PkResourceStorage::PlatformDir::AppData);
    const bool removeDefaultLocation = KoResourcePaths::getAppDataLocation() != defaultLocation;
    for (const PkString &path : paths) {
        PkString cleaned = clean(path);
        if (removeDefaultLocation && startsWithPath(cleaned, defaultLocation)) {
            continue;
        }
        if (addTrailingSlash) {
            cleaned = withTrailingSlash(cleaned);
        }
        if (!cleaned.isEmpty()) {
            result.append(cleaned);
        }
    }
    return result;
}

bool pathExists(const PkString &path)
{
    std::error_code ec;
    return !path.isEmpty() && fs::exists(toPath(path), ec);
}

bool directoryExists(const PkString &path)
{
    std::error_code ec;
    return !path.isEmpty() && fs::is_directory(toPath(path), ec);
}

bool isAbsolute(const PkString &path)
{
    return toPath(path).is_absolute();
}

bool isWritable(const fs::path &path)
{
    std::error_code ec;
    const fs::perms permissions = fs::status(path, ec).permissions();
    if (ec) {
        return false;
    }
    const fs::perms writable = fs::perms::owner_write | fs::perms::group_write | fs::perms::others_write;
    return (permissions & writable) != fs::perms::none;
}

PkString absolutePath(const PkString &path)
{
    std::error_code ec;
    const fs::path absolute = fs::absolute(toPath(path), ec);
    return ec ? path : fromPath(absolute.lexically_normal());
}

bool equalForPlatform(const PkString &lhs, const PkString &rhs)
{
#ifdef _WIN32
    const std::string a = lhs.PkToUtf8();
    const std::string b = rhs.PkToUtf8();
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
#else
    return lhs == rhs;
#endif
}

bool containsForPlatform(const PkStringList &list, const PkString &value)
{
    for (const PkString &entry : list) {
        if (equalForPlatform(entry, value)) {
            return true;
        }
    }
    return false;
}

void appendResources(PkStringList *destination, const PkStringList &source, bool eliminateDuplicates)
{
    for (const PkString &resource : source) {
        const PkString realPath = clean(resource);
        if (!realPath.isEmpty() && (!eliminateDuplicates || !destination->contains(realPath))) {
            destination->append(realPath);
        }
    }
}

PkString installationPrefix()
{
    const PkString testPrefix = environment("KIS_TEST_PREFIX_PATH");
    if (!testPrefix.isEmpty()) {
        return withTrailingSlash(testPrefix);
    }
#ifdef __APPLE__
    std::vector<char> executableBuffer(1024);
    uint32_t executableBufferSize = static_cast<uint32_t>(executableBuffer.size());
    if (_NSGetExecutablePath(executableBuffer.data(), &executableBufferSize) != 0) {
        executableBuffer.resize(executableBufferSize);
        _NSGetExecutablePath(executableBuffer.data(), &executableBufferSize);
    }
    const fs::path executable = fs::weakly_canonical(fs::path(executableBuffer.data()));
    const fs::path executableDir = executable.parent_path();
    if (executableDir.filename() == "MacOS" && executableDir.parent_path().filename() == "Contents") {
        const fs::path contents = executableDir.parent_path();
        if (fs::exists(contents / "Resources" / "kritaplugins")) {
            return withTrailingSlash(fromPath(contents));
        }
    }
    return withTrailingSlash(fromPath(executableDir.parent_path()));
#elif defined(__ANDROID__)
    return withTrailingSlash(platformDir(PkResourceStorage::PlatformDir::AppData));
#else
#ifdef __linux__
    std::error_code linkError;
    const fs::path executable = fs::read_symlink("/proc/self/exe", linkError);
    if (!linkError) {
        return withTrailingSlash(fromPath(executable.parent_path().parent_path()));
    }
#endif
    std::error_code currentPathError;
    return withTrailingSlash(fromPath(fs::current_path(currentPathError)));
#endif
}

bool globMatches(const std::string &pattern, const std::string &text)
{
    std::size_t p = 0;
    std::size_t t = 0;
    std::size_t star = std::string::npos;
    std::size_t retry = 0;
    while (t < text.size()) {
        if (p < pattern.size() && (pattern[p] == '?' || pattern[p] == text[t])) {
            ++p;
            ++t;
        } else if (p < pattern.size() && pattern[p] == '*') {
            star = p++;
            retry = t;
        } else if (star != std::string::npos) {
            p = star + 1;
            t = ++retry;
        } else {
            return false;
        }
    }
    while (p < pattern.size() && pattern[p] == '*') {
        ++p;
    }
    return p == pattern.size();
}

PkStringList filesInDir(const PkString &startDir, const PkString &filter, bool recursive)
{
    PkStringList result;
    std::error_code ec;
    const fs::path root = toPath(startDir);
    if (!fs::is_directory(root, ec)) {
        return result;
    }
    const std::string pattern = filter.isEmpty() ? "*" : filter.PkToUtf8();
    std::vector<fs::path> files;
    std::vector<fs::path> directories;
    for (fs::directory_iterator it(root, fs::directory_options::skip_permission_denied, ec), end;
         it != end; it.increment(ec)) {
        if (!ec) {
            if (it->is_regular_file(ec) && globMatches(pattern, it->path().filename().string())) {
                files.push_back(it->path());
            } else if (recursive && it->is_directory(ec)) {
                directories.push_back(it->path());
            }
        }
        ec.clear();
    }
    std::sort(files.begin(), files.end());
    for (const fs::path &path : files) {
        result.append(fromPath(path.lexically_normal()));
    }
    std::sort(directories.begin(), directories.end());
    for (const fs::path &directory : directories) {
        result += filesInDir(fromPath(directory), filter, true);
    }
    return result;
}

KoResourcePaths &instance()
{
    static KoResourcePaths value;
    return value;
}

} // namespace

class KoResourcePaths::Private
{
public:
    PkMap<PkString, PkStringList> absolutes;
    PkMap<PkString, PkStringList> relatives;
    PkMutex relativesMutex;
    PkMutex absolutesMutex;

    PkStringList aliases(const PkString &type)
    {
        PkStringList result;
        relativesMutex.lock();
        result += relatives.value(type);
        relativesMutex.unlock();
        absolutesMutex.lock();
        result += absolutes.value(type);
        absolutesMutex.unlock();
        return result;
    }

    PkResourceStorage::PlatformDir mapTypeToPlatformDir(const PkString &type) const
    {
        if (type == PkString("cache")) {
            return PkResourceStorage::PlatformDir::Cache;
        }
        if (type == PkString("genericdata")) {
            return PkResourceStorage::PlatformDir::GenericData;
        }
        return PkResourceStorage::PlatformDir::AppData;
    }
};

KoResourcePaths::KoResourcePaths()
    : d(new Private)
{
}

KoResourcePaths::~KoResourcePaths() = default;

PkString KoResourcePaths::getApplicationRoot()
{
    return installationPrefix();
}

PkString KoResourcePaths::getAppDataLocation()
{
    if (!s_overrideAppDataLocation.isEmpty()) {
        return clean(s_overrideAppDataLocation);
    }

    const PkString defaultPath = platformDir(PkResourceStorage::PlatformDir::AppData);
    PkConfigGroup config(PkSharedConfig::openConfig(), PkString());
    PkString path = configuredResourceLocation(config);
    if (path.isEmpty()) {
        path = defaultPath;
    }

#ifndef _WIN32
    const std::string text = path.PkToUtf8();
    const bool looksLikeWindowsPath = text.size() >= 3 &&
                                      std::isalpha(static_cast<unsigned char>(text[0])) &&
                                      text[1] == ':' && text[2] == '/';
    if (looksLikeWindowsPath) {
        warnResource << "Resource path uses a Windows prefix; resetting" << path;
        path = defaultPath;
        updateConfiguredResourceLocation(config, path);
    }
#else
    const std::string text = path.PkToUtf8();
    if (text.size() >= 2 && text[0] == '/' && text[1] != '/') {
        warnResource << "Resource path uses a Unix prefix; resetting" << path;
        path = defaultPath;
        updateConfiguredResourceLocation(config, path);
    }
#endif

    if (!isAbsolute(path)) {
        path = absolutePath(path);
        updateConfiguredResourceLocation(config, path);
    }

    const fs::path nativePath = toPath(path);
    std::error_code ec;
    if (fs::exists(nativePath, ec)) {
        if (!isWritable(nativePath)) {
            path = defaultPath;
        }
    } else {
        ec.clear();
        const bool created = fs::create_directories(nativePath, ec);
        if (ec) {
            path = defaultPath;
        } else if (created) {
            fs::remove(nativePath, ec);
        }
    }
    return clean(path);
}

void KoResourcePaths::getAllUserResourceFoldersLocationsForWindowsStore(PkString &standardLocation,
                                                                         PkString &privateLocation)
{
    standardLocation = getAppDataLocation();
    privateLocation = PkString();
}

void KoResourcePaths::addAssetType(const PkString &type, const char *baseType,
                                   const PkString &relativeName, bool priority)
{
    instance().addResourceTypeInternal(type, PkString(baseType ? baseType : ""), relativeName, priority);
}

void KoResourcePaths::addAssetDir(const PkString &type, const PkString &dir, bool priority)
{
    instance().addResourceDirInternal(type, dir, priority);
}

PkString KoResourcePaths::findAsset(const PkString &type, const PkString &fileName)
{
    return clean(instance().findResourceInternal(type, fileName));
}

PkStringList KoResourcePaths::findDirs(const PkString &type)
{
    return cleanedPaths(instance().findDirsInternal(type), true);
}

PkStringList KoResourcePaths::findAllAssets(const PkString &type, const PkString &filter,
                                            SearchOptions options)
{
    return cleanedPaths(instance().findAllResourcesInternal(type, filter, options), false);
}

PkStringList KoResourcePaths::assetDirs(const PkString &type)
{
    return cleanedPaths(instance().resourceDirsInternal(type), true);
}

PkString KoResourcePaths::saveLocation(const PkString &type, const PkString &suffix, bool create)
{
    return withTrailingSlash(instance().saveLocationInternal(type, suffix, create));
}

PkString KoResourcePaths::locate(const PkString &type, const PkString &filename)
{
    return clean(instance().locateInternal(type, filename));
}

PkString KoResourcePaths::locateLocal(const PkString &type, const PkString &filename, bool createDir)
{
    return clean(instance().locateLocalInternal(type, filename, createDir));
}

void KoResourcePaths::addResourceTypeInternal(const PkString &type, const PkString &baseType,
                                              const PkString &relativeName, bool priority)
{
    if (relativeName.isEmpty()) {
        return;
    }
    assert(baseType == PkString("data"));
    const PkString copy = normalizedResourceAlias(relativeName);
    if (copy.isEmpty()) {
        return;
    }
    d->relativesMutex.lock();
    PkStringList &paths = d->relatives[type];
    if (!containsForPlatform(paths, copy)) {
        priority ? paths.prepend(copy) : paths.append(copy);
    }
    d->relativesMutex.unlock();
}

void KoResourcePaths::addResourceDirInternal(const PkString &type, const PkString &absoluteDir,
                                             bool priority)
{
    if (absoluteDir.isEmpty() || type.isEmpty()) {
        return;
    }
    const PkString copy = withTrailingSlash(absoluteDir);
    d->absolutesMutex.lock();
    PkStringList &paths = d->absolutes[type];
    if (!containsForPlatform(paths, copy)) {
        priority ? paths.prepend(copy) : paths.append(copy);
    }
    d->absolutesMutex.unlock();
}

PkString KoResourcePaths::findResourceInternal(const PkString &type, const PkString &fileName)
{
    const PkString relativeFileName = normalizedRelativePath(fileName);
    const PkStringList aliases = d->aliases(type);
    const PkStringList roots = platformSearchDirs(d->mapTypeToPlatformDir(type));
    for (const PkString &root : platformSearchDirs(PkResourceStorage::PlatformDir::AppData)) {
        const PkString direct = PkResourceStorage::joinPath(root, relativeFileName);
        if (pathExists(direct)) {
            return direct;
        }
    }
    for (const PkString &alias : aliases) {
        if (isAbsolute(alias)) {
            const PkString candidate = PkResourceStorage::joinPath(alias, relativeFileName);
            if (pathExists(candidate)) {
                return candidate;
            }
            continue;
        }
        for (const PkString &root : roots) {
            const PkString candidate = PkResourceStorage::joinPath(
                PkResourceStorage::joinPath(root, alias), relativeFileName);
            if (pathExists(candidate)) {
                return candidate;
            }
        }
    }

    const PkString prefix = installationPrefix();
    for (const PkString &alias : aliases) {
        if (isAbsolute(alias)) {
            continue;
        }
        const PkString share = PkResourceStorage::joinPath(
            PkResourceStorage::joinPath(PkResourceStorage::joinPath(prefix, PkString("share")), alias), relativeFileName);
        if (pathExists(share)) {
            return share;
        }
    }
    for (const PkString &alias : aliases) {
        if (isAbsolute(alias)) {
            continue;
        }
        const PkString kritaShare = PkResourceStorage::joinPath(
            PkResourceStorage::joinPath(
                PkResourceStorage::joinPath(prefix, PkString("share/krita")), alias), relativeFileName);
        if (pathExists(kritaShare)) {
            return kritaShare;
        }
    }

    for (const PkString &extra : findExtraResourceDirs()) {
        if (aliases.isEmpty()) {
            const PkString candidate = PkResourceStorage::joinPath(extra, relativeFileName);
            if (pathExists(candidate)) {
                return candidate;
            }
        }
        for (const PkString &alias : aliases) {
            const PkString candidate = PkResourceStorage::joinPath(
                PkResourceStorage::joinPath(extra, alias), relativeFileName);
            if (pathExists(candidate)) {
                return candidate;
            }
        }
    }
    return PkString();
}

PkStringList KoResourcePaths::findDirsInternal(const PkString &type)
{
    PkStringList dirs;
    const PkStringList roots = platformSearchDirs(d->mapTypeToPlatformDir(type));
    for (const PkString &root : roots) {
        if (directoryExists(root)) {
            appendResources(&dirs, PkStringList{root}, true);
        }
    }
    const PkString prefix = installationPrefix();
    for (const PkString &alias : d->aliases(type)) {
        for (const PkString &root : roots) {
            const PkString candidate = isAbsolute(alias) ? alias : PkResourceStorage::joinPath(root, alias);
            if (directoryExists(candidate)) {
                appendResources(&dirs, PkStringList{candidate}, true);
            }
        }
        appendResources(&dirs,
                        PkStringList{
                            PkResourceStorage::joinPath(
                                PkResourceStorage::joinPath(prefix, PkString("share")), alias),
                            PkResourceStorage::joinPath(
                                PkResourceStorage::joinPath(prefix, PkString("share/krita")), alias)},
                        true);
    }
    appendResources(&dirs, PkStringList{saveLocationInternal(type, PkString(), true)}, true);
    return dirs;
}

PkStringList KoResourcePaths::findAllResourcesInternal(const PkString &type,
                                                       const PkString &inputFilter,
                                                       SearchOptions options) const
{
    const bool recursive = options.testFlag(Recursive);
    PkStringList aliases = d->aliases(type);
    std::string filterText = inputFilter.isEmpty() ? "*" : inputFilter.PkToUtf8();
    const std::size_t star = filterText.find('*');
    if (star != std::string::npos && star > 0) {
        std::string directoryPart = filterText.substr(0, star);
        while (!directoryPart.empty() && directoryPart.back() == '/') {
            directoryPart.pop_back();
        }
        if (!directoryPart.empty()) {
            aliases.append(PkString(directoryPart.c_str()));
        }
        filterText = filterText.substr(star);
    }
    const PkString filter(filterText.c_str());

    const PkStringList roots = platformSearchDirs(d->mapTypeToPlatformDir(type));
    PkStringList resources;
    if (aliases.isEmpty()) {
        for (const PkString &root : roots) {
            appendResources(&resources, filesInDir(root, filter, false), true);
        }
    }

    for (const PkString &extra : findExtraResourceDirs()) {
        if (aliases.isEmpty()) {
            appendResources(&resources,
                            filesInDir(PkResourceStorage::joinPath(extra, type), filter, recursive),
                            true);
        } else {
            for (const PkString &alias : aliases) {
                if (!isAbsolute(alias)) {
                    appendResources(&resources,
                                    filesInDir(PkResourceStorage::joinPath(extra, alias), filter, recursive),
                                    true);
                }
            }
        }
    }

    const PkString prefix = installationPrefix();
    for (const PkString &alias : aliases) {
        PkStringList directories;
        if (isAbsolute(alias) && directoryExists(alias)) {
            directories.append(alias);
        } else {
            for (const PkString &root : roots) {
                const PkString candidate = PkResourceStorage::joinPath(root, alias);
                if (directoryExists(candidate)) {
                    directories.append(candidate);
                }
            }
            directories.append(PkResourceStorage::joinPath(
                PkResourceStorage::joinPath(prefix, PkString("share")), alias));
            directories.append(PkResourceStorage::joinPath(
                PkResourceStorage::joinPath(prefix, PkString("share/krita")), alias));
        }
        for (const PkString &directory : directories) {
            appendResources(&resources, filesInDir(directory, filter, recursive), true);
        }
    }

    if (!inputFilter.isEmpty()) {
        const fs::path inputPath = toPath(inputFilter);
        const PkString relativeDirectory = fromPath(inputPath.parent_path());
        const PkString fileFilter = fromPath(inputPath.filename());
        appendResources(&resources,
                        filesInDir(PkResourceStorage::joinPath(
                                       PkResourceStorage::joinPath(prefix, PkString("share")),
                                       relativeDirectory),
                                   fileFilter, false),
                        true);
        appendResources(&resources,
                        filesInDir(PkResourceStorage::joinPath(
                                       PkResourceStorage::joinPath(prefix, PkString("share/krita")),
                                       relativeDirectory),
                                   fileFilter, false),
                        true);
    }
    return resources;
}

PkStringList KoResourcePaths::resourceDirsInternal(const PkString &type)
{
    PkStringList result;
    const PkStringList roots = platformSearchDirs(d->mapTypeToPlatformDir(type));
    for (const PkString &alias : d->aliases(type)) {
        if (isAbsolute(alias)) {
            appendResources(&result, PkStringList{alias}, true);
            continue;
        }
        for (const PkString &root : roots) {
            const PkString candidate = PkResourceStorage::joinPath(root, alias);
            if (directoryExists(candidate)) {
                appendResources(&result, PkStringList{candidate}, true);
            }
        }
        appendResources(&result,
                        PkStringList{
                            PkResourceStorage::joinPath(
                                PkResourceStorage::joinPath(installationPrefix(), PkString("share")), alias),
                            PkResourceStorage::joinPath(
                                PkResourceStorage::joinPath(installationPrefix(), PkString("share/krita")), alias)},
                        true);
    }
    return result;
}

PkString KoResourcePaths::saveLocationInternal(const PkString &type, const PkString &suffix, bool create)
{
    const PkResourceStorage::PlatformDir location = d->mapTypeToPlatformDir(type);
    PkString path;
    bool usesStandardLocation = false;
    if (location == PkResourceStorage::PlatformDir::AppData) {
        PkConfigGroup config(PkSharedConfig::openConfig(), PkString());
        path = configuredResourceLocation(config);
    }
    if (path.isEmpty()) {
        path = platformDir(location);
        usesStandardLocation = true;
    }
#ifndef __ANDROID__
    if (usesStandardLocation && toPath(path).filename() != fs::path("krita")) {
        path = PkResourceStorage::joinPath(path, PkString("krita"));
    }
#endif

    const PkStringList aliases = d->aliases(type);
    if (!aliases.isEmpty()) {
        path = PkResourceStorage::joinPath(path, aliases.first());
    } else if (!suffix.isEmpty()) {
        path = PkResourceStorage::joinPath(path, normalizedRelativePath(suffix));
    }

    if (create) {
        std::error_code ec;
        fs::create_directories(toPath(path), ec);
        if (ec) {
            warnResource << "Unable to create resource directory" << path;
        }
    }
    return clean(path);
}

PkString KoResourcePaths::locateInternal(const PkString &type, const PkString &filename)
{
    return findResourceInternal(type, filename);
}

PkString KoResourcePaths::locateLocalInternal(const PkString &type, const PkString &filename,
                                              bool createDir)
{
    return PkResourceStorage::joinPath(saveLocationInternal(type, PkString(), createDir),
                                       normalizedRelativePath(filename));
}

PkStringList KoResourcePaths::findExtraResourceDirs() const
{
    PkStringList result;
    const PkString raw = environment("EXTRA_RESOURCE_DIRS");
    if (!raw.isEmpty()) {
        for (const PkString &entry : raw.split(u';')) {
            if (!entry.isEmpty()) {
                result.append(entry);
            }
        }
    }

    PkConfigGroup config(PkSharedConfig::openConfig(), PkString());
    const PkString customPath = configuredResourceLocation(config);
    if (!customPath.isEmpty()) {
        result.append(customPath);
    }
    const PkString defaultPath = platformDir(PkResourceStorage::PlatformDir::AppData);
    const PkString appDataPath = getAppDataLocation();
    if (appDataPath != defaultPath) {
        result.append(appDataPath);
    }
    result.removeDuplicates();
    return result;
}
