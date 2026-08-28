#include "../PkImageFileDecoder.h"

#include <atomic>
#include <cstdlib>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <set>
#include <string>
#include <thread>
#include <vector>

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

PkImage solid(uint32_t argb)
{
    PkImage image(1, 1, PkImage::Format_ARGB32);
    image.setPixel(0, 0, argb);
    return image;
}

PkImageFileDecoderHandler markerHandler(std::string name,
                                        int priority,
                                        uint8_t marker,
                                        uint32_t result)
{
    return {
        std::move(name),
        priority,
        {"registry"},
        [marker](const uint8_t *data, std::size_t size, const std::string &) {
            return size > 0 && data && data[0] == marker;
        },
        [result](const uint8_t *, std::size_t, const std::string &) {
            return solid(result);
        }
    };
}

std::vector<uint8_t> readFixture(const std::string &name)
{
    std::ifstream input(std::string(PKIMAGE_TEST_DATA_DIR) + "/" + name, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
}

bool channelNear(uint32_t actual, uint32_t expected, unsigned shift, int tolerance)
{
    const int actualChannel = static_cast<int>((actual >> shift) & 0xFFu);
    const int expectedChannel = static_cast<int>((expected >> shift) & 0xFFu);
    return std::abs(actualChannel - expectedChannel) <= tolerance;
}

bool pixelNear(uint32_t actual, uint32_t expected, int tolerance)
{
    return channelNear(actual, expected, 24, 0) &&
           channelNear(actual, expected, 16, tolerance) &&
           channelNear(actual, expected, 8, tolerance) &&
           channelNear(actual, expected, 0, tolerance);
}

void duplicateNameKeepsOriginalHandler()
{
    const std::string name = "test.registry.duplicate";
    expect(PkImageFileDecoder::registerHandler(
               markerHandler(name, 100000, 'D', 0xFF112233u)),
           "first registration should succeed");
    expect(!PkImageFileDecoder::registerHandler(
                markerHandler(name, 200000, 'D', 0xFF445566u)),
           "duplicate handler name should be rejected");

    const uint8_t bytes[] = {'D'};
    const PkImage image = PkImageFileDecoder::decode(bytes, sizeof(bytes), "sample.registry");
    expect(!image.isNull(), "original duplicate handler should still decode");
    expect(image.pixel(0, 0) == 0xFF112233u,
           "duplicate registration must not replace original behavior");
}

void equalPriorityIsStable()
{
    expect(PkImageFileDecoder::registerHandler(
               markerHandler("test.registry.stable.first", 100100, 'S', 0xFF102030u)),
           "first equal-priority handler should register");
    expect(PkImageFileDecoder::registerHandler(
               markerHandler("test.registry.stable.second", 100100, 'S', 0xFF405060u)),
           "second equal-priority handler should register");

    const uint8_t bytes[] = {'S'};
    const PkImage image = PkImageFileDecoder::decode(bytes, sizeof(bytes));
    expect(image.pixel(0, 0) == 0xFF102030u,
           "equal priority must preserve registration order");
}

void contentSniffWinsOverContradictoryExtension()
{
    PkImageFileDecoderHandler extensionOnly = markerHandler(
        "test.registry.extension-hint", 100200, 'X', 0xFFFF0000u);
    extensionOnly.canDecode = [](const uint8_t *, std::size_t, const std::string &hint) {
        return hint.size() >= 5 && hint.substr(hint.size() - 5) == ".fake";
    };
    expect(PkImageFileDecoder::registerHandler(std::move(extensionOnly)),
           "extension-hint handler should register");
    expect(PkImageFileDecoder::registerHandler(
               markerHandler("test.registry.content", 100300, 'C', 0xFF00FF00u)),
           "content handler should register");

    const uint8_t bytes[] = {'C'};
    const PkImage image = PkImageFileDecoder::decode(bytes, sizeof(bytes), "contradiction.fake");
    expect(image.pixel(0, 0) == 0xFF00FF00u,
           "content-sniffing handler must outrank contradictory extension hint");
}

void callbackMayRegisterWithoutDeadlock()
{
    std::atomic<bool> nestedRegistered{false};
    PkImageFileDecoderHandler reentrant {
        "test.registry.reentrant.trigger",
        100400,
        {"registry"},
        [&nestedRegistered](const uint8_t *data, std::size_t size, const std::string &) {
            if (data && size > 0 && data[0] == 'R') {
                nestedRegistered.store(PkImageFileDecoder::registerHandler(
                    markerHandler("test.registry.reentrant.added", 100350, 'N', 0xFFABCDEFu)));
            }
            return false;
        },
        [](const uint8_t *, std::size_t, const std::string &) { return PkImage(); }
    };
    expect(PkImageFileDecoder::registerHandler(std::move(reentrant)),
           "reentrant trigger should register");

    const uint8_t trigger[] = {'R'};
    expect(PkImageFileDecoder::decode(trigger, sizeof(trigger)).isNull(),
           "trigger handler intentionally declines the first decode");
    expect(nestedRegistered.load(), "callback should register a new handler");

    const uint8_t nested[] = {'N'};
    const PkImage image = PkImageFileDecoder::decode(nested, sizeof(nested));
    expect(image.pixel(0, 0) == 0xFFABCDEFu,
           "handler registered reentrantly should serve the next snapshot");
}

void concurrentLoadAndRegistrationRemainConsistent()
{
    expect(PkImageFileDecoder::registerHandler(
               markerHandler("test.registry.concurrent.base", 100500, 'L', 0xFF2468ACu)),
           "concurrent base handler should register");

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "pkimage-file-decoder-concurrent.registry";
    {
        std::ofstream output(path, std::ios::binary | std::ios::trunc);
        output.put('L');
    }

    std::atomic<int> badLoads{0};
    std::vector<std::thread> readers;
    for (int threadIndex = 0; threadIndex < 4; ++threadIndex) {
        readers.emplace_back([&] {
            for (int i = 0; i < 250; ++i) {
                const PkImage image = PkImageFileDecoder::load(path.string());
                if (image.isNull() || image.pixel(0, 0) != 0xFF2468ACu) {
                    ++badLoads;
                }
            }
        });
    }

    std::thread registrar([] {
        for (int i = 0; i < 250; ++i) {
            PkImageFileDecoder::registerHandler(markerHandler(
                "test.registry.concurrent." + std::to_string(i), 10 + i, 'Z', 0xFFFFFFFFu));
        }
    });

    registrar.join();
    for (std::thread &reader : readers) {
        reader.join();
    }
    std::error_code ignored;
    std::filesystem::remove(path, ignored);

    expect(badLoads.load() == 0,
           "concurrent load/register must only expose complete registry snapshots");
    expect(PkImageFileDecoder::load(path.string()).isNull(),
           "missing file should return a null image");
}

void nativeFormatMatrix()
{
    struct FormatCase {
        const char *file;
        uint32_t first;
        uint32_t second;
        int tolerance;
    };
    const FormatCase cases[] = {
        {"valid.png",  0xFFFF0000u, 0x4000FF00u, 0},
        {"valid.jpg",  0xFF202020u, 0xFFE0E0E0u, 3},
        {"valid.tiff", 0xFFFF0000u, 0x800000FFu, 0},
        {"valid.gif",  0xFFFF0000u, 0x0000FF00u, 0},
        {"valid.webp", 0xFFFF0000u, 0x800000FFu, 0},
    };

    for (const FormatCase &test : cases) {
        const PkImage image = PkImageFileDecoder::load(
            std::string(PKIMAGE_TEST_DATA_DIR) + "/" + test.file);
        expect(!image.isNull(), (std::string(test.file) + " should decode").c_str());
        if (image.isNull()) {
            continue;
        }
        expect(image.width() == 2 && image.height() == 1,
               (std::string(test.file) + " should preserve 2x1 dimensions").c_str());
        expect(image.format() == PkImage::Format_ARGB32,
               (std::string(test.file) + " should produce ARGB32").c_str());
        expect(pixelNear(image.pixel(0, 0), test.first, test.tolerance),
               (std::string(test.file) + " first pixel mismatch").c_str());
        const uint32_t secondPixel = image.pixel(1, 0);
        expect(pixelNear(secondPixel, test.second, test.tolerance),
               (std::string(test.file) + " second pixel/alpha mismatch: actual=" +
                std::to_string(secondPixel)).c_str());
    }
}

void partialGifInitializesLogicalScreen()
{
    struct GifCase {
        const char *file;
        uint32_t outside;
        uint32_t inside;
    };
    const GifCase cases[] = {
        {"partial-opaque.gif", 0xFF123456u, 0xFFEF1020u},
        {"partial-transparent.gif", 0x00446688u, 0xFFEF1020u},
    };

    for (const GifCase &test : cases) {
        const PkImage image = PkImageFileDecoder::load(
            std::string(PKIMAGE_TEST_DATA_DIR) + "/" + test.file);
        expect(!image.isNull(), (std::string(test.file) + " should decode").c_str());
        if (image.isNull()) {
            continue;
        }
        expect(image.width() == 3 && image.height() == 2,
               (std::string(test.file) + " should preserve logical-screen dimensions").c_str());
        expect(image.pixel(0, 0) == test.outside,
               (std::string(test.file) + " outside pixel should use logical-screen fill").c_str());
        expect(image.pixel(1, 0) == test.inside,
               (std::string(test.file) + " descriptor pixel mismatch").c_str());
        expect(image.pixel(2, 1) == test.outside,
               (std::string(test.file) + " lower outside pixel should preserve fill").c_str());
    }
}

void corruptAndOversizeInputsReturnNull()
{
    for (const char *file : {"corrupt.png", "corrupt.jpg", "corrupt.tiff",
                             "corrupt.gif", "corrupt.webp", "oversize.png"}) {
        const PkImage image = PkImageFileDecoder::load(
            std::string(PKIMAGE_TEST_DATA_DIR) + "/" + file);
        expect(image.isNull(), (std::string(file) + " should return null").c_str());
    }

    const std::filesystem::path hugePath =
        std::filesystem::temp_directory_path() / "pkimage-file-decoder-oversize.bin";
    {
        std::ofstream output(hugePath, std::ios::binary | std::ios::trunc);
        output.put('x');
    }
    std::error_code error;
    std::filesystem::resize_file(hugePath, 512ull * 1024ull * 1024ull + 1ull, error);
    expect(!error, "sparse oversize fixture should be created");
    if (!error) {
        expect(PkImageFileDecoder::load(hugePath.string()).isNull(),
               "encoded file above the bounded-reader limit should return null");
    }
    std::filesystem::remove(hugePath, error);
}

void jpegUnsignedLongBoundaryIsRejectedWhereRepresentable()
{
    if constexpr (std::numeric_limits<std::size_t>::max() >
                  std::numeric_limits<unsigned long>::max()) {
        const uint8_t jpegLike[12] = {0xFFu, 0xD8u, 0xFFu};
        const std::size_t unrepresentable =
            static_cast<std::size_t>(std::numeric_limits<unsigned long>::max()) + 1u;
        expect(PkImageFileDecoder::decode(jpegLike, unrepresentable).isNull(),
               "JPEG size above unsigned long must be rejected before jpeg_mem_src");
    }
}

void nativeHandlersSniffBytesAndPublishExtensions()
{
    const std::vector<uint8_t> png = readFixture("valid.png");
    expect(!png.empty(), "PNG fixture should be readable");
    const PkImage contradicted = PkImageFileDecoder::decode(
        png.data(), png.size(), "actually-not-a-jpeg.jpg");
    expect(!contradicted.isNull() && contradicted.pixel(1, 0) == 0x4000FF00u,
           "PNG content should decode despite contradictory JPEG extension");

    const std::vector<std::string> extensions = PkImageFileDecoder::supportedExtensions();
    const std::set<std::string> supported(extensions.begin(), extensions.end());
    for (const char *extension : {"png", "jpg", "jpeg", "tif", "tiff", "gif", "webp"}) {
        expect(supported.count(extension) == 1,
               (std::string("supportedExtensions should contain ") + extension).c_str());
    }
}

} // namespace

int main()
{
    duplicateNameKeepsOriginalHandler();
    equalPriorityIsStable();
    contentSniffWinsOverContradictoryExtension();
    callbackMayRegisterWithoutDeadlock();
    concurrentLoadAndRegistrationRemainConsistent();
    nativeFormatMatrix();
    partialGifInitializesLogicalScreen();
    corruptAndOversizeInputsReturnNull();
    jpegUnsignedLongBoundaryIsRejectedWhereRepresentable();
    nativeHandlersSniffBytesAndPublishExtensions();

    if (failures == 0) {
        std::cout << "registry tests passed\n";
    }
    return failures == 0 ? 0 : 1;
}
