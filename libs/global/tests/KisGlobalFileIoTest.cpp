#include "KisBackup.h"
#include "KisFileUtils.h"
#include "KisGlobalFileSystem.h"
#include "KisUsageLogger.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <unistd.h>

namespace fs = std::filesystem;

namespace {

class EnvironmentGuard
{
public:
    EnvironmentGuard(const char *name, const std::string &value)
        : m_name(name)
    {
        if (const char *oldValue = std::getenv(name)) {
            m_oldValue = oldValue;
        }
        ::setenv(name, value.c_str(), 1);
    }

    ~EnvironmentGuard()
    {
        if (m_oldValue) {
            ::setenv(m_name.c_str(), m_oldValue->c_str(), 1);
        } else {
            ::unsetenv(m_name.c_str());
        }
    }

private:
    std::string m_name;
    std::optional<std::string> m_oldValue;
};

class TemporaryDirectory
{
public:
    TemporaryDirectory()
        : m_path(fs::temp_directory_path() /
                 ("kritaglobal-file-io-" + std::to_string(::getpid())))
    {
        std::error_code error;
        fs::remove_all(m_path, error);
        fs::create_directories(m_path);
    }

    ~TemporaryDirectory()
    {
        std::error_code error;
        fs::remove_all(m_path, error);
    }

    const fs::path &path() const
    {
        return m_path;
    }

private:
    fs::path m_path;
};

void writeText(const fs::path &path, const std::string &text)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    stream << text;
}

std::string readText(const fs::path &path)
{
    std::ifstream stream(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(stream),
                       std::istreambuf_iterator<char>());
}

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        std::exit(1);
    }
}

void requirePath(const PkString &actual, const fs::path &expected, const char *message)
{
    require(KisGlobalFileSystem::toPath(actual).lexically_normal() == expected.lexically_normal(),
            message);
}

void testPlatformLocationsAndUtf8RoundTrip(const fs::path &root)
{
    const fs::path dataRoot = root / "d\xC3\xA1ta";
    const fs::path configRoot = root / "config";
    EnvironmentGuard dataGuard("XDG_DATA_HOME", dataRoot.string());
    EnvironmentGuard configGuard("XDG_CONFIG_HOME", configRoot.string());

    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::GenericData) == dataRoot,
            "GenericData must honor XDG_DATA_HOME");
    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::GenericConfig) == configRoot,
            "GenericConfig must honor XDG_CONFIG_HOME");
    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::AppData) == dataRoot / "krita",
            "AppData must be application-scoped under XDG_DATA_HOME");

    const fs::path unicodePath = root / fs::u8path(u8"\u753B\u5E03/brush-\u00F8.kpp");
    require(KisGlobalFileSystem::toPath(KisGlobalFileSystem::fromPath(unicodePath)) == unicodePath,
            "filesystem/PkString conversion must preserve UTF-8 paths");
}

void testResolveAbsolutePath(const fs::path &root)
{
    const fs::path directoryBase = root / "directory-base";
    const fs::path fileBase = root / "file-base.kra";
    fs::create_directories(directoryBase);
    writeText(fileBase, "document");

    requirePath(KritaUtils::resolveAbsoluteFilePath(
                    KisGlobalFileSystem::fromPath(directoryBase), PkString("child.png")),
                directoryBase / "child.png",
                "directory base must resolve child beneath itself");
    requirePath(KritaUtils::resolveAbsoluteFilePath(
                    KisGlobalFileSystem::fromPath(fileBase), PkString("child.png")),
                root / "child.png",
                "file base must resolve child beside the file");

    const fs::path absolute = root / "already-absolute.png";
    requirePath(KritaUtils::resolveAbsoluteFilePath(
                    PkString("ignored"), KisGlobalFileSystem::fromPath(absolute)),
                absolute,
                "absolute file names must be preserved");
}

void testSimpleAndNumberedBackup(const fs::path &root)
{
    const fs::path source = root / "document.kra";
    const fs::path backupDir = root / "backups";
    fs::create_directories(backupDir);
    writeText(source, "new document");
    writeText(backupDir / "document.kra~", "stale backup");

    require(KisBackup::simpleBackupFile(KisGlobalFileSystem::fromPath(source),
                                        KisGlobalFileSystem::fromPath(backupDir)),
            "simple backup must copy successfully");
    require(readText(backupDir / "document.kra~") == "new document",
            "simple backup must replace stale destination content");

    writeText(backupDir / "document.kra.1~", "old one");
    writeText(backupDir / "document.kra.2~", "old two");
    require(KisBackup::numberedBackupFile(KisGlobalFileSystem::fromPath(source),
                                          KisGlobalFileSystem::fromPath(backupDir),
                                          PkString("~"), 3),
            "numbered backup must copy successfully");
    require(readText(backupDir / "document.kra.1~") == "new document",
            "numbered backup #1 must contain the source");
    require(readText(backupDir / "document.kra.2~") == "old one",
            "numbered backup must rotate #1 to #2");
    require(readText(backupDir / "document.kra.3~") == "old two",
            "numbered backup must rotate #2 to #3");
}

void testDeduplicateFileName()
{
    const auto deduplicate = [](const char *fileName,
                                const char *separator,
                                std::vector<std::string> existing) {
        return KritaUtils::deduplicateFileName(
            PkString(fileName), PkString(separator),
            [existing = std::move(existing)](PkString candidate) {
                const std::string value = candidate.PkToUtf8();
                return std::find(existing.begin(), existing.end(), value) == existing.end();
            }).PkToUtf8();
    };

    require(deduplicate("foo.tar.gz", "_", {"foo.tar.gz", "foo_0.tar.gz"}) ==
                "foo_1.tar.gz",
            "deduplicate must increment an existing suffix");
    require(deduplicate("foo.0.2.tar.gz", ".", {"foo.tar.gz", "foo.0.2.tar.gz"}) ==
                "foo.1.2.tar.gz",
            "deduplicate must preserve the complete suffix after the leftmost dot");
    require(deduplicate("foo_xx.xx_0.tar.gz", "_xx.xx_", {"foo_xx.xx_0.tar.gz"}) ==
                "foo_xx.xx_1.tar.gz",
            "deduplicate must treat a dotted separator literally");
}

void testUsageLoggerCreatesItsFiles(const fs::path &root)
{
    const fs::path dataRoot = root / "usage-data";
    const fs::path configRoot = root / "usage-config";
    EnvironmentGuard dataGuard("XDG_DATA_HOME", dataRoot.string());
    EnvironmentGuard configGuard("XDG_CONFIG_HOME", configRoot.string());

    {
        KisUsageLogger logger;
        require(fs::exists(dataRoot / "krita.log"),
                "usage logger must create the session log");
        require(fs::exists(dataRoot / "krita-sysinfo.log"),
                "usage logger must create the system-info log");
    }
}

} // namespace

int main()
{
    TemporaryDirectory temporaryDirectory;
    testPlatformLocationsAndUtf8RoundTrip(temporaryDirectory.path());
    testResolveAbsolutePath(temporaryDirectory.path());
    testSimpleAndNumberedBackup(temporaryDirectory.path());
    testDeduplicateFileName();
    testUsageLoggerCreatesItsFiles(temporaryDirectory.path());
    std::cout << "PASS: kritaglobal file I/O\n";
    return 0;
}
