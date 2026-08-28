/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisPkImageFileDecoderAdapter.h"

#include <KisDocument.h>
#include <KisImportExportFilter.h>
#include <KisImportExportManager.h>
#include <KisSprayShapeOptionData.h>
#include <PkImageFileDecoder.h>
#include <PkThread.h>
#include <PkThreadCallQueue.h>
#include <kis_image.h>
#include <kis_properties_configuration.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#if defined(KIS_PK_ADAPTER_DSO_TEST)
#include <dlfcn.h>
#endif

namespace
{

int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class TestImportFilter final : public KisImportExportFilter
{
public:
    enum class Result {
        ExactImage,
        EmptyDocument,
    };

    explicit TestImportFilter(Result result)
        : m_result(result)
    {
    }

    KisImportExportErrorCode convert(KisDocument *document,
                                     PkStream *,
                                     KisPropertiesConfigurationSP) override
    {
        if (m_result == Result::EmptyDocument) {
            return ImportExportCodes::OK;
        }

        PkImage source(2, 1, PkImage::Format_ARGB32);
        source.setPixel(0, 0, 0xffff0000U);
        source.setPixel(1, 0, 0x4000ff00U);
        document->setCurrentImage(KisImage::fromQImage(source, document->createUndoStore()));
        return ImportExportCodes::OK;
    }

private:
    Result m_result;
};

void registerHdrFilter(int weight, TestImportFilter::Result result)
{
    KisImportExportManager::registerFilter({
        PkStringList() << PkString("image/vnd.radiance"),
        {},
        weight,
        [result]() -> KisImportExportFilter * {
            return new TestImportFilter(result);
        },
    });
}

bool exactPixels(const PkImage &image)
{
    return !image.isNull() && image.width() == 2 && image.height() == 1
        && image.format() == PkImage::Format_ARGB32
        && image.pixel(0, 0) == 0xffff0000U
        && image.pixel(1, 0) == 0x4000ff00U;
}

class TemporaryFixture
{
public:
    TemporaryFixture(const char *stem, const char *extension)
    {
        static int serial = 0;
        m_path = std::filesystem::temp_directory_path()
            / (std::string("kis-pk-decoder-") + stem + "-"
               + std::to_string(++serial) + extension);
        std::ofstream output(m_path, std::ios::binary);
        const std::uint8_t bytes[] = {0x50, 0x4b, 0x2d, 0x49, 0x4d, 0x50, 0x45, 0x58};
        output.write(reinterpret_cast<const char *>(bytes), sizeof(bytes));
    }

    ~TemporaryFixture()
    {
        std::error_code error;
        std::filesystem::remove(m_path, error);
    }

    std::string string() const { return m_path.string(); }

private:
    std::filesystem::path m_path;
};

bool initializeAdapter(int argc, char **argv)
{
#if defined(KIS_PK_ADAPTER_DSO_TEST)
    if (argc != 2) {
        std::cerr << "FAIL: adapter DSO path argument missing\n";
        return false;
    }
    void *handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!handle) {
        std::cerr << "FAIL: adapter DSO did not load: " << dlerror() << '\n';
        return false;
    }
    dlerror();
    auto ensure = reinterpret_cast<void (*)()>(
        dlsym(handle, "kisEnsurePkImageFileDecoderAdapterRegistered"));
    const char *error = dlerror();
    if (error || !ensure) {
        std::cerr << "FAIL: adapter registration anchor missing: "
                  << (error ? error : "null symbol") << '\n';
        dlclose(handle);
        return false;
    }
    ensure();
    dlclose(handle);
#else
    (void)argc;
    (void)argv;
    kisEnsurePkImageFileDecoderAdapterRegistered();
#endif
    return true;
}

} // namespace

int runAdapterTests(int argc, char **argv)
{
    PkThread::registerMainThread();
    PkThreadCallQueue::warmUpCurrentThread();

    if (!initializeAdapter(argc, argv)) {
        return 1;
    }

    TemporaryFixture hdr("registered", ".hdr");
    TemporaryFixture gap("gap", ".ani");

    expect(PkImageFileDecoder::load(hdr.string()).isNull(),
           "recognized exotic format without a registered filter must be null");
    expect(PkImageFileDecoder::load(gap.string()).isNull(),
           "exotic extension without a mapped import format must be a graceful GAP");

    registerHdrFilter(10, TestImportFilter::Result::EmptyDocument);
    expect(PkImageFileDecoder::load(hdr.string()).isNull(),
           "successful import that leaves the document empty must be null");

    registerHdrFilter(20, TestImportFilter::Result::ExactImage);
    const PkImage decoded = PkImageFileDecoder::load(hdr.string());
    expect(exactPixels(decoded),
           "registered exotic import must return exact unpremultiplied ARGB32 pixels");

    KisPropertiesConfiguration settings;
    settings.setProperty(PkString("SprayShape/imageUrl"), PkString(hdr.string().c_str()));
    KisSprayShapeOptionData spray;
    expect(spray.read(&settings), "Spray shape settings read must succeed");
    expect(exactPixels(spray.image),
           "Spray read path must preserve a configured non-PNG exotic image");

    if (failures == 0) {
        std::cout << "adapter exact pixels, graceful failures, and Spray HDR read passed\n";
    }
    return failures == 0 ? 0 : 1;
}

int main(int argc, char **argv)
{
    const int result = runAdapterTests(argc, argv);
#if defined(KIS_PK_ADAPTER_DSO_TEST)
    std::cout.flush();
    std::cerr.flush();

    // The shell providers intentionally aggregate static archives whose
    // process-global logging singletons have no cross-DSO destruction order.
    // All test-owned objects have already been destroyed by runAdapterTests;
    // skip only those unrelated provider globals at process teardown.
    std::_Exit(result);
#else
    return result;
#endif
}
