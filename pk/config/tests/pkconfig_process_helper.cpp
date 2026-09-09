#include "../PkConfigGroup.h"
#include "../PkConfigStore.h"
#include "../PkSharedConfig.h"

#include <cstdlib>
#include <string>

namespace {

PkString fromUtf8(const char *text)
{
    const std::string value = text ? text : "";
    return PkString::PkFromUtf8(value.data(), static_cast<int>(value.size()));
}

bool setConfigRoot(const char *path)
{
#ifdef _WIN32
    return ::_putenv_s("APPDATA", path) == 0;
#else
    return ::setenv("XDG_CONFIG_HOME", path, 1) == 0;
#endif
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 6 || !setConfigRoot(argv[1])) {
        return 2;
    }

    const std::string mode = argv[2];
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
