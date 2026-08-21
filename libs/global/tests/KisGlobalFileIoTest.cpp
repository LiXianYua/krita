#include "KisBackup.h"
#include "KisFileUtils.h"
#include "KisGlobalFileSystem.h"
#include "KisUsageLogger.h"
#include "kis_algebra_2d_debug_p.h"
#include "kis_dom_utils.h"
#include "kis_assert.h"

#include "config-safe-asserts.h"

#include <algorithm>
#include <cerrno>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include <PkLogSink.h>

#ifdef _WIN32
#include <process.h>
#else
#include <sys/wait.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

#ifdef _WIN32
using NativeString = std::wstring;
using NativeCharacter = wchar_t;
#else
using NativeString = std::string;
using NativeCharacter = char;
#endif

NativeString nativeText(const char *text)
{
#ifdef _WIN32
    NativeString result;
    while (*text) {
        result.push_back(static_cast<unsigned char>(*text++));
    }
    return result;
#else
    return text;
#endif
}

std::optional<NativeString> environmentValue(const NativeString &name)
{
#ifdef _WIN32
    if (const wchar_t *value = ::_wgetenv(name.c_str())) {
        return NativeString(value);
    }
#else
    if (const char *value = std::getenv(name.c_str())) {
        return NativeString(value);
    }
#endif
    return std::nullopt;
}

bool setEnvironmentValue(const NativeString &name,
                         const std::optional<NativeString> &value)
{
#ifdef _WIN32
    return ::_wputenv_s(name.c_str(), value ? value->c_str() : L"") == 0;
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
    EnvironmentGuard(NativeString name, NativeString value)
        : m_name(std::move(name))
    {
        m_oldValue = environmentValue(m_name);
        if (!setEnvironmentValue(m_name, value)) {
            std::cerr << "FAIL: could not set environment variable\n";
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
    NativeString m_name;
    std::optional<NativeString> m_oldValue;
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

void captureLog(PkLogLevel, const PkLogContext &, const char *message, void *userData)
{
    *static_cast<std::string *>(userData) = message ? message : "";
}

struct ChildResult
{
    bool launched = false;
    bool terminatedBySignal = false;
    int exitCode = -1;
};

constexpr int windowsAbortHandshakeStatus = 90;

#ifdef _WIN32
void abortHandshake(int)
{
    ::_exit(windowsAbortHandshakeStatus);
}
#endif

ChildResult runChild(const fs::path &executable, const NativeString &scenario)
{
    const NativeString executableText = executable.native();
#ifdef _WIN32
    const wchar_t *arguments[] = {
        executableText.c_str(), L"--assert-child", scenario.c_str(), nullptr
    };
    const intptr_t status = ::_wspawnv(_P_WAIT, executableText.c_str(), arguments);
    return {status != -1, false, static_cast<int>(status)};
#else
    const pid_t child = ::fork();
    if (child < 0) {
        return {};
    }
    if (child == 0) {
        ::execl(executableText.c_str(), executableText.c_str(),
                "--assert-child", scenario.c_str(), static_cast<char *>(nullptr));
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

bool terminatedByAbort(const ChildResult &result)
{
#ifdef _WIN32
    return result.launched && !result.terminatedBySignal &&
           result.exitCode == windowsAbortHandshakeStatus;
#else
    return result.launched && result.terminatedBySignal &&
           result.exitCode == SIGABRT;
#endif
}

void testDomIntegerParsing()
{
    bool ok = false;
    require(KisDomUtils::toInt(PkString("1234"), &ok) == 1234 && ok,
            "DOM integer parsing must retain C-locale integers");
    require(KisDomUtils::toInt(PkString("1.234"), &ok) == 1234 && ok,
            "DOM integer parsing must accept German thousands grouping");
    require(KisDomUtils::toInt(PkString("12.345.678"), &ok) == 12345678 && ok,
            "DOM integer parsing must accept repeated German thousands grouping");
    require(KisDomUtils::toInt(PkString("12.34"), &ok) == 0 && !ok,
            "DOM integer parsing must reject malformed German grouping");
    require(KisDomUtils::toInt(PkString("not-an-integer"), &ok) == 0 && !ok,
            "DOM integer parsing must report failed conversion");

    // Qt 5.15.7 oracle: QString::toInt trims both Unicode spaces for the
    // C-locale path, while QLocale(German) accepts the grouped form after the
    // same trimming. Keep these literals independent from PkString::trimmed().
    require(KisDomUtils::toInt(PkString(u8"\u00A0" "1234" "\u00A0"), &ok) == 1234 && ok,
            "DOM integer parsing must trim NBSP around C-locale integers");
    require(KisDomUtils::toInt(PkString(u8"\u202F" "1234" "\u202F"), &ok) == 1234 && ok,
            "DOM integer parsing must trim NNBSP around C-locale integers");
    require(KisDomUtils::toInt(PkString(u8"\u00A0" "12.345.678" "\u00A0"), &ok) == 12345678 && ok,
            "DOM integer parsing must trim NBSP before German grouping fallback");
    require(KisDomUtils::toInt(PkString(u8"\u202F" "12.345.678" "\u202F"), &ok) == 12345678 && ok,
            "DOM integer parsing must trim NNBSP before German grouping fallback");
}

void testVectorPathPointDebugFormattingState()
{
    std::string captured;
    const int sink = PkLogAddSink(captureLog, &captured);
    {
        PkDebug debug = PkDebugMakeForTest(PkLogDebug, "kritaglobal.format-test");
        debug << qSetRealNumberPrecision(3) << qSetFieldWidth(4) << qSetPadChar('_');
        KisAlgebra2D::Private::writeVectorPathPoint(
            debug,
            KisAlgebra2D::VectorPath::VectorPathPoint::lineTo(PkPointF(1.23456, 7.0)));
        debug << 9.876;
    }
    PkLogRemoveSink(sink);

    require(captured == "(line ___(1.23__, ___7___)___)___ 9.88___",
            "VectorPathPoint debug formatting must preserve precision, field width, pad char, and spacing state");
}

void testAssertionPolicies(const fs::path &executable, const fs::path &root)
{
    constexpr int safeRecoveryReturned = 42;

    const fs::path dataRoot = root / "assert-data";
    const fs::path configRoot = root / "assert-config";
#ifdef _WIN32
    EnvironmentGuard dataGuard(nativeText("APPDATA"), dataRoot.native());
    const fs::path expectedDataRoot = dataRoot;
    const fs::path expectedConfigRoot = dataRoot;
#elif defined(__APPLE__)
    EnvironmentGuard homeGuard(nativeText("HOME"), dataRoot.native());
    const fs::path expectedDataRoot = dataRoot / "Library" / "Application Support";
    const fs::path expectedConfigRoot = dataRoot / "Library" / "Preferences";
#elif defined(__ANDROID__)
    EnvironmentGuard dataGuard(nativeText("ANDROID_APP_DATA"), dataRoot.native());
    const fs::path expectedDataRoot = dataRoot;
    const fs::path expectedConfigRoot = dataRoot;
#else
    EnvironmentGuard dataGuard(nativeText("XDG_DATA_HOME"), dataRoot.native());
    EnvironmentGuard configGuard(nativeText("XDG_CONFIG_HOME"), configRoot.native());
    const fs::path expectedDataRoot = dataRoot;
    const fs::path expectedConfigRoot = configRoot;
#endif
    EnvironmentGuard expectedDataGuard(nativeText("KRITA_ASSERT_EXPECTED_DATA"),
                                       expectedDataRoot.native());
    EnvironmentGuard expectedConfigGuard(nativeText("KRITA_ASSERT_EXPECTED_CONFIG"),
                                         expectedConfigRoot.native());

    require(!terminatedByAbort(runChild(executable, nativeText("ordinary-exit"))),
            "an ordinary zero exit must not satisfy fatal assertion termination");
    require(!terminatedByAbort(runChild(executable, nativeText("setup-failure"))),
            "the child setup-failure status must not satisfy fatal assertion termination");
    require(!terminatedByAbort(runChild(executable, nativeText("exec-failure"))),
            "the exec-failure status must not satisfy fatal assertion termination");

    const ChildResult fatal = runChild(executable, nativeText("fatal"));
    require(terminatedByAbort(fatal),
            "fatal assertion must terminate its subprocess");

    const ChildResult fatalX = runChild(executable, nativeText("fatal-x"));
    require(terminatedByAbort(fatalX),
            "fatal X assertion must terminate its subprocess");

    const ChildResult hardRecovery = runChild(executable, nativeText("hard-recover"));
    require(terminatedByAbort(hardRecovery),
            "non-safe recover assertion must terminate before its recovery branch");

    const ChildResult safeRecovery = runChild(executable, nativeText("safe-recover"));
#ifdef CRASH_ON_SAFE_ASSERTS
    require(terminatedByAbort(safeRecovery),
            "safe recover assertion must terminate when CRASH_ON_SAFE_ASSERTS is enabled");
#else
    require(safeRecovery.launched && !safeRecovery.terminatedBySignal &&
                safeRecovery.exitCode == safeRecoveryReturned,
            "safe recover assertion must execute its recovery branch");
#endif
}

int runAssertionChild(const NativeString &scenario)
{
    constexpr int fatalReturned = 80;
    constexpr int fatalXReturned = 81;
    constexpr int hardRecoveryReturned = 82;
    constexpr int safeRecoveryReturned = 42;

    const std::optional<NativeString> expectedData =
        environmentValue(nativeText("KRITA_ASSERT_EXPECTED_DATA"));
    const std::optional<NativeString> expectedConfig =
        environmentValue(nativeText("KRITA_ASSERT_EXPECTED_CONFIG"));
    if (!expectedData || !expectedConfig ||
        KisGlobalFileSystem::writableLocation(
            KisGlobalFileSystem::Location::GenericData) != fs::path(*expectedData) ||
        KisGlobalFileSystem::writableLocation(
            KisGlobalFileSystem::Location::GenericConfig) != fs::path(*expectedConfig)) {
        return 79;
    }

    if (scenario == nativeText("ordinary-exit")) {
        return 0;
    }
    if (scenario == nativeText("setup-failure")) {
        return 79;
    }
    if (scenario == nativeText("exec-failure")) {
        return 127;
    }

    if (!setEnvironmentValue(nativeText("KRITA_NO_ASSERT_MSG"), nativeText("1"))) {
        return 79;
    }
#ifdef _WIN32
    std::signal(SIGABRT, abortHandshake);
#endif
    if (scenario == nativeText("fatal")) {
        kis_assert_exception("false", __FILE__, __LINE__);
        return fatalReturned;
    }
    if (scenario == nativeText("fatal-x")) {
        kis_assert_x_exception("false", "assertion-test", "fatal X",
                               __FILE__, __LINE__);
        return fatalXReturned;
    }
    if (scenario == nativeText("hard-recover")) {
        KIS_ASSERT_RECOVER(false) {
            return hardRecoveryReturned;
        }
        return 83;
    }
    if (scenario == nativeText("safe-recover")) {
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
    EnvironmentGuard dataGuard(nativeText("APPDATA"), dataRoot.native());

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
    EnvironmentGuard homeGuard(nativeText("HOME"), dataRoot.native());
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
    EnvironmentGuard dataGuard(nativeText("ANDROID_APP_DATA"), dataRoot.native());

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
    EnvironmentGuard dataGuard(nativeText("XDG_DATA_HOME"), dataRoot.native());
    EnvironmentGuard configGuard(nativeText("XDG_CONFIG_HOME"), configRoot.native());

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
    EnvironmentGuard dataGuard(nativeText("APPDATA"), dataRoot.native());
    const fs::path expectedDataRoot = dataRoot;
#elif defined(__APPLE__)
    EnvironmentGuard homeGuard(nativeText("HOME"), dataRoot.native());
    const fs::path expectedDataRoot = dataRoot / "Library" / "Application Support";
#elif defined(__ANDROID__)
    EnvironmentGuard dataGuard(nativeText("ANDROID_APP_DATA"), dataRoot.native());
    const fs::path expectedDataRoot = dataRoot;
#else
    EnvironmentGuard dataGuard(nativeText("XDG_DATA_HOME"), dataRoot.native());
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

int testMain(int argc, NativeCharacter **argv)
{
    if (argc == 3 && NativeString(argv[1]) == nativeText("--assert-child")) {
        return runAssertionChild(argv[2]);
    }

    TemporaryDirectory temporaryDirectory;
    testPlatformLocationsAndUtf8RoundTrip(temporaryDirectory.path());
    testResolveAbsolutePath(temporaryDirectory.path());
    testSimpleAndNumberedBackup(temporaryDirectory.path());
    testDeduplicateFileName();
    testDomIntegerParsing();
    testVectorPathPointDebugFormattingState();
    testUsageLoggerCreatesItsFiles(temporaryDirectory.path());
    testAssertionPolicies(fs::absolute(argv[0]), temporaryDirectory.path());
    std::cout << "PASS: kritaglobal file I/O\n";
    return 0;
}

#ifdef _WIN32
int wmain(int argc, wchar_t **argv)
{
    return testMain(argc, argv);
}
#else
int main(int argc, char **argv)
{
    return testMain(argc, argv);
}
#endif
