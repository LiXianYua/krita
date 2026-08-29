/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KisResourceThumbnailCodec.h"

#include <png.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <limits>
#include <vector>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace {

namespace fs = std::filesystem;

constexpr std::size_t kMaxDecodedRgbaBytes = 256u * 1024u * 1024u;
constexpr std::size_t kMaxDecodedTextBytes = 256u * 1024u * 1024u;
constexpr std::size_t kPlainTextLimit = 256u;

struct MemoryReader {
    const png_byte *data = nullptr;
    std::size_t size = 0;
    std::size_t offset = 0;
};

void readMemory(png_structp png, png_bytep destination, png_size_t count)
{
    auto *reader = static_cast<MemoryReader *>(png_get_io_ptr(png));
    if (!reader || reader->offset > reader->size ||
        count > reader->size - reader->offset) {
        png_error(png, "truncated PNG input");
    }
    std::memcpy(destination, reader->data + reader->offset, count);
    reader->offset += count;
}

void writeMemory(png_structp png, png_bytep source, png_size_t count)
{
    auto *output = static_cast<std::vector<std::uint8_t> *>(png_get_io_ptr(png));
    if (!output) {
        png_error(png, "missing PNG output");
    }
    try {
        output->insert(output->end(), source, source + count);
    } catch (...) {
        png_error(png, "PNG output allocation failed");
    }
}

void flushMemory(png_structp)
{
}

bool isPlainAscii(const std::string &text)
{
    return text.size() <= kPlainTextLimit &&
        std::all_of(text.begin(), text.end(), [](unsigned char value) {
            return value >= 0x20 && value <= 0x7e;
        });
}

bool isValidKeyword(const std::string &keyword)
{
    return !keyword.empty() && keyword.size() <= 79 &&
        std::all_of(keyword.begin(), keyword.end(), [](unsigned char value) {
            return value >= 0x20 && value <= 0x7e;
        });
}

void collectText(png_structp png,
                 png_infop info,
                 PkMap<PkString, PkString> &result,
                 std::size_t &totalTextBytes)
{
    png_textp entries = nullptr;
    int count = 0;
    if (png_get_text(png, info, &entries, &count) <= 0) {
        return;
    }
    for (int i = 0; i < count; ++i) {
        if (!entries[i].key || !entries[i].text) {
            continue;
        }
        const bool internationalText =
            entries[i].compression == PNG_ITXT_COMPRESSION_NONE ||
            entries[i].compression == PNG_ITXT_COMPRESSION_zTXt;
        const std::size_t valueSize = internationalText
            ? entries[i].itxt_length : entries[i].text_length;
        if (valueSize > kMaxDecodedTextBytes - totalTextBytes ||
            valueSize > static_cast<std::size_t>(std::numeric_limits<int>::max())) {
            png_error(png, "decoded PNG text exceeds size limit");
        }
        totalTextBytes += valueSize;
        result.insert(PkString::PkFromUtf8(entries[i].key,
                                           static_cast<int>(std::strlen(entries[i].key))),
                      PkString::PkFromUtf8(entries[i].text,
                                           static_cast<int>(valueSize)));
    }
}

struct RgbaPixel {
    png_byte red = 0;
    png_byte green = 0;
    png_byte blue = 0;
    png_byte alpha = 0;
};

uint16_t read16(const uint8_t *source)
{
    uint16_t value = 0;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

uint32_t read32(const uint8_t *source)
{
    uint32_t value = 0;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

png_byte expand4(uint32_t value)
{
    return static_cast<png_byte>((value << 4) | value);
}

png_byte expand5(uint32_t value)
{
    return static_cast<png_byte>((value << 3) | (value >> 2));
}

png_byte expand6(uint32_t value)
{
    return static_cast<png_byte>((value << 2) | (value >> 4));
}

png_byte expand10(uint32_t value)
{
    return static_cast<png_byte>((value * 255u + 511u) / 1023u);
}

png_byte normalize16To8(uint32_t value)
{
    return static_cast<png_byte>((value * 255u + 32767u) / 65535u);
}

png_byte unpremultiply(png_byte component, png_byte alpha)
{
    if (alpha == 0) {
        return 0;
    }
    return static_cast<png_byte>(std::min(
        255u, (static_cast<unsigned int>(component) * 255u + alpha / 2u) / alpha));
}

uint32_t unpremultiplyNative(uint32_t component,
                             uint32_t componentMaximum,
                             uint32_t alpha,
                             uint32_t alphaMaximum)
{
    if (alpha == 0) {
        return 0;
    }
    const uint64_t straight =
        (static_cast<uint64_t>(component) * alphaMaximum + alpha / 2u) / alpha;
    return static_cast<uint32_t>(std::min<uint64_t>(componentMaximum, straight));
}

RgbaPixel straightArgb(uint32_t argb)
{
    return {static_cast<png_byte>((argb >> 16) & 0xffu),
            static_cast<png_byte>((argb >> 8) & 0xffu),
            static_cast<png_byte>(argb & 0xffu),
            static_cast<png_byte>((argb >> 24) & 0xffu)};
}

RgbaPixel premultiplied(png_byte red, png_byte green, png_byte blue, png_byte alpha)
{
    return {unpremultiply(red, alpha),
            unpremultiply(green, alpha),
            unpremultiply(blue, alpha),
            alpha};
}

bool readPixel(const PkImage &image, int x, int y, RgbaPixel &result)
{
    const uint8_t *row = image.constScanLine(y);
    if (!row) {
        return false;
    }

    switch (image.format()) {
    case PkImage::Format_Mono:
    case PkImage::Format_MonoLSB:
    case PkImage::Format_Indexed8:
        result = straightArgb(image.color(image.pixelIndex(x, y)));
        return true;
    case PkImage::Format_RGB32: {
        const uint32_t rgb = read32(row + static_cast<std::size_t>(x) * 4u);
        result = straightArgb(rgb | 0xff000000u);
        return true;
    }
    case PkImage::Format_ARGB32:
        result = straightArgb(read32(row + static_cast<std::size_t>(x) * 4u));
        return true;
    case PkImage::Format_ARGB32_Premultiplied: {
        const uint32_t argb = read32(row + static_cast<std::size_t>(x) * 4u);
        result = premultiplied(static_cast<png_byte>((argb >> 16) & 0xffu),
                               static_cast<png_byte>((argb >> 8) & 0xffu),
                               static_cast<png_byte>(argb & 0xffu),
                               static_cast<png_byte>((argb >> 24) & 0xffu));
        return true;
    }
    case PkImage::Format_RGB16: {
        const uint16_t rgb = read16(row + static_cast<std::size_t>(x) * 2u);
        result = {expand5((rgb >> 11) & 0x1fu),
                  expand6((rgb >> 5) & 0x3fu),
                  expand5(rgb & 0x1fu), 0xff};
        return true;
    }
    case PkImage::Format_ARGB8565_Premultiplied: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const uint16_t rgb = static_cast<uint16_t>((pixel[0] << 8) | pixel[1]);
        result = premultiplied(expand5((rgb >> 11) & 0x1fu),
                               expand6((rgb >> 5) & 0x3fu),
                               expand5(rgb & 0x1fu), pixel[2]);
        return true;
    }
    case PkImage::Format_RGB666: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const uint32_t rgb = (static_cast<uint32_t>(pixel[0]) << 16) |
                             (static_cast<uint32_t>(pixel[1]) << 8) | pixel[2];
        result = {expand6((rgb >> 12) & 0x3fu),
                  expand6((rgb >> 6) & 0x3fu),
                  expand6(rgb & 0x3fu), 0xff};
        return true;
    }
    case PkImage::Format_ARGB6666_Premultiplied: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const uint32_t argb = (static_cast<uint32_t>(pixel[0]) << 16) |
                              (static_cast<uint32_t>(pixel[1]) << 8) | pixel[2];
        result = premultiplied(expand6((argb >> 12) & 0x3fu),
                               expand6((argb >> 6) & 0x3fu),
                               expand6(argb & 0x3fu),
                               expand6((argb >> 18) & 0x3fu));
        return true;
    }
    case PkImage::Format_RGB555: {
        const uint16_t rgb = read16(row + static_cast<std::size_t>(x) * 2u);
        result = {expand5((rgb >> 10) & 0x1fu),
                  expand5((rgb >> 5) & 0x1fu),
                  expand5(rgb & 0x1fu), 0xff};
        return true;
    }
    case PkImage::Format_ARGB8555_Premultiplied: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const uint16_t rgb = static_cast<uint16_t>((pixel[0] << 8) | pixel[1]);
        result = premultiplied(expand5((rgb >> 10) & 0x1fu),
                               expand5((rgb >> 5) & 0x1fu),
                               expand5(rgb & 0x1fu), pixel[2]);
        return true;
    }
    case PkImage::Format_RGB888: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        result = {pixel[0], pixel[1], pixel[2], 0xff};
        return true;
    }
    case PkImage::Format_RGB444: {
        const uint16_t rgb = read16(row + static_cast<std::size_t>(x) * 2u);
        result = {expand4((rgb >> 8) & 0x0fu),
                  expand4((rgb >> 4) & 0x0fu),
                  expand4(rgb & 0x0fu), 0xff};
        return true;
    }
    case PkImage::Format_ARGB4444_Premultiplied: {
        const uint16_t argb = read16(row + static_cast<std::size_t>(x) * 2u);
        result = premultiplied(expand4((argb >> 8) & 0x0fu),
                               expand4((argb >> 4) & 0x0fu),
                               expand4(argb & 0x0fu),
                               expand4((argb >> 12) & 0x0fu));
        return true;
    }
    case PkImage::Format_RGBX8888: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 4u;
        result = {pixel[0], pixel[1], pixel[2], 0xff};
        return true;
    }
    case PkImage::Format_RGBA8888: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 4u;
        result = {pixel[0], pixel[1], pixel[2], pixel[3]};
        return true;
    }
    case PkImage::Format_RGBA8888_Premultiplied: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 4u;
        result = premultiplied(pixel[0], pixel[1], pixel[2], pixel[3]);
        return true;
    }
    case PkImage::Format_BGR30:
    case PkImage::Format_A2BGR30_Premultiplied:
    case PkImage::Format_RGB30:
    case PkImage::Format_A2RGB30_Premultiplied: {
        const uint32_t packed = read32(row + static_cast<std::size_t>(x) * 4u);
        const bool bgr = image.format() == PkImage::Format_BGR30 ||
                         image.format() == PkImage::Format_A2BGR30_Premultiplied;
        const bool hasAlpha = image.format() == PkImage::Format_A2BGR30_Premultiplied ||
                              image.format() == PkImage::Format_A2RGB30_Premultiplied;
        uint32_t first = (packed >> 20) & 0x3ffu;
        uint32_t green = (packed >> 10) & 0x3ffu;
        uint32_t last = packed & 0x3ffu;
        const uint32_t alpha = (packed >> 30) & 0x3u;
        if (hasAlpha) {
            first = unpremultiplyNative(first, 0x3ffu, alpha, 0x3u);
            green = unpremultiplyNative(green, 0x3ffu, alpha, 0x3u);
            last = unpremultiplyNative(last, 0x3ffu, alpha, 0x3u);
        }
        const png_byte red = expand10(bgr ? last : first);
        const png_byte blue = expand10(bgr ? first : last);
        result = {red, expand10(green), blue,
                  hasAlpha ? static_cast<png_byte>(alpha * 85u) : static_cast<png_byte>(0xff)};
        return true;
    }
    case PkImage::Format_Alpha8:
        result = {0, 0, 0, row[x]};
        return true;
    case PkImage::Format_Grayscale8:
        result = {row[x], row[x], row[x], 0xff};
        return true;
    case PkImage::Format_RGBX64:
    case PkImage::Format_RGBA64:
    case PkImage::Format_RGBA64_Premultiplied: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 8u;
        uint32_t red = read16(pixel);
        uint32_t green = read16(pixel + 2);
        uint32_t blue = read16(pixel + 4);
        const uint32_t alpha16 = image.format() == PkImage::Format_RGBX64
            ? 0xffffu : read16(pixel + 6);
        if (image.format() == PkImage::Format_RGBA64_Premultiplied) {
            red = unpremultiplyNative(red, 0xffffu, alpha16, 0xffffu);
            green = unpremultiplyNative(green, 0xffffu, alpha16, 0xffffu);
            blue = unpremultiplyNative(blue, 0xffffu, alpha16, 0xffffu);
        }
        result = {normalize16To8(red),
                  normalize16To8(green),
                  normalize16To8(blue),
                  normalize16To8(alpha16)};
        return true;
    }
    case PkImage::Format_Grayscale16: {
        const png_byte gray = normalize16To8(
            read16(row + static_cast<std::size_t>(x) * 2u));
        result = {gray, gray, gray, 0xff};
        return true;
    }
    case PkImage::Format_BGR888: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        result = {pixel[2], pixel[1], pixel[0], 0xff};
        return true;
    }
    case PkImage::Format_Invalid:
        return false;
    }
    return false;
}

bool convertToRgba(const PkImage &image, std::vector<png_byte> &pixels)
{
    if (image.isNull() || image.width() > std::numeric_limits<int>::max() / 4) {
        return false;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(image.width()) * 4u;
    if (static_cast<std::size_t>(image.height()) >
        std::numeric_limits<std::size_t>::max() / rowBytes) {
        return false;
    }
    pixels.resize(rowBytes * static_cast<std::size_t>(image.height()));
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            RgbaPixel pixel;
            if (!readPixel(image, x, y, pixel)) {
                return false;
            }
            const std::size_t offset = static_cast<std::size_t>(y) * rowBytes +
                                       static_cast<std::size_t>(x) * 4u;
            pixels[offset] = pixel.red;
            pixels[offset + 1] = pixel.green;
            pixels[offset + 2] = pixel.blue;
            pixels[offset + 3] = pixel.alpha;
        }
    }
    return true;
}

bool decodedRgbaSize(const png_image &image, std::size_t &byteCount)
{
    if (image.width == 0 || image.height == 0 ||
        image.width > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
        image.height > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
        image.width > std::numeric_limits<std::size_t>::max() / 4u) {
        return false;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(image.width) * 4u;
    if (image.height > std::numeric_limits<std::size_t>::max() / rowBytes) {
        return false;
    }
    byteCount = rowBytes * static_cast<std::size_t>(image.height);
    return byteCount <= kMaxDecodedRgbaBytes;
}

bool createTemporaryFile(const fs::path &target, fs::path &temporary, FILE *&file)
{
    const fs::path directory = target.has_parent_path() ? target.parent_path() : fs::path(".");
#ifdef _WIN32
    static std::atomic<unsigned long long> sequence{0};
    for (int attempt = 0; attempt < 128; ++attempt) {
        const std::wstring name = L"." + target.filename().wstring() + L".tmp." +
            std::to_wstring(::GetCurrentProcessId()) + L"." +
            std::to_wstring(sequence.fetch_add(1, std::memory_order_relaxed));
        temporary = directory / name;
        const HANDLE handle = ::CreateFileW(temporary.c_str(), GENERIC_WRITE, 0, nullptr,
                                             CREATE_NEW, FILE_ATTRIBUTE_TEMPORARY, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            if (::GetLastError() == ERROR_FILE_EXISTS ||
                ::GetLastError() == ERROR_ALREADY_EXISTS) {
                continue;
            }
            temporary.clear();
            return false;
        }
        const int descriptor = ::_open_osfhandle(reinterpret_cast<intptr_t>(handle),
                                                  _O_BINARY | _O_WRONLY);
        if (descriptor < 0) {
            ::CloseHandle(handle);
            std::error_code ec;
            fs::remove(temporary, ec);
            temporary.clear();
            return false;
        }
        file = ::_fdopen(descriptor, "wb");
        if (!file) {
            ::_close(descriptor);
            std::error_code ec;
            fs::remove(temporary, ec);
            temporary.clear();
            return false;
        }
        return true;
    }
    temporary.clear();
    return false;
#else
    std::string pattern = (directory / ("." + target.filename().u8string() + ".tmp.XXXXXX")).u8string();
    std::vector<char> writablePattern(pattern.begin(), pattern.end());
    writablePattern.push_back('\0');
    const int descriptor = ::mkstemp(writablePattern.data());
    if (descriptor < 0) {
        return false;
    }
    temporary = fs::u8path(writablePattern.data());
    file = ::fdopen(descriptor, "wb");
    if (!file) {
        ::close(descriptor);
        std::error_code ec;
        fs::remove(temporary, ec);
        temporary.clear();
        return false;
    }
    return true;
#endif
}

bool flushFile(FILE *file)
{
    if (std::fflush(file) != 0) {
        return false;
    }
#ifdef _WIN32
    const intptr_t nativeHandle = ::_get_osfhandle(::_fileno(file));
    return nativeHandle != -1 &&
        ::FlushFileBuffers(reinterpret_cast<HANDLE>(nativeHandle)) != 0;
#else
    return ::fsync(::fileno(file)) == 0;
#endif
}

bool publishIfAbsent(const fs::path &temporary, const fs::path &target)
{
#ifdef _WIN32
    return ::MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
#else
    if (::link(temporary.c_str(), target.c_str()) != 0) {
        return false;
    }
    return ::unlink(temporary.c_str()) == 0;
#endif
}

FILE *openFileForRead(const fs::path &path)
{
#ifdef _WIN32
    return ::_wfopen(path.c_str(), L"rb");
#else
    return std::fopen(path.c_str(), "rb");
#endif
}

} // namespace

namespace KisResourceThumbnailCodec
{

PkImage loadPng(const PkString &path)
{
    FILE *file = openFileForRead(fs::u8path(path.PkToUtf8()));
    if (!file) {
        return PkImage();
    }

    png_image pngImage{};
    pngImage.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_stdio(&pngImage, file)) {
        png_image_free(&pngImage);
        std::fclose(file);
        return PkImage();
    }
    std::size_t byteCount = 0;
    if (!decodedRgbaSize(pngImage, byteCount)) {
        png_image_free(&pngImage);
        std::fclose(file);
        return PkImage();
    }

    pngImage.format = PNG_FORMAT_RGBA;
    std::vector<png_byte> pixels(byteCount);
    const bool read = png_image_finish_read(&pngImage, nullptr, pixels.data(), 0, nullptr) != 0;
    const bool closed = std::fclose(file) == 0;
    if (!read || !closed) {
        png_image_free(&pngImage);
        return PkImage();
    }

    PkImage image(static_cast<int>(pngImage.width), static_cast<int>(pngImage.height),
                  PkImage::Format_ARGB32);
    for (png_uint_32 y = 0; y < pngImage.height; ++y) {
        for (png_uint_32 x = 0; x < pngImage.width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * pngImage.width + x) * 4u;
            image.setPixel(static_cast<int>(x), static_cast<int>(y),
                           (static_cast<uint32_t>(pixels[offset + 3]) << 24) |
                           (static_cast<uint32_t>(pixels[offset]) << 16) |
                           (static_cast<uint32_t>(pixels[offset + 1]) << 8) |
                           pixels[offset + 2]);
        }
    }
    png_image_free(&pngImage);
    return image;
}

bool decodePng(const PkByteArray &data, PngPayload &payload)
{
    payload = PngPayload();
    if (data.isEmpty()) {
        return false;
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        return false;
    }
    png_infop info = png_create_info_struct(png);
    png_infop endInfo = png_create_info_struct(png);
    if (!info || !endInfo) {
        png_destroy_read_struct(&png, info ? &info : nullptr, endInfo ? &endInfo : nullptr);
        return false;
    }

    MemoryReader reader{
        reinterpret_cast<const png_byte *>(data.constData()),
        static_cast<std::size_t>(data.size()), 0};
    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bitDepth = 0;
    int colorType = 0;
    png_image sizeImage{};
    std::size_t byteCount = 0;
    std::vector<png_byte> pixels;
    std::vector<png_bytep> rows;
    PngPayload decoded;
    std::size_t totalTextBytes = 0;

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, &endInfo);
        payload = PngPayload();
        return false;
    }

    png_set_read_fn(png, &reader, readMemory);
    png_read_info(png, info);

    if (!png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType,
                      nullptr, nullptr, nullptr)) {
        png_error(png, "missing PNG header");
    }

    sizeImage.width = width;
    sizeImage.height = height;
    if (!decodedRgbaSize(sizeImage, byteCount)) {
        png_error(png, "decoded PNG exceeds size limit");
    }

    if (bitDepth == 16) {
        png_set_strip_16(png);
    }
    if (colorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    }
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }
    const bool hasTransparency = png_get_valid(png, info, PNG_INFO_tRNS) != 0;
    if (hasTransparency) {
        png_set_tRNS_to_alpha(png);
    }
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if (!(colorType & PNG_COLOR_MASK_ALPHA) && !hasTransparency) {
        png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
    }
    png_read_update_info(png, info);
    if (png_get_bit_depth(png, info) != 8 || png_get_channels(png, info) != 4 ||
        png_get_rowbytes(png, info) != static_cast<png_size_t>(width) * 4u) {
        png_error(png, "unsupported normalized PNG layout");
    }

    pixels.resize(byteCount);
    rows.resize(static_cast<std::size_t>(height));
    for (png_uint_32 y = 0; y < height; ++y) {
        rows[static_cast<std::size_t>(y)] =
            pixels.data() + static_cast<std::size_t>(y) * width * 4u;
    }
    png_read_image(png, rows.data());
    png_read_end(png, endInfo);

    collectText(png, info, decoded.text, totalTextBytes);
    collectText(png, endInfo, decoded.text, totalTextBytes);
    decoded.image = PkImage(static_cast<int>(width), static_cast<int>(height),
                            PkImage::Format_ARGB32);
    if (decoded.image.isNull()) {
        png_destroy_read_struct(&png, &info, &endInfo);
        return false;
    }
    for (png_uint_32 y = 0; y < height; ++y) {
        for (png_uint_32 x = 0; x < width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * width + x) * 4u;
            decoded.image.setPixel(
                static_cast<int>(x), static_cast<int>(y),
                (static_cast<uint32_t>(pixels[offset + 3]) << 24) |
                (static_cast<uint32_t>(pixels[offset]) << 16) |
                (static_cast<uint32_t>(pixels[offset + 1]) << 8) |
                pixels[offset + 2]);
        }
    }
    png_destroy_read_struct(&png, &info, &endInfo);
    payload = std::move(decoded);
    return true;
}

PkImage decodePng(const PkByteArray &data)
{
    PngPayload payload;
    return decodePng(data, payload) ? payload.image : PkImage();
}

PkByteArray encodePng(const PkImage &image,
                      const PkMap<PkString, PkString> &text)
{
    std::vector<png_byte> pixels;
    if (!convertToRgba(image, pixels)) {
        return PkByteArray();
    }

    std::vector<std::string> keys;
    std::vector<std::string> values;
    keys.reserve(static_cast<std::size_t>(text.size()));
    values.reserve(static_cast<std::size_t>(text.size()));
    for (auto it = text.constBegin(); it != text.constEnd(); ++it) {
        keys.push_back(it.key().PkToUtf8());
        values.push_back(it.value().PkToUtf8());
        if (!isValidKeyword(keys.back())) {
            return PkByteArray();
        }
    }

    std::vector<png_text> entries(static_cast<std::size_t>(text.size()));
    for (std::size_t i = 0; i < entries.size(); ++i) {
        png_text &entry = entries[i];
        entry.key = const_cast<char *>(keys[i].c_str());
        entry.text = const_cast<char *>(values[i].data());
        if (isPlainAscii(values[i])) {
            entry.compression = PNG_TEXT_COMPRESSION_NONE;
            entry.text_length = values[i].size();
        } else {
            entry.compression = PNG_ITXT_COMPRESSION_zTXt;
            entry.itxt_length = values[i].size();
        }
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) {
        return PkByteArray();
    }
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        return PkByteArray();
    }

    std::vector<std::uint8_t> encoded;
    std::vector<png_bytep> rows(static_cast<std::size_t>(image.height()));
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return PkByteArray();
    }
    png_set_write_fn(png, &encoded, writeMemory, flushMemory);
    png_set_IHDR(png, info,
                 static_cast<png_uint_32>(image.width()),
                 static_cast<png_uint_32>(image.height()),
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    if (!entries.empty()) {
        png_set_text(png, info, entries.data(), static_cast<int>(entries.size()));
    }
    png_write_info(png, info);
    const std::size_t rowBytes = static_cast<std::size_t>(image.width()) * 4u;
    for (int y = 0; y < image.height(); ++y) {
        rows[static_cast<std::size_t>(y)] =
            pixels.data() + static_cast<std::size_t>(y) * rowBytes;
    }
    png_write_image(png, rows.data());
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    return PkByteArray(encoded);
}

PkByteArray encodePng(const PkImage &image)
{
    return encodePng(image, PkMap<PkString, PkString>());
}

bool savePng(const PkString &path, const PkImage &image)
{
    std::vector<png_byte> pixels;
    if (!convertToRgba(image, pixels)) {
        return false;
    }

    const fs::path target = fs::u8path(path.PkToUtf8());
    fs::path temporary;
    FILE *file = nullptr;
    if (!createTemporaryFile(target, temporary, file)) {
        return false;
    }

    png_image pngImage{};
    pngImage.version = PNG_IMAGE_VERSION;
    pngImage.width = static_cast<png_uint_32>(image.width());
    pngImage.height = static_cast<png_uint_32>(image.height());
    pngImage.format = PNG_FORMAT_RGBA;
    const bool wrote = png_image_write_to_stdio(&pngImage, file, 0, pixels.data(), 0, nullptr) != 0;
    png_image_free(&pngImage);
    const bool flushed = wrote && flushFile(file);
    const bool closed = std::fclose(file) == 0;
    if (!wrote || !flushed || !closed || !publishIfAbsent(temporary, target)) {
        std::error_code ec;
        fs::remove(temporary, ec);
        return false;
    }
    return true;
}

} // namespace KisResourceThumbnailCodec
