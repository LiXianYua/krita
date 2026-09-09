#include "../PkConfigGroup.h"
#include "../PkConfigStore.h"
#include "../PkSharedConfig.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

PkString fromUtf8(const char *text)
{
    const std::string value = text ? text : "";
    return PkString::PkFromUtf8(value.data(), static_cast<int>(value.size()));
}

bool setConfigRoot(const char *path)
{
    return PkConfigStore::setConfigFilePathForTesting(
        std::filesystem::u8path(path) / "kritarc");
}

std::string readBytes(const std::filesystem::path &path)
{
    std::ifstream input(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>());
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 6) {
        return 2;
    }

    std::string mode = argv[2];
#if !defined(_WIN32) && !defined(__APPLE__) && !defined(__ANDROID__) && !defined(__HAIKU__)
    if (mode == "write-default-path") {
        ::unsetenv("XDG_CONFIG_HOME");
        ::unsetenv("HOME");
        std::error_code error;
        std::filesystem::current_path(std::filesystem::u8path(argv[1]), error);
        if (error) return 2;
        mode = "write";
    } else
#endif
    if (!setConfigRoot(argv[1])) {
        return 2;
    }

    PkConfigGroup group = PkSharedConfig::openConfig()->group(fromUtf8(argv[3]));
    const PkString key = fromUtf8(argv[4]);
    const PkString value = fromUtf8(argv[5]);

    if (mode == "write") {
        group.writeEntry(key, value);
        return 0; // Production RAII teardown must make this durable.
    }
    if (mode == "write-int") {
        bool ok = false;
        const int integer = value.toInt(&ok);
        if (!ok) return 7;
        group.writeEntry(key, integer);
        return 0;
    }
    if (mode == "read") {
        return group.hasKey(key) && group.readEntry(key, PkString()) == value ? 0 : 3;
    }
    if (mode == "read-int") {
        bool ok = false;
        const int expected = value.toInt(&ok);
        return ok && group.hasKey(key) && group.readEntry(key, -1) == expected ? 0 : 8;
    }
    if (mode == "read-default") {
        return !group.hasKey(key) && group.readEntry(key, value) == value ? 0 : 4;
    }
    if (mode == "sync-failure") {
        group.writeEntry(key, value);
        const bool persisted = PkConfigStore::instance().sync();
        const bool retained = group.hasKey(key) && group.readEntry(key, PkString()) == value;
        return !persisted && retained ? 0 : 6;
    }
    if (mode == "write-sized" || mode == "read-sized" ||
        mode == "oversize-retry") {
        bool ok = false;
        const int requested = value.toInt(&ok);
        if (!ok || requested < 0) return 9;
        const std::string payload(static_cast<std::size_t>(requested), 'x');
        const PkString sized = fromUtf8(payload.c_str());
        if (mode == "write-sized") {
            group.writeEntry(key, sized);
            return PkConfigStore::instance().sync() ? 0 : 10;
        }
        if (mode == "read-sized") {
            return group.hasKey(key) &&
                    group.readEntry(key, PkString()).PkToUtf8().size() == payload.size()
                ? 0 : 11;
        }

        const std::filesystem::path path =
            PkConfigStore::configFilePathForTesting();
        const std::string before = readBytes(path);
        group.writeEntry(key, sized);
        const bool rejected = !PkConfigStore::instance().sync();
        const bool retained = group.hasKey(key) &&
            group.readEntry(key, PkString()).PkToUtf8().size() == payload.size();
        const bool unchanged = readBytes(path) == before;
        group.writeEntry(key, PkString("recovered"));
        const bool recovered = PkConfigStore::instance().sync();
        return rejected && retained && unchanged && recovered ? 0 : 12;
    }
    if (mode == "commit-failure") {
        const std::filesystem::path path =
            PkConfigStore::configFilePathForTesting();
        const std::string before = readBytes(path);
        group.writeEntry(key, value);
        const bool parentOpen = value == PkString("parent-open");
        PkConfigStore::setCommitFailureForTesting(
            parentOpen ? PkConfigStore::CommitFailureForTesting::ParentOpen
                       : PkConfigStore::CommitFailureForTesting::ParentFsync);
        const bool synced = PkConfigStore::instance().sync();
        const bool changed = readBytes(path) != before;
        const bool retained = group.hasKey(key) && group.readEntry(key, PkString()) == value;
        return parentOpen ? (!synced && !changed && retained ? 0 : 13)
                          : (synced && changed ? 0 : 14);
    }
    if (mode == "delete") {
        group.deleteEntry(key);
        return 0;
    }
    if (mode == "clear-group") {
        group.deleteGroup();
        return 0;
    }
    return 5;
}
