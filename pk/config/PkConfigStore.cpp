#include "PkConfigStore.h"

#include <algorithm>
#include <atomic>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

#ifdef __HAIKU__
#include <FindDirectory.h>
#include <StorageDefs.h>
#endif

namespace {

namespace fs = std::filesystem;
using ConfigData = std::map<PkString, std::map<PkString, PkString>>;

constexpr const char kOwnedSectionPrefix[] = "[PkConfig-v1:";
constexpr std::size_t kMaximumConfigBytes = 16U * 1024U * 1024U;

#ifdef PKCONFIG_ENABLE_TEST_HOOKS
std::optional<fs::path> g_testConfigFilePath;
bool g_storeConstructed = false;
std::atomic<int> g_commitFailureForTesting{0};
#endif

fs::path environmentPath(const char *name)
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

fs::path homePath()
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

fs::path genericConfigPath()
{
    const fs::path home = homePath();
#ifdef _WIN32
    const fs::path roaming = environmentPath("APPDATA");
    return roaming.empty() ? home : roaming;
#elif defined(__APPLE__)
    return home / "Library" / "Preferences";
#elif defined(__ANDROID__)
    fs::path applicationHome = environmentPath("ANDROID_APP_DATA");
    if (applicationHome.empty()) applicationHome = environmentPath("HOME");
    if (applicationHome.empty()) {
        const fs::path temporary = environmentPath("TMPDIR");
        if (!temporary.empty()) applicationHome = temporary.parent_path() / "files";
    }
    return applicationHome.empty() ? home : applicationHome;
#elif defined(__HAIKU__)
    char path[B_PATH_NAME_LENGTH] = {};
    return find_directory(B_USER_SETTINGS_DIRECTORY, -1, true, path, sizeof(path)) == B_OK
        ? fs::u8path(path) : home;
#else
    const fs::path xdg = environmentPath("XDG_CONFIG_HOME");
    if (!xdg.empty()) return xdg;
    const fs::path configuredHome = environmentPath("HOME");
    return configuredHome.empty() ? fs::path() : configuredHome / ".config";
#endif
}

fs::path defaultConfigFilePath()
{
    return genericConfigPath() / "kritarc";
}

fs::path configFilePath()
{
#ifdef PKCONFIG_ENABLE_TEST_HOOKS
    if (g_testConfigFilePath) return *g_testConfigFilePath;
#endif
    return defaultConfigFilePath();
}

fs::path lockFilePath(const fs::path &configPath)
{
    fs::path path = configPath;
    path += ".lock";
    return path;
}

fs::path directoryForFileOperations(const fs::path &path)
{
    const fs::path parent = path.parent_path();
    return parent.empty() ? fs::path(".") : parent;
}

class ConfigFileLock
{
public:
    explicit ConfigFileLock(const fs::path &configPath)
    {
        const fs::path path = lockFilePath(configPath);
#ifdef _WIN32
        m_handle = CreateFileW(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                               nullptr, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (m_handle == INVALID_HANDLE_VALUE) return;
        OVERLAPPED overlapped = {};
        if (!LockFileEx(m_handle, LOCKFILE_EXCLUSIVE_LOCK, 0,
                        MAXDWORD, MAXDWORD, &overlapped)) {
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
            return;
        }
#else
        m_fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0600);
        if (m_fd < 0) return;
        struct flock lock = {};
        lock.l_type = F_WRLCK;
        lock.l_whence = SEEK_SET;
        int result;
        do {
            result = ::fcntl(m_fd, F_SETLKW, &lock);
        } while (result < 0 && errno == EINTR);
        if (result < 0) {
            ::close(m_fd);
            m_fd = -1;
            return;
        }
#endif
        m_locked = true;
    }

    ~ConfigFileLock()
    {
        if (!m_locked) return;
#ifdef _WIN32
        OVERLAPPED overlapped = {};
        UnlockFileEx(m_handle, 0, MAXDWORD, MAXDWORD, &overlapped);
        CloseHandle(m_handle);
#else
        struct flock lock = {};
        lock.l_type = F_UNLCK;
        lock.l_whence = SEEK_SET;
        ::fcntl(m_fd, F_SETLK, &lock);
        ::close(m_fd);
#endif
    }

    ConfigFileLock(const ConfigFileLock &) = delete;
    ConfigFileLock &operator=(const ConfigFileLock &) = delete;
    bool isLocked() const { return m_locked; }

private:
    bool m_locked = false;
#ifdef _WIN32
    HANDLE m_handle = INVALID_HANDLE_VALUE;
#else
    int m_fd = -1;
#endif
};

char hexDigit(unsigned int value)
{
    return value < 10 ? static_cast<char>('0' + value)
                      : static_cast<char>('a' + value - 10);
}

int hexValue(char character)
{
    if (character >= '0' && character <= '9') return character - '0';
    if (character >= 'a' && character <= 'f') return character - 'a' + 10;
    if (character >= 'A' && character <= 'F') return character - 'A' + 10;
    return -1;
}

bool hexDecode(const std::string &encoded, std::string *bytes)
{
    if (encoded.size() % 2 != 0) return false;
    bytes->clear();
    bytes->reserve(encoded.size() / 2);
    for (std::size_t index = 0; index < encoded.size(); index += 2) {
        const int high = hexValue(encoded[index]);
        const int low = hexValue(encoded[index + 1]);
        if (high < 0 || low < 0) return false;
        bytes->push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

bool isValidUtf8(const std::string &bytes)
{
    std::size_t index = 0;
    while (index < bytes.size()) {
        const unsigned char first = static_cast<unsigned char>(bytes[index++]);
        if (first <= 0x7f) continue;
        int continuationCount = 0;
        std::uint32_t codePoint = 0;
        if (first >= 0xc2 && first <= 0xdf) {
            continuationCount = 1;
            codePoint = first & 0x1f;
        } else if (first >= 0xe0 && first <= 0xef) {
            continuationCount = 2;
            codePoint = first & 0x0f;
        } else if (first >= 0xf0 && first <= 0xf4) {
            continuationCount = 3;
            codePoint = first & 0x07;
        } else {
            return false;
        }
        if (index + static_cast<std::size_t>(continuationCount) > bytes.size()) return false;
        for (int count = 0; count < continuationCount; ++count) {
            const unsigned char next = static_cast<unsigned char>(bytes[index++]);
            if ((next & 0xc0) != 0x80) return false;
            codePoint = (codePoint << 6) | (next & 0x3f);
        }
        const std::uint32_t minimum = continuationCount == 1 ? 0x80U
                                    : continuationCount == 2 ? 0x800U : 0x10000U;
        if (codePoint < minimum || codePoint > 0x10ffffU ||
            (codePoint >= 0xd800U && codePoint <= 0xdfffU)) return false;
    }
    return true;
}

bool decodePkString(const std::string &encoded, PkString *value)
{
    std::string bytes;
    if (!hexDecode(encoded, &bytes) || !isValidUtf8(bytes) ||
        bytes.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return false;
    *value = PkString::PkFromUtf8(bytes.data(), static_cast<int>(bytes.size()));
    return true;
}

struct ParsedConfig
{
    ConfigData data;
    std::vector<std::string> foreignLines;
};

bool isSectionHeader(const std::string &line)
{
    return line.size() >= 2 && line.front() == '[' && line.back() == ']';
}

bool readConfig(const fs::path &path, ParsedConfig *parsed)
{
    parsed->data.clear();
    parsed->foreignLines.clear();
    std::error_code error;
    if (!fs::exists(path, error)) return !error;
    if (error || !fs::is_regular_file(path, error) || error) return false;
    const std::uintmax_t size = fs::file_size(path, error);
    if (error || size > kMaximumConfigBytes) return false;
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;

    bool inOwnedSection = false;
    PkString currentGroup;
    std::string line;
    while (std::getline(input, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (isSectionHeader(line)) {
            if (line.compare(0, sizeof(kOwnedSectionPrefix) - 1, kOwnedSectionPrefix) == 0) {
                const std::size_t prefixLength = sizeof(kOwnedSectionPrefix) - 1;
                const std::string encodedGroup =
                    line.substr(prefixLength, line.size() - prefixLength - 1);
                if (!decodePkString(encodedGroup, &currentGroup) ||
                    parsed->data.find(currentGroup) != parsed->data.end()) return false;
                parsed->data[currentGroup] = {};
                inOwnedSection = true;
                continue;
            }
            inOwnedSection = false;
        }
        if (!inOwnedSection) {
            parsed->foreignLines.push_back(line);
            continue;
        }
        if (line.empty()) continue;
        const std::size_t equals = line.find('=');
        if (equals == std::string::npos || line.find('=', equals + 1) != std::string::npos) return false;
        PkString key;
        PkString value;
        if (!decodePkString(line.substr(0, equals), &key) ||
            !decodePkString(line.substr(equals + 1), &value) ||
            parsed->data[currentGroup].find(key) != parsed->data[currentGroup].end()) return false;
        parsed->data[currentGroup][key] = value;
    }
    return input.eof() && !input.bad();
}

bool appendBounded(std::string *output, std::string_view bytes)
{
    if (bytes.size() > kMaximumConfigBytes - output->size()) return false;
    output->append(bytes.data(), bytes.size());
    return true;
}

bool appendBounded(std::string *output, char byte)
{
    if (output->size() == kMaximumConfigBytes) return false;
    output->push_back(byte);
    return true;
}

bool appendEncodedPkString(std::string *output, const PkString &value)
{
    const std::string bytes = value.PkToUtf8();
    const std::size_t remaining = kMaximumConfigBytes - output->size();
    if (bytes.size() > remaining / 2U) return false;
    output->reserve(output->size() + bytes.size() * 2U);
    for (const unsigned char byte : bytes) {
        output->push_back(hexDigit(byte >> 4));
        output->push_back(hexDigit(byte & 0x0f));
    }
    return true;
}

bool serializeConfig(const ParsedConfig &parsed, std::string *output)
{
    output->clear();
    for (const std::string &line : parsed.foreignLines) {
        if (!appendBounded(output, line) || !appendBounded(output, '\n')) return false;
    }
    if (!output->empty() && output->size() >= 2 && (*output)[output->size() - 2] != '\n') {
        if (!appendBounded(output, '\n')) return false;
    }
    for (const auto &group : parsed.data) {
        if (!appendBounded(output, kOwnedSectionPrefix) ||
            !appendEncodedPkString(output, group.first) ||
            !appendBounded(output, "]\n")) return false;
        for (const auto &entry : group.second) {
            if (!appendEncodedPkString(output, entry.first) ||
                !appendBounded(output, '=') ||
                !appendEncodedPkString(output, entry.second) ||
                !appendBounded(output, '\n')) return false;
        }
        if (!appendBounded(output, '\n')) return false;
    }
    return true;
}

fs::path temporaryPath(const fs::path &configPath, std::uint64_t sequence)
{
    fs::path path = configPath;
#ifdef _WIN32
    const unsigned long processId = GetCurrentProcessId();
#else
    const long processId = static_cast<long>(::getpid());
#endif
    path += ".tmp." + std::to_string(processId) + "." + std::to_string(sequence);
    return path;
}

class AtomicConfigWriter
{
public:
    enum class CommitResult {
        NotReplaced,
        Replaced,
        ReplacedDurabilityUncertain
    };

    AtomicConfigWriter(const fs::path &destination, const std::string &contents)
        : m_destination(destination)
    {
        static std::atomic<std::uint64_t> sequence{0};
        for (;;) {
            m_temporary = temporaryPath(
                destination, sequence.fetch_add(1, std::memory_order_relaxed));
#ifdef _WIN32
            m_handle = CreateFileW(m_temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                   CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
            if (m_handle == INVALID_HANDLE_VALUE) {
                const DWORD error = GetLastError();
                if (error == ERROR_FILE_EXISTS || error == ERROR_ALREADY_EXISTS) continue;
                return;
            }
            m_ready = writeWindows(contents);
            CloseHandle(m_handle);
            m_handle = INVALID_HANDLE_VALUE;
#else
            m_fd = ::open(m_temporary.c_str(), O_CREAT | O_EXCL | O_WRONLY, 0600);
            if (m_fd < 0) {
                if (errno == EEXIST) continue;
                return;
            }
            m_ready = writePosix(contents) && ::fsync(m_fd) == 0;
            ::close(m_fd);
            m_fd = -1;
#endif
            return;
        }
    }

    ~AtomicConfigWriter()
    {
#ifdef _WIN32
        if (m_handle != INVALID_HANDLE_VALUE) CloseHandle(m_handle);
#else
        if (m_fd >= 0) ::close(m_fd);
#endif
        if (!m_temporary.empty()) {
            std::error_code error;
            fs::remove(m_temporary, error);
        }
    }

    AtomicConfigWriter(const AtomicConfigWriter &) = delete;
    AtomicConfigWriter &operator=(const AtomicConfigWriter &) = delete;

    CommitResult commit()
    {
        if (!m_ready) return CommitResult::NotReplaced;
#ifdef _WIN32
        if (!MoveFileExW(m_temporary.c_str(), m_destination.c_str(),
                         MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
            return CommitResult::NotReplaced;
        }
        m_temporary.clear();
#else
        int flags = O_RDONLY;
#ifdef O_DIRECTORY
        flags |= O_DIRECTORY;
#endif
#ifdef PKCONFIG_ENABLE_TEST_HOOKS
        const bool failParentOpen = g_commitFailureForTesting.load() ==
            static_cast<int>(PkConfigStore::CommitFailureForTesting::ParentOpen);
#else
        const bool failParentOpen = false;
#endif
        const int directory = failParentOpen
            ? -1 : ::open(directoryForFileOperations(m_destination).c_str(), flags);
        if (directory < 0) return CommitResult::NotReplaced;

        std::error_code error;
        fs::rename(m_temporary, m_destination, error);
        if (error) {
            ::close(directory);
            return CommitResult::NotReplaced;
        }
        // rename() is the logical commit point. The temporary pathname no
        // longer exists and the destination already names the new bytes.
        m_temporary.clear();
        int result;
#ifdef PKCONFIG_ENABLE_TEST_HOOKS
        if (g_commitFailureForTesting.load() ==
            static_cast<int>(PkConfigStore::CommitFailureForTesting::ParentFsync)) {
            result = -1;
        } else
#endif
        do {
            result = ::fsync(directory);
        } while (result < 0 && errno == EINTR);
        ::close(directory);
        if (result != 0) return CommitResult::ReplacedDurabilityUncertain;
#endif
        return CommitResult::Replaced;
    }

private:
#ifdef _WIN32
    bool writeWindows(const std::string &contents)
    {
        std::size_t written = 0;
        while (written < contents.size()) {
            const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(
                contents.size() - written, static_cast<std::size_t>(MAXDWORD)));
            DWORD actual = 0;
            if (!WriteFile(m_handle, contents.data() + written, chunk, &actual, nullptr) ||
                actual == 0) return false;
            written += actual;
        }
        return FlushFileBuffers(m_handle) != 0;
    }
    HANDLE m_handle = INVALID_HANDLE_VALUE;
#else
    bool writePosix(const std::string &contents)
    {
        std::size_t written = 0;
        while (written < contents.size()) {
            const ssize_t actual = ::write(m_fd, contents.data() + written,
                                           contents.size() - written);
            if (actual < 0 && errno == EINTR) continue;
            if (actual <= 0) return false;
            written += static_cast<std::size_t>(actual);
        }
        return true;
    }
    int m_fd = -1;
#endif
    fs::path m_destination;
    fs::path m_temporary;
    bool m_ready = false;
};

} // namespace

PkConfigStore &PkConfigStore::instance()
{
    static PkConfigStore store;
    return store;
}

#ifdef PKCONFIG_ENABLE_TEST_HOOKS
bool PkConfigStore::setConfigFilePathForTesting(const std::filesystem::path &path)
{
    if (g_storeConstructed || path.empty()) return false;
    g_testConfigFilePath = path;
    return true;
}

std::filesystem::path PkConfigStore::configFilePathForTesting()
{
    return instance().m_configPath;
}

std::filesystem::path PkConfigStore::defaultConfigFilePathForTesting()
{
    return defaultConfigFilePath();
}

std::filesystem::path PkConfigStore::defaultConfigLockFilePathForTesting()
{
    return lockFilePath(defaultConfigFilePath());
}

void PkConfigStore::setCommitFailureForTesting(CommitFailureForTesting failure)
{
    g_commitFailureForTesting.store(static_cast<int>(failure));
}
#endif

PkConfigStore::PkConfigStore()
    : m_configPath(configFilePath())
{
#ifdef PKCONFIG_ENABLE_TEST_HOOKS
    g_storeConstructed = true;
#endif
    std::error_code error;
    if (!fs::exists(m_configPath, error)) {
        m_persistentStateValid = !error;
        return;
    }
    if (error) {
        m_persistentStateValid = false;
        return;
    }
    ConfigFileLock lock(m_configPath);
    ParsedConfig parsed;
    m_persistentStateValid = lock.isLocked() && readConfig(m_configPath, &parsed);
    if (m_persistentStateValid) m_data = std::move(parsed.data);
}

PkConfigStore::~PkConfigStore()
{
    sync();
}

PkString PkConfigStore::get(const PkString &group, const PkString &key,
                            const PkString &fallback) const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    const auto groupIt = m_data.find(group);
    if (groupIt == m_data.end()) return fallback;
    const auto keyIt = groupIt->second.find(key);
    return keyIt == groupIt->second.end() ? fallback : keyIt->second;
}

void PkConfigStore::set(const PkString &group, const PkString &key,
                        const PkString &value)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_data[group][key] = value;
    m_pending.push_back({MutationKind::Set, group, key, value});
}

bool PkConfigStore::has(const PkString &group, const PkString &key) const
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    const auto groupIt = m_data.find(group);
    return groupIt != m_data.end() && groupIt->second.find(key) != groupIt->second.end();
}

void PkConfigStore::remove(const PkString &group, const PkString &key)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    auto groupIt = m_data.find(group);
    if (groupIt != m_data.end()) {
        groupIt->second.erase(key);
        if (groupIt->second.empty()) m_data.erase(groupIt);
    }
    m_pending.push_back({MutationKind::Remove, group, key, PkString()});
}

void PkConfigStore::clearGroup(const PkString &group)
{
    const std::lock_guard<std::mutex> lock(m_mutex);
    m_data.erase(group);
    m_pending.push_back({MutationKind::ClearGroup, group, PkString(), PkString()});
}

bool PkConfigStore::sync() noexcept
{
    try {
        const std::lock_guard<std::mutex> processLock(m_mutex);
        if (m_pending.empty()) return m_persistentStateValid;

        std::error_code error;
        const fs::path parent = m_configPath.parent_path();
        if (!parent.empty()) {
            fs::create_directories(parent, error);
            if (error) return false;
        }
        ConfigFileLock lock(m_configPath);
        if (!lock.isLocked()) return false;

        ParsedConfig parsed;
        if (!readConfig(m_configPath, &parsed)) {
            m_persistentStateValid = false;
            return false;
        }
        for (const Mutation &mutation : m_pending) {
            switch (mutation.kind) {
            case MutationKind::Set:
                parsed.data[mutation.group][mutation.key] = mutation.value;
                break;
            case MutationKind::Remove: {
                auto group = parsed.data.find(mutation.group);
                if (group != parsed.data.end()) {
                    group->second.erase(mutation.key);
                    if (group->second.empty()) parsed.data.erase(group);
                }
                break;
            }
            case MutationKind::ClearGroup:
                parsed.data.erase(mutation.group);
                break;
            }
        }

        std::string contents;
        if (!serializeConfig(parsed, &contents)) return false;
        AtomicConfigWriter writer(m_configPath, contents);
        const AtomicConfigWriter::CommitResult commitResult = writer.commit();
        if (commitResult == AtomicConfigWriter::CommitResult::NotReplaced) return false;
        m_data = parsed.data;
        m_pending.clear();
        m_persistentStateValid = true;
        return true;
    } catch (...) {
        return false;
    }
}
