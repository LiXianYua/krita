/*
 *  SPDX-FileCopyrightText: 2019 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisUsageLogger.h"

#include "KisGlobalFileSystem.h"

#include <KritaVersionWrapper.h>

#include <PkDateTime.h>
#include <PkMessageLogger.h>

#include <clocale>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <process.h>
#include <windows.h>
#else
#include <sys/utsname.h>
#include <unistd.h>
#endif

#ifdef __APPLE__
#include <crt_externs.h>
#endif

#ifdef __ANDROID__
#include <KisAndroidExitInfo.h>
#endif

namespace {

namespace fs = std::filesystem;

KisUsageLogger *s_instance()
{
    static KisUsageLogger instance;
    return &instance;
}

PkString fromUtf8(const std::string &text)
{
    return PkString::PkFromUtf8(text.data(), static_cast<int>(text.size()));
}

std::string currentTimestamp()
{
    return PkDateTime::currentDateTime().toString(PkDateTime::DateFormat::RFC2822Date);
}

long long processId()
{
#ifdef _WIN32
    return static_cast<long long>(::_getpid());
#else
    return static_cast<long long>(::getpid());
#endif
}

bool readFile(const fs::path &path, std::string *contents);

std::string processCommandLine()
{
#ifdef _WIN32
    const wchar_t *commandLine = ::GetCommandLineW();
    return commandLine ? fs::path(commandLine).u8string() : std::string();
#elif defined(__APPLE__)
    const int count = *_NSGetArgc();
    char **arguments = *_NSGetArgv();
    std::string commandLine;
    for (int i = 0; arguments && i < count; ++i) {
        if (i != 0) {
            commandLine.push_back(' ');
        }
        commandLine += arguments[i] ? arguments[i] : "";
    }
    return commandLine;
#elif defined(__linux__) || defined(__ANDROID__)
    std::string commandLine;
    if (!readFile("/proc/self/cmdline", &commandLine)) {
        return std::string();
    }
    for (char &character : commandLine) {
        if (character == '\0') {
            character = ' ';
        }
    }
    while (!commandLine.empty() && commandLine.back() == ' ') {
        commandLine.pop_back();
    }
    return commandLine;
#else
    return std::string();
#endif
}

bool readFile(const fs::path &path, std::string *contents)
{
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return false;
    }
    contents->assign(std::istreambuf_iterator<char>(stream),
                     std::istreambuf_iterator<char>());
    return stream.good() || stream.eof();
}

bool replaceFile(const fs::path &path, const std::string &contents)
{
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    if (!stream) {
        return false;
    }
    stream.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    stream.flush();
    return stream.good();
}

std::vector<std::string> splitKeepingEmptyParts(const std::string &text,
                                                 const std::string &separator)
{
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t position = text.find(separator, start);
        if (position == std::string::npos) {
            parts.push_back(text.substr(start));
            return parts;
        }
        parts.push_back(text.substr(start, position - start));
        start = position + separator.size();
    }
}

std::string join(const std::vector<std::string> &parts,
                 std::size_t first,
                 const std::string &separator)
{
    std::string result;
    for (std::size_t i = first; i < parts.size(); ++i) {
        if (i != first) {
            result += separator;
        }
        result += parts[i];
    }
    return result;
}

const char *platformName()
{
#if defined(_WIN32)
    return "Windows";
#elif defined(__ANDROID__)
    return "Android";
#elif defined(__APPLE__)
    return "macOS";
#elif defined(__linux__)
    return "Linux";
#elif defined(__FreeBSD__)
    return "FreeBSD";
#else
    return "Unknown";
#endif
}

const char *architectureName()
{
#if defined(__aarch64__) || defined(_M_ARM64)
    return "arm64";
#elif defined(__arm__) || defined(_M_ARM)
    return "arm";
#elif defined(__x86_64__) || defined(_M_X64)
    return "x86_64";
#elif defined(__i386__) || defined(_M_IX86)
    return "x86";
#else
    return "unknown";
#endif
}

} // namespace

const PkString KisUsageLogger::s_sectionHeader(
    "================================================================================\n");

struct KisUsageLogger::Private {
    bool active {false};
    fs::path logPath;
    fs::path sysInfoPath;
    std::ofstream logFile;
    std::ofstream sysInfoFile;
};

KisUsageLogger::KisUsageLogger()
    : d(new Private)
{
    const fs::path dataDirectory = KisGlobalFileSystem::writableLocation(
        KisGlobalFileSystem::Location::GenericData);
    std::error_code error;
    fs::create_directories(dataDirectory, error);
    if (error) {
        qWarning() << "Could not create usage-log directory"
                   << KisGlobalFileSystem::fromPath(dataDirectory)
                   << error.message();
    }

    d->logPath = dataDirectory / "krita.log";
    d->sysInfoPath = dataDirectory / "krita-sysinfo.log";

    error.clear();
    const std::uintmax_t logSize = fs::file_size(d->logPath, error);
    if (!error && logSize > 100ULL * 1000ULL * 1000ULL) {
        std::ofstream truncate(d->logPath, std::ios::binary | std::ios::trunc);
        if (!truncate) {
            qWarning() << "Could not clear the >100MB usage log"
                       << KisGlobalFileSystem::fromPath(d->logPath);
        }
    } else {
        rotateLog();
    }

    d->logFile.open(d->logPath, std::ios::binary | std::ios::app);
    if (!d->logFile) {
        qWarning() << "Could not open usage log for writing"
                   << KisGlobalFileSystem::fromPath(d->logPath);
    }

    d->sysInfoFile.open(d->sysInfoPath, std::ios::binary | std::ios::trunc);
    if (!d->sysInfoFile) {
        qWarning() << "Could not open system-info log for writing"
                   << KisGlobalFileSystem::fromPath(d->sysInfoPath);
    }
}

KisUsageLogger::~KisUsageLogger()
{
    if (d->active) {
        close();
    }
}

void KisUsageLogger::initialize()
{
    KisUsageLogger *logger = s_instance();
    logger->d->active = true;

    const std::string systemInfo = basicSystemInfo().PkToUtf8();
    if (logger->d->sysInfoFile.is_open()) {
        logger->d->sysInfoFile.write(systemInfo.data(),
                                     static_cast<std::streamsize>(systemInfo.size()));
        logger->d->sysInfoFile.flush();
    }
}

PkString KisUsageLogger::basicSystemInfo()
{
    std::ostringstream information;
    information << "Krita\n"
                << "\n Version: " << KritaVersionWrapper::versionString(true).PkToUtf8()
                << "\n\nOS Information\n"
                << "\n  Platform: " << platformName()
                << "\n  CPU Architecture: " << architectureName()
                << "\n  Hardware Threads: " << std::thread::hardware_concurrency();

#ifndef _WIN32
    struct utsname systemName {};
    if (::uname(&systemName) == 0) {
        information << "\n  Kernel: " << systemName.sysname
                    << "\n  Kernel Version: " << systemName.release
                    << "\n  Machine: " << systemName.machine;
    }
#endif

#if defined(__linux__) && !defined(__ANDROID__)
    if (const char *desktop = std::getenv("XDG_CURRENT_DESKTOP")) {
        information << "\n  Desktop: " << desktop;
    }
    information << "\n  AppImage build: "
                << (std::getenv("APPIMAGE") ? "Yes" : "No");
#endif
    information << "\n\n";
    return fromUtf8(information.str());
}

void KisUsageLogger::writeLocaleSysInfo()
{
    KisUsageLogger *logger = s_instance();
    if (!logger->d->active || !logger->d->sysInfoFile.is_open()) {
        return;
    }

    std::ostringstream information;
    information << "Locale\n";
    const char *cLocale = std::setlocale(LC_ALL, nullptr);
    information << "\n  C locale: " << (cLocale ? cLocale : "unknown");
    try {
        information << "\n  C++ locale: " << std::locale("").name();
    } catch (const std::runtime_error &) {
        information << "\n  C++ locale: unavailable";
    }
    information << "\n\n";

    const std::string text = information.str();
    logger->d->sysInfoFile.write(text.data(), static_cast<std::streamsize>(text.size()));
    logger->d->sysInfoFile.flush();
}

void KisUsageLogger::close()
{
    KisUsageLogger *logger = s_instance();
    if (logger->d->active) {
        log("CLOSING SESSION");
    }
    logger->d->active = false;
    if (logger->d->logFile.is_open()) {
        logger->d->logFile.flush();
        logger->d->logFile.close();
    }
    if (logger->d->sysInfoFile.is_open()) {
        logger->d->sysInfoFile.flush();
        logger->d->sysInfoFile.close();
    }
}

void KisUsageLogger::log(const PkString &message)
{
    KisUsageLogger *logger = s_instance();
    if (!logger->d->active || !logger->d->logFile.is_open()) {
        return;
    }

    const std::string timestamp = currentTimestamp();
    logger->d->logFile.write(timestamp.data(), static_cast<std::streamsize>(timestamp.size()));
    logger->d->logFile.write(": ", 2);
    write(message);
}

void KisUsageLogger::write(const PkString &message)
{
    KisUsageLogger *logger = s_instance();
    if (!logger->d->active || !logger->d->logFile.is_open()) {
        return;
    }

    const std::string text = message.PkToUtf8();
    logger->d->logFile.write(text.data(), static_cast<std::streamsize>(text.size()));
    logger->d->logFile.put('\n');
    logger->d->logFile.flush();
}

void KisUsageLogger::writeSysInfo(const PkString &message)
{
    KisUsageLogger *logger = s_instance();
    if (!logger->d->active || !logger->d->sysInfoFile.is_open()) {
        return;
    }

    const std::string text = message.PkToUtf8();
    logger->d->sysInfoFile.write(text.data(), static_cast<std::streamsize>(text.size()));
    logger->d->sysInfoFile.put('\n');
    logger->d->sysInfoFile.flush();
}

void KisUsageLogger::writeHeader()
{
    KisUsageLogger *logger = s_instance();
    if (!logger->d->active || !logger->d->logFile.is_open()) {
        return;
    }

    const std::string sectionHeader = s_sectionHeader.PkToUtf8();
    logger->d->logFile.write(sectionHeader.data(),
                             static_cast<std::streamsize>(sectionHeader.size()));

    std::ostringstream sessionHeader;
    const std::string commandLine = processCommandLine();
    sessionHeader << "SESSION: " << currentTimestamp() << ". Executing "
                  << (commandLine.empty() ? "unknown" : commandLine) << "\n\n"
                  << "Krita Version: "
                  << KritaVersionWrapper::versionString(true).PkToUtf8()
                  << ". Process ID: " << processId() << "\n"
                  << "-- -- -- -- -- -- -- --\n";
    const std::string text = sessionHeader.str();
    logger->d->logFile.write(text.data(), static_cast<std::streamsize>(text.size()));
    logger->d->logFile.flush();

#ifdef __ANDROID__
    const KisAndroidExitInfo androidExitInfo = KisAndroidExitInfo::getLast();
    if (androidExitInfo.isValid()) {
        log(PkString("Last exit: %1").arg(androidExitInfo.buildLogString()));
    }
#endif
}

PkString KisUsageLogger::screenInformation()
{
    return PkString("Display Information\n  Unavailable in the headless core\n");
}

void KisUsageLogger::rotateLog()
{
    std::error_code error;
    if (!fs::exists(d->logPath, error) || error) {
        return;
    }

    std::string logContents;
    if (!readFile(d->logPath, &logContents)) {
        return;
    }

    const std::string sectionHeader = s_sectionHeader.PkToUtf8();
    const std::size_t lastHeader = logContents.rfind(sectionHeader);
    const std::string lastSession = lastHeader == std::string::npos
        ? logContents : logContents.substr(lastHeader + sectionHeader.size());
    if (lastSession.find("CLOSING SESSION") == std::string::npos) {
        logContents += "\nKRITA DID NOT CLOSE CORRECTLY\n";

        const fs::path crashLog = KisGlobalFileSystem::writableLocation(
            KisGlobalFileSystem::Location::GenericConfig) / "kritacrash.log";
        std::string crashes;
        if (readFile(crashLog, &crashes)) {
            const std::vector<std::string> crashItems =
                splitKeepingEmptyParts(crashes, "-------------------");
            logContents += "\nThere were " + std::to_string(crashItems.size()) +
                           " crashes in total in the crash log.\n";
            if (!crashes.empty()) {
                logContents += crashItems.back();
            }
        }
        replaceFile(d->logPath, logContents);
    }

    const std::vector<std::string> logItems =
        splitKeepingEmptyParts(logContents, "SESSION:");
    if (logItems.size() > static_cast<std::size_t>(s_maxLogs)) {
        const std::size_t first = logItems.size() - static_cast<std::size_t>(s_maxLogs);
        replaceFile(d->logPath, join(logItems, first, "\nSESSION:"));
    }
}
