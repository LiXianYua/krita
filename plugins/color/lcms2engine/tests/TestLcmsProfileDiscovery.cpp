#include "LcmsProfileDiscovery.h"
#include "LcmsStringUtils.h"

#include <PkString.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/types.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace
{
bool expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
    }
    return condition;
}

PkString fromPath(const fs::path &path)
{
    const std::string utf8 = path.generic_u8string();
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

std::vector<std::string> utf8Entries(const PkStringList &entries)
{
    std::vector<std::string> result;
    for (const PkString &entry : entries) {
        result.push_back(entry.PkToUtf8());
    }
    return result;
}

bool testProfileEntries()
{
    const fs::path root = fs::temp_directory_path()
        / ("kritalcms-profile-discovery-" + std::to_string(
#ifdef _WIN32
               1
#else
               static_cast<long long>(::getpid())
#endif
               ));
    std::error_code error;
    fs::remove_all(root, error);
    fs::create_directories(root);

    const std::vector<std::string> readableNames {
        "Zulu.icc",
        "zulu.icc",
        "\xC3\x84rger.icc",
        "\xC3\xA4pfel.icc",
        "keep.ICM",
        "PANHEXRO.ICM",
        "ctpctdmed.icc",
        "ignored.txt",
    };
    for (const std::string &name : readableNames) {
        std::ofstream(root / fs::u8path(name), std::ios::binary) << "profile";
    }
    fs::create_directory(root / "directory.icc");

    const fs::path unreadable = root / "unreadable.icc";
    std::ofstream(unreadable, std::ios::binary) << "profile";
#ifndef _WIN32
    fs::permissions(unreadable, fs::perms::none, fs::perm_options::replace);
#endif

    const std::vector<std::string> actual =
        utf8Entries(LcmsProfileDiscovery::profileEntries(fromPath(root)));
    std::vector<std::string> expected {
        "keep.ICM",
        "Zulu.icc",
        "zulu.icc",
        "\xC3\xA4pfel.icc",
        "\xC3\x84rger.icc",
    };

    std::ifstream readabilityProbe(unreadable, std::ios::binary);
    if (readabilityProbe.is_open()) {
        expected.insert(expected.begin() + 1, "unreadable.icc");
    }

#ifndef _WIN32
    fs::permissions(unreadable, fs::perms::owner_read | fs::perms::owner_write,
                    fs::perm_options::replace, error);
#endif
    fs::remove_all(root, error);
    return expect(actual == expected,
                  "profile discovery must preserve Unicode case-insensitive ordering, "
                  "case tie-breaks, blacklist, and readability filtering");
}

bool testMalformedWideDescription()
{
    const wchar_t malformed[] = {static_cast<wchar_t>(0xd800), L'X', L'\0'};
    return expect(LcmsStringUtils::fromWideString(malformed).PkToUtf8()
                      == std::string("\xEF\xBF\xBDX"),
                  "surrogate wchar_t values must become U+FFFD");
}

bool testHomePathSemantics()
{
#ifdef _WIN32
    return true;
#else
    const fs::path originalDirectory = fs::current_path();
    const fs::path isolatedDirectory = fs::temp_directory_path()
        / ("kritalcms-home-unset-" + std::to_string(static_cast<long long>(::getpid())));
    std::error_code error;
    fs::remove_all(isolatedDirectory, error);
    fs::create_directories(isolatedDirectory);
    fs::current_path(isolatedDirectory);

    const char *savedHome = std::getenv("HOME");
    const std::string savedHomeValue = savedHome ? savedHome : "";
    const bool hadHome = savedHome != nullptr;
    ::unsetenv("HOME");
    bool ok = expect(LcmsProfileDiscovery::homePath() == PkString("/"),
                     "HOME unset must resolve to the Unix root path");

    ::setenv("HOME", "", 1);
    ok = expect(LcmsProfileDiscovery::homePath() == PkString("/"),
                "empty HOME must resolve to the Unix root path") && ok;

    ::setenv("HOME", "/tmp/s09f-home/./nested/../profile///", 1);
    ok = expect(LcmsProfileDiscovery::homePath() == PkString("/tmp/s09f-home/profile"),
                "HOME must use QDir::cleanPath-equivalent normalization") && ok;

    if (hadHome) {
        ::setenv("HOME", savedHomeValue.c_str(), 1);
    } else {
        ::unsetenv("HOME");
    }

    fs::current_path(originalDirectory);
    fs::remove_all(isolatedDirectory, error);
    return ok;
#endif
}
} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && std::string(argv[1]) == "--home-unset") {
        return testHomePathSemantics() ? 0 : 1;
    }

    const bool ok = testProfileEntries() && testMalformedWideDescription()
        && testHomePathSemantics();
    return ok ? 0 : 1;
}
