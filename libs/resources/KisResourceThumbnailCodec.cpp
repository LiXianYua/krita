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
    std::string pattern = (directory / ("." + target.filename().string() + ".tmp.XXXXXX")).string();
    std::vector<char> writablePattern(pattern.begin(), pattern.end());
    writablePattern.push_back('\0');
    const int descriptor = ::mkstemp(writablePattern.data());
    if (descriptor < 0) {
        return false;
    }
    temporary = fs::path(writablePattern.data());
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

} // namespace

namespace KisResourceThumbnailCodec
{

PkImage loadPng(const PkString &path)
{
    png_image pngImage{};
    pngImage.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&pngImage, path.PkToUtf8().c_str())) {
        png_image_free(&pngImage);
        return PkImage();
    }
    if (pngImage.width == 0 || pngImage.height == 0 ||
        pngImage.width > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
        pngImage.height > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
        pngImage.width > std::numeric_limits<std::size_t>::max() / 4u ||
        pngImage.height > std::numeric_limits<std::size_t>::max() /
            (static_cast<std::size_t>(pngImage.width) * 4u)) {
        png_image_free(&pngImage);
        return PkImage();
    }

    pngImage.format = PNG_FORMAT_RGBA;
    std::vector<png_byte> pixels(static_cast<std::size_t>(pngImage.width) *
                                 static_cast<std::size_t>(pngImage.height) * 4u);
    if (!png_image_finish_read(&pngImage, nullptr, pixels.data(), 0, nullptr)) {
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

PkImage decodePng(const PkByteArray &data)
{
    if (data.isEmpty()) {
        return PkImage();
    }

    png_image pngImage{};
    pngImage.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&pngImage,
                                          data.constData(),
                                          static_cast<std::size_t>(data.size()))) {
        png_image_free(&pngImage);
        return PkImage();
    }
    if (pngImage.width == 0 || pngImage.height == 0 ||
        pngImage.width > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
        pngImage.height > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
        pngImage.width > std::numeric_limits<std::size_t>::max() / 4u ||
        pngImage.height > std::numeric_limits<std::size_t>::max() /
            (static_cast<std::size_t>(pngImage.width) * 4u)) {
        png_image_free(&pngImage);
        return PkImage();
    }

    pngImage.format = PNG_FORMAT_RGBA;
    std::vector<png_byte> pixels(static_cast<std::size_t>(pngImage.width) *
                                 static_cast<std::size_t>(pngImage.height) * 4u);
    if (!png_image_finish_read(&pngImage, nullptr, pixels.data(), 0, nullptr)) {
        png_image_free(&pngImage);
        return PkImage();
    }

    PkImage image(static_cast<int>(pngImage.width),
                  static_cast<int>(pngImage.height),
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

PkByteArray encodePng(const PkImage &image)
{
    std::vector<png_byte> pixels;
    if (!convertToRgba(image, pixels)) {
        return PkByteArray();
    }

    png_image pngImage{};
    pngImage.version = PNG_IMAGE_VERSION;
    pngImage.width = static_cast<png_uint_32>(image.width());
    pngImage.height = static_cast<png_uint_32>(image.height());
    pngImage.format = PNG_FORMAT_RGBA;
    png_alloc_size_t byteCount = 0;
    if (!png_image_write_to_memory(&pngImage, nullptr, &byteCount, 0,
                                   pixels.data(), 0, nullptr)) {
        png_image_free(&pngImage);
        return PkByteArray();
    }
    std::vector<std::uint8_t> encoded(static_cast<std::size_t>(byteCount));
    if (!png_image_write_to_memory(&pngImage, encoded.data(), &byteCount, 0,
                                   pixels.data(), 0, nullptr)) {
        png_image_free(&pngImage);
        return PkByteArray();
    }
    png_image_free(&pngImage);
    encoded.resize(static_cast<std::size_t>(byteCount));
    return PkByteArray(encoded);
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
