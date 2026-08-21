#include "KisBackup.h"
#include "KisFileUtils.h"
#include "KisGlobalFileSystem.h"
#include "KisUsageLogger.h"
#include "kis_assert.h"

#include "config-safe-asserts.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

bool setEnvironmentValue(const std::string &name,
                         const std::optional<std::string> &value)
{
#ifdef _WIN32
    return ::_putenv_s(name.c_str(), value ? value->c_str() : "") == 0;
#else
    return value ? ::setenv(name.c_str(), value->c_str(), 1) == 0
                 : ::unsetenv(name.c_str()) == 0;
#endif
}

long long processId()
{
#ifdef _WIN32
    return static_cast<long long>(::_getpid());
#else
    return static_cast<long long>(::getpid());
#endif
}

class EnvironmentGuard
{
public:
    EnvironmentGuard(const char *name, const std::string &value)
        : m_name(name)
    {
        if (const char *oldValue = std::getenv(name)) {
            m_oldValue = oldValue;
        }
        if (!setEnvironmentValue(m_name, value)) {
            std::cerr << "FAIL: could not set environment variable " << m_name << '\n';
            std::exit(1);
        }
    }

    ~EnvironmentGuard()
    {
        if (!setEnvironmentValue(m_name, m_oldValue)) {
            std::abort();
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
                 ("kritaglobal-file-io-" + std::to_string(processId())))
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

struct ChildResult
{
    bool launched = false;
    bool terminatedBySignal = false;
    int exitCode = -1;
};

ChildResult runChild(const fs::path &executable, const char *scenario)
{
    const std::string executableText = executable.string();
#ifdef _WIN32
    const char *arguments[] = {
        executableText.c_str(), "--assert-child", scenario, nullptr
    };
    const intptr_t status = ::_spawnv(_P_WAIT, executableText.c_str(), arguments);
    return {status != -1, false, static_cast<int>(status)};
#else
    const pid_t child = ::fork();
    if (child < 0) {
        return {};
    }
    if (child == 0) {
        ::execl(executableText.c_str(), executableText.c_str(),
                "--assert-child", scenario, static_cast<char *>(nullptr));
        ::_exit(127);
    }

    int status = 0;
    pid_t waited = -1;
    do {
        waited = ::waitpid(child, &status, 0);
    } while (waited < 0 && errno == EINTR);
    if (waited < 0) {
        return {};
    }
    if (WIFSIGNALED(status)) {
        return {true, true, WTERMSIG(status)};
    }
    return {true, false, WIFEXITED(status) ? WEXITSTATUS(status) : -1};
#endif
}

bool terminatedBeforeReturn(const ChildResult &result, int returnedSentinel)
{
    return result.launched &&
           (result.terminatedBySignal || result.exitCode != returnedSentinel);
}

void testAssertionPolicies(const fs::path &executable)
{
    constexpr int fatalReturned = 80;
    constexpr int fatalXReturned = 81;
    constexpr int hardRecoveryReturned = 82;
    constexpr int safeRecoveryReturned = 42;

    const ChildResult fatal = runChild(executable, "fatal");
    require(terminatedBeforeReturn(fatal, fatalReturned),
            "fatal assertion must terminate its subprocess");

    const ChildResult fatalX = runChild(executable, "fatal-x");
    require(terminatedBeforeReturn(fatalX, fatalXReturned),
            "fatal X assertion must terminate its subprocess");

    const ChildResult hardRecovery = runChild(executable, "hard-recover");
    require(terminatedBeforeReturn(hardRecovery, hardRecoveryReturned),
            "non-safe recover assertion must terminate before its recovery branch");

    const ChildResult safeRecovery = runChild(executable, "safe-recover");
#ifdef CRASH_ON_SAFE_ASSERTS
    require(terminatedBeforeReturn(safeRecovery, safeRecoveryReturned),
            "safe recover assertion must terminate when CRASH_ON_SAFE_ASSERTS is enabled");
#else
    require(safeRecovery.launched && !safeRecovery.terminatedBySignal &&
                safeRecovery.exitCode == safeRecoveryReturned,
            "safe recover assertion must execute its recovery branch");
#endif
}

int runAssertionChild(const std::string &scenario)
{
    constexpr int fatalReturned = 80;
    constexpr int fatalXReturned = 81;
    constexpr int hardRecoveryReturned = 82;
    constexpr int safeRecoveryReturned = 42;

    if (!setEnvironmentValue("KRITA_NO_ASSERT_MSG", std::string("1"))) {
        return 79;
    }
    if (scenario == "fatal") {
        kis_assert_exception("false", __FILE__, __LINE__);
        return fatalReturned;
    }
    if (scenario == "fatal-x") {
        kis_assert_x_exception("false", "assertion-test", "fatal X",
                               __FILE__, __LINE__);
        return fatalXReturned;
    }
    if (scenario == "hard-recover") {
        KIS_ASSERT_RECOVER(false) {
            return hardRecoveryReturned;
        }
        return 83;
    }
    if (scenario == "safe-recover") {
        KIS_SAFE_ASSERT_RECOVER(false) {
            return safeRecoveryReturned;
        }
        return 43;
    }
    return 78;
}

void testPlatformLocationsAndUtf8RoundTrip(const fs::path &root)
{
    const fs::path dataRoot = root / "d\xC3\xA1ta";
    const fs::path configRoot = root / "config";
#ifdef _WIN32
    EnvironmentGuard dataGuard("APPDATA", dataRoot.string());

    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::GenericData) == dataRoot,
            "GenericData must honor APPDATA");
    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::GenericConfig) == dataRoot,
            "GenericConfig must honor APPDATA");
    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::AppData) == dataRoot / "krita",
            "AppData must be application-scoped under APPDATA");
#elif defined(__APPLE__)
    EnvironmentGuard homeGuard("HOME", dataRoot.string());
    const fs::path library = dataRoot / "Library";

    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::GenericData) ==
                library / "Application Support",
            "GenericData must use Application Support");
    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::GenericConfig) ==
                library / "Preferences",
            "GenericConfig must use Preferences");
    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::AppData) ==
                library / "Application Support" / "krita",
            "AppData must be application-scoped under Application Support");
#elif defined(__ANDROID__)
    EnvironmentGuard dataGuard("ANDROID_APP_DATA", dataRoot.string());

    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::GenericData) == dataRoot,
            "GenericData must honor ANDROID_APP_DATA");
    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::GenericConfig) == dataRoot,
            "GenericConfig must honor ANDROID_APP_DATA");
    require(KisGlobalFileSystem::writableLocation(
                KisGlobalFileSystem::Location::AppData) == dataRoot,
            "AppData must honor ANDROID_APP_DATA");
#else
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
#endif

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
#ifdef _WIN32
    EnvironmentGuard dataGuard("APPDATA", dataRoot.string());
    const fs::path expectedDataRoot = dataRoot;
#elif defined(__APPLE__)
    EnvironmentGuard homeGuard("HOME", dataRoot.string());
    const fs::path expectedDataRoot = dataRoot / "Library" / "Application Support";
#elif defined(__ANDROID__)
    EnvironmentGuard dataGuard("ANDROID_APP_DATA", dataRoot.string());
    const fs::path expectedDataRoot = dataRoot;
#else
    EnvironmentGuard dataGuard("XDG_DATA_HOME", dataRoot.string());
    const fs::path expectedDataRoot = dataRoot;
#endif

    {
        KisUsageLogger logger;
        require(fs::exists(expectedDataRoot / "krita.log"),
                "usage logger must create the session log");
        require(fs::exists(expectedDataRoot / "krita-sysinfo.log"),
                "usage logger must create the system-info log");
    }
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 3 && std::string(argv[1]) == "--assert-child") {
        return runAssertionChild(argv[2]);
    }

    TemporaryDirectory temporaryDirectory;
    testPlatformLocationsAndUtf8RoundTrip(temporaryDirectory.path());
    testResolveAbsolutePath(temporaryDirectory.path());
    testSimpleAndNumberedBackup(temporaryDirectory.path());
    testDeduplicateFileName();
    testUsageLoggerCreatesItsFiles(temporaryDirectory.path());
    testAssertionPolicies(fs::absolute(argv[0]));
    std::cout << "PASS: kritaglobal file I/O\n";
    return 0;
}
