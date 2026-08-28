/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisExiv2IODevice.h"
#include "kis_exiv2_common.h"

#include <dlfcn.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <PkStringHash.h>
#include <kis_meta_data_backend_registry.h>

namespace {

int fail(const char *message)
{
    std::cerr << message << '\n';
    return 1;
}

int testStablePathReference()
{
    const std::filesystem::path directory =
        std::filesystem::temp_directory_path() / "krita-metadata-path-lifetime";
    std::filesystem::create_directories(directory);
    const std::filesystem::path filePath = directory / "metadata.exv";
    std::ofstream(filePath).close();

    KisExiv2IODevice device(PkString(filePath.string().c_str()));
    const std::string *first = &device.path();
    const std::string firstValue = *first;
    const std::string *second = &device.path();

    std::vector<std::string> churn;
    for (int i = 0; i < 4096; ++i) {
        churn.emplace_back(256, static_cast<char>('a' + (i % 26)));
    }

    const bool stableAddress = first == second;
    const bool stableContent = *first == firstValue && *second == firstValue;
    std::filesystem::remove_all(directory);

    if (!stableAddress) {
        return fail("KisExiv2IODevice::path() did not return a stable reference");
    }
    if (!stableContent) {
        return fail("KisExiv2IODevice::path() reference content did not survive churn");
    }
    return 0;
}

int testExifDateFormatting()
{
    if (!formatExifDateTime(PkDateTime()).empty()) {
        std::cerr << "invalid EXIF date formatted as: "
                  << formatExifDateTime(PkDateTime()) << '\n';
        return 1;
    }

    const PkDateTime valid(PkDate(2024, 2, 3), PkTime(4, 5, 6));
    if (formatExifDateTime(valid) != "2024:02:03 04:05:06") {
        std::cerr << "valid EXIF date formatted as: "
                  << formatExifDateTime(valid) << '\n';
        return 1;
    }
    return 0;
}

int testBackendRegistration()
{
    const char *modules[] = {METADATA_EXIF_MODULE,
                             METADATA_IPTC_MODULE,
                             METADATA_XMP_MODULE};
    for (const char *module : modules) {
        if (!dlopen(module, RTLD_NOW | RTLD_GLOBAL)) {
            std::cerr << "failed to load metadata module " << module << ": "
                      << dlerror() << '\n';
            return 1;
        }
    }

    KisMetadataBackendRegistry *registry = KisMetadataBackendRegistry::instance();
    if (!registry->get(PkString("exif")) ||
        !registry->get(PkString("iptc")) ||
        !registry->get(PkString("xmp"))) {
        return fail("metadata modules did not register all three backends");
    }
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        return fail("usage: kis_metadata_regression_test path|date|registry");
    }
    const std::string testName(argv[1]);
    if (testName == "path") {
        return testStablePathReference();
    }
    if (testName == "date") {
        return testExifDateFormatting();
    }
    if (testName == "registry") {
        return testBackendRegistration();
    }
    return fail("unknown metadata regression test");
}
