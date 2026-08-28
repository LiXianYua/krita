/* SPDX-License-Identifier: GPL-2.0-or-later */

#include <generator/kis_generator_registry.h>

#include <dlfcn.h>

#include <iostream>

namespace {

bool loadModule(const char *path)
{
    if (!dlopen(path, RTLD_NOW | RTLD_GLOBAL)) {
        std::cerr << "failed to load " << path << ": " << dlerror() << '\n';
        return false;
    }
    return true;
}

}

int main()
{
    const char *modules[] = {
        GENERATOR_GRADIENT_MODULE,
        GENERATOR_SOLID_MODULE,
        GENERATOR_PATTERN_MODULE,
        GENERATOR_SIMPLEX_MODULE,
        GENERATOR_SCREENTONE_MODULE,
        GENERATOR_MULTIGRID_MODULE,
        GENERATOR_SEEXPR_MODULE,
    };
    for (const char *module : modules) {
        if (!loadModule(module)) {
            return 1;
        }
    }

    KisGeneratorRegistry *registry = KisGeneratorRegistry::instance();
    const PkString ids[] = {
        "gradient", "color", "pattern", "simplex_noise",
        "screentone", "multigrid", "seexpr",
    };
    const PkList<PkString> keys = registry->keys();
    const PkList<KisGeneratorSP> duplicates = registry->doubleEntries();
    for (const PkString &id : ids) {
        if (!registry->get(id) || keys.count(id) != 1) {
            std::cerr << "generator id missing or non-unique: " << id.PkToUtf8() << '\n';
            return 1;
        }
        for (const KisGeneratorSP &duplicate : duplicates) {
            if (duplicate && duplicate->id() == id) {
                std::cerr << "duplicate generator registration: " << id.PkToUtf8() << '\n';
                return 1;
            }
        }
    }
    return 0;
}
