/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ImageShapePngData.h"

#include <png.h>

#include <cstddef>
#include <cstdio>
#include <jpeglib.h>
#include <tiffio.h>

#include <algorithm>
#include <array>
#include <csetjmp>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace
{
constexpr std::size_t kMaxDecodedRgbaBytes = 256u * 1024u * 1024u;
constexpr std::size_t kMaxDecodedCompressedBytes = 256u * 1024u * 1024u;
constexpr char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

struct Rgba8 {
    std::uint8_t red = 0;
    std::uint8_t green = 0;
    std::uint8_t blue = 0;
    std::uint8_t alpha = 0;
};

std::uint16_t read16(const std::uint8_t *source)
{
    std::uint16_t value = 0;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

std::uint32_t read32(const std::uint8_t *source)
{
    std::uint32_t value = 0;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

std::uint8_t expand4(std::uint32_t value) { return static_cast<std::uint8_t>((value << 4) | value); }
std::uint8_t expand5(std::uint32_t value) { return static_cast<std::uint8_t>((value << 3) | (value >> 2)); }
std::uint8_t expand6(std::uint32_t value) { return static_cast<std::uint8_t>((value << 2) | (value >> 4)); }
std::uint8_t expand10(std::uint32_t value)
{
    return static_cast<std::uint8_t>((value * 255u + 511u) / 1023u);
}

std::uint32_t unpremultiplyNative(std::uint32_t component,
                                  std::uint32_t componentMaximum,
                                  std::uint32_t alpha,
                                  std::uint32_t alphaMaximum)
{
    if (alpha == 0) return 0;
    const std::uint64_t straight =
        (static_cast<std::uint64_t>(component) * alphaMaximum + alpha / 2u) / alpha;
    return static_cast<std::uint32_t>(std::min<std::uint64_t>(componentMaximum, straight));
}

std::uint8_t unpremultiply8(std::uint8_t component, std::uint8_t alpha)
{
    return static_cast<std::uint8_t>(unpremultiplyNative(component, 255u, alpha, 255u));
}

Rgba8 straightArgb(std::uint32_t argb)
{
    return {static_cast<std::uint8_t>((argb >> 16) & 0xffu),
            static_cast<std::uint8_t>((argb >> 8) & 0xffu),
            static_cast<std::uint8_t>(argb & 0xffu),
            static_cast<std::uint8_t>((argb >> 24) & 0xffu)};
}

Rgba8 premultiplied(std::uint8_t red, std::uint8_t green,
                    std::uint8_t blue, std::uint8_t alpha)
{
    return {unpremultiply8(red, alpha), unpremultiply8(green, alpha),
            unpremultiply8(blue, alpha), alpha};
}

bool readPixel8(const PkImage &image, int x, int y, Rgba8 &result)
{
    const std::uint8_t *row = image.constScanLine(y);
    if (!row) return false;
    switch (image.format()) {
    case PkImage::Format_Mono:
    case PkImage::Format_MonoLSB:
    case PkImage::Format_Indexed8:
        result = straightArgb(image.color(image.pixelIndex(x, y)));
        return true;
    case PkImage::Format_RGB32:
        result = straightArgb(read32(row + std::size_t(x) * 4u) | 0xff000000u);
        return true;
    case PkImage::Format_ARGB32:
        result = straightArgb(read32(row + std::size_t(x) * 4u));
        return true;
    case PkImage::Format_ARGB32_Premultiplied: {
        const std::uint32_t value = read32(row + std::size_t(x) * 4u);
        result = premultiplied((value >> 16) & 0xffu, (value >> 8) & 0xffu,
                               value & 0xffu, (value >> 24) & 0xffu);
        return true;
    }
    case PkImage::Format_RGB16: {
        const std::uint16_t value = read16(row + std::size_t(x) * 2u);
        result = {expand5(value >> 11), expand6((value >> 5) & 0x3fu), expand5(value & 0x1fu), 255};
        return true;
    }
    case PkImage::Format_ARGB8565_Premultiplied:
    case PkImage::Format_ARGB8555_Premultiplied: {
        const std::uint8_t *pixel = row + std::size_t(x) * 3u;
        const std::uint16_t value = static_cast<std::uint16_t>((pixel[0] << 8) | pixel[1]);
        const bool sixGreen = image.format() == PkImage::Format_ARGB8565_Premultiplied;
        result = premultiplied(expand5(value >> (sixGreen ? 11 : 10)),
                               sixGreen ? expand6((value >> 5) & 0x3fu) : expand5((value >> 5) & 0x1fu),
                               expand5(value & 0x1fu), pixel[2]);
        return true;
    }
    case PkImage::Format_RGB666:
    case PkImage::Format_ARGB6666_Premultiplied: {
        const std::uint8_t *pixel = row + std::size_t(x) * 3u;
        const std::uint32_t value = (std::uint32_t(pixel[0]) << 16) |
                                    (std::uint32_t(pixel[1]) << 8) | pixel[2];
        const std::uint8_t red = expand6((value >> 12) & 0x3fu);
        const std::uint8_t green = expand6((value >> 6) & 0x3fu);
        const std::uint8_t blue = expand6(value & 0x3fu);
        if (image.format() == PkImage::Format_ARGB6666_Premultiplied) {
            result = premultiplied(red, green, blue, expand6((value >> 18) & 0x3fu));
        } else {
            result = {red, green, blue, 255};
        }
        return true;
    }
    case PkImage::Format_RGB555: {
        const std::uint16_t value = read16(row + std::size_t(x) * 2u);
        result = {expand5(value >> 10), expand5((value >> 5) & 0x1fu), expand5(value & 0x1fu), 255};
        return true;
    }
    case PkImage::Format_RGB888:
    case PkImage::Format_BGR888: {
        const std::uint8_t *pixel = row + std::size_t(x) * 3u;
        result = image.format() == PkImage::Format_RGB888
            ? Rgba8{pixel[0], pixel[1], pixel[2], 255}
            : Rgba8{pixel[2], pixel[1], pixel[0], 255};
        return true;
    }
    case PkImage::Format_RGB444:
    case PkImage::Format_ARGB4444_Premultiplied: {
        const std::uint16_t value = read16(row + std::size_t(x) * 2u);
        const std::uint8_t red = expand4((value >> 8) & 0xfu);
        const std::uint8_t green = expand4((value >> 4) & 0xfu);
        const std::uint8_t blue = expand4(value & 0xfu);
        result = image.format() == PkImage::Format_ARGB4444_Premultiplied
            ? premultiplied(red, green, blue, expand4(value >> 12))
            : Rgba8{red, green, blue, 255};
        return true;
    }
    case PkImage::Format_RGBX8888:
    case PkImage::Format_RGBA8888:
    case PkImage::Format_RGBA8888_Premultiplied: {
        const std::uint8_t *pixel = row + std::size_t(x) * 4u;
        if (image.format() == PkImage::Format_RGBX8888) result = {pixel[0], pixel[1], pixel[2], 255};
        else if (image.format() == PkImage::Format_RGBA8888) result = {pixel[0], pixel[1], pixel[2], pixel[3]};
        else result = premultiplied(pixel[0], pixel[1], pixel[2], pixel[3]);
        return true;
    }
    case PkImage::Format_BGR30:
    case PkImage::Format_A2BGR30_Premultiplied:
    case PkImage::Format_RGB30:
    case PkImage::Format_A2RGB30_Premultiplied: {
        const std::uint32_t packed = read32(row + std::size_t(x) * 4u);
        const bool bgr = image.format() == PkImage::Format_BGR30 ||
                         image.format() == PkImage::Format_A2BGR30_Premultiplied;
        const bool alphaFormat = image.format() == PkImage::Format_A2BGR30_Premultiplied ||
                                 image.format() == PkImage::Format_A2RGB30_Premultiplied;
        std::uint32_t first = (packed >> 20) & 0x3ffu;
        std::uint32_t green = (packed >> 10) & 0x3ffu;
        std::uint32_t last = packed & 0x3ffu;
        const std::uint32_t alpha = (packed >> 30) & 0x3u;
        if (alphaFormat) {
            first = unpremultiplyNative(first, 0x3ffu, alpha, 3u);
            green = unpremultiplyNative(green, 0x3ffu, alpha, 3u);
            last = unpremultiplyNative(last, 0x3ffu, alpha, 3u);
        }
        result = {expand10(bgr ? last : first), expand10(green), expand10(bgr ? first : last),
                  static_cast<std::uint8_t>(alphaFormat ? alpha * 85u : 255u)};
        return true;
    }
    case PkImage::Format_Alpha8:
        result = {0, 0, 0, row[x]};
        return true;
    case PkImage::Format_Grayscale8:
        result = {row[x], row[x], row[x], 255};
        return true;
    case PkImage::Format_RGBX64:
    case PkImage::Format_RGBA64:
    case PkImage::Format_RGBA64_Premultiplied: {
        const std::uint8_t *pixel = row + std::size_t(x) * 8u;
        std::uint32_t red = read16(pixel);
        std::uint32_t green = read16(pixel + 2);
        std::uint32_t blue = read16(pixel + 4);
        const std::uint32_t alpha = image.format() == PkImage::Format_RGBX64 ? 0xffffu : read16(pixel + 6);
        if (image.format() == PkImage::Format_RGBA64_Premultiplied) {
            red = unpremultiplyNative(red, 0xffffu, alpha, 0xffffu);
            green = unpremultiplyNative(green, 0xffffu, alpha, 0xffffu);
            blue = unpremultiplyNative(blue, 0xffffu, alpha, 0xffffu);
        }
        result = {static_cast<std::uint8_t>((red + 128u) / 257u),
                  static_cast<std::uint8_t>((green + 128u) / 257u),
                  static_cast<std::uint8_t>((blue + 128u) / 257u),
                  static_cast<std::uint8_t>((alpha + 128u) / 257u)};
        return true;
    }
    case PkImage::Format_Grayscale16: {
        const std::uint8_t gray = static_cast<std::uint8_t>((read16(row + std::size_t(x) * 2u) + 128u) / 257u);
        result = {gray, gray, gray, 255};
        return true;
    }
    case PkImage::Format_Invalid:
        return false;
    }
    return false;
}

bool checkedPixelBytes(int width, int height, std::size_t bytesPerPixel, std::size_t &total)
{
    if (width <= 0 || height <= 0) return false;
    const std::size_t w = static_cast<std::size_t>(width);
    const std::size_t h = static_cast<std::size_t>(height);
    if (w > std::numeric_limits<std::size_t>::max() / bytesPerPixel) return false;
    const std::size_t row = w * bytesPerPixel;
    if (h > std::numeric_limits<std::size_t>::max() / row) return false;
    total = row * h;
    return total <= kMaxDecodedRgbaBytes;
}

void pngWrite(png_structp png, png_bytep data, png_size_t length)
{
    auto *bytes = static_cast<std::vector<std::uint8_t> *>(png_get_io_ptr(png));
    if (length > std::numeric_limits<std::size_t>::max() - bytes->size()) png_error(png, "PNG size overflow");
    bytes->insert(bytes->end(), data, data + length);
}

void pngFlush(png_structp) {}

PkByteArray encodePng(const PkImage &image)
{
    const bool gray16 = image.format() == PkImage::Format_Grayscale16;
    const bool rgba16 = image.format() == PkImage::Format_RGBX64 ||
                        image.format() == PkImage::Format_RGBA64 ||
                        image.format() == PkImage::Format_RGBA64_Premultiplied;
    const std::size_t bytesPerPixel = gray16 ? 2u : (rgba16 ? 8u : 4u);
    std::size_t byteCount = 0;
    if (image.isNull() || !checkedPixelBytes(image.width(), image.height(), bytesPerPixel, byteCount)) return {};
    std::vector<std::uint8_t> pixels(byteCount);
    const std::size_t rowBytes = std::size_t(image.width()) * bytesPerPixel;

    for (int y = 0; y < image.height(); ++y) {
        const std::uint8_t *source = image.constScanLine(y);
        for (int x = 0; x < image.width(); ++x) {
            std::uint8_t *target = pixels.data() + std::size_t(y) * rowBytes + std::size_t(x) * bytesPerPixel;
            if (gray16) {
                const std::uint16_t value = read16(source + std::size_t(x) * 2u);
                target[0] = static_cast<std::uint8_t>(value >> 8);
                target[1] = static_cast<std::uint8_t>(value);
            } else if (rgba16) {
                const std::uint8_t *pixel = source + std::size_t(x) * 8u;
                std::array<std::uint32_t, 4> channel{
                    read16(pixel), read16(pixel + 2), read16(pixel + 4),
                    image.format() == PkImage::Format_RGBX64 ? 0xffffu : read16(pixel + 6)};
                if (image.format() == PkImage::Format_RGBA64_Premultiplied) {
                    for (int i = 0; i < 3; ++i) channel[i] = unpremultiplyNative(channel[i], 0xffffu, channel[3], 0xffffu);
                }
                for (int i = 0; i < 4; ++i) {
                    target[i * 2] = static_cast<std::uint8_t>(channel[i] >> 8);
                    target[i * 2 + 1] = static_cast<std::uint8_t>(channel[i]);
                }
            } else {
                Rgba8 pixel;
                if (!readPixel8(image, x, y, pixel)) return {};
                target[0] = pixel.red;
                target[1] = pixel.green;
                target[2] = pixel.blue;
                target[3] = pixel.alpha;
            }
        }
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) return {};
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_write_struct(&png, nullptr);
        return {};
    }
    std::vector<std::uint8_t> encoded;
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        return {};
    }
    png_set_write_fn(png, &encoded, pngWrite, pngFlush);
    png_set_IHDR(png, info, static_cast<png_uint_32>(image.width()), static_cast<png_uint_32>(image.height()),
                 gray16 || rgba16 ? 16 : 8, gray16 ? PNG_COLOR_TYPE_GRAY : PNG_COLOR_TYPE_RGBA,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
    png_write_info(png, info);
    std::vector<png_bytep> rows(static_cast<std::size_t>(image.height()));
    for (int y = 0; y < image.height(); ++y) rows[static_cast<std::size_t>(y)] = pixels.data() + std::size_t(y) * rowBytes;
    png_write_image(png, rows.data());
    png_write_end(png, info);
    png_destroy_write_struct(&png, &info);
    if (encoded.size() > static_cast<std::size_t>(std::numeric_limits<int>::max())) return {};
    return PkByteArray(encoded);
}

std::string base64Encode(const PkByteArray &data)
{
    if (data.isEmpty()) return {};
    const std::size_t length = static_cast<std::size_t>(data.size());
    if (length > (std::numeric_limits<std::size_t>::max() - 2u) / 4u * 3u) return {};
    std::string result;
    result.reserve(((length + 2u) / 3u) * 4u);
    const auto *source = reinterpret_cast<const std::uint8_t *>(data.constData());
    for (std::size_t i = 0; i < length; i += 3u) {
        const unsigned int chunk = (unsigned(source[i]) << 16) |
            (i + 1u < length ? unsigned(source[i + 1u]) << 8 : 0u) |
            (i + 2u < length ? unsigned(source[i + 2u]) : 0u);
        result.push_back(kBase64Alphabet[(chunk >> 18) & 0x3f]);
        result.push_back(kBase64Alphabet[(chunk >> 12) & 0x3f]);
        result.push_back(i + 1u < length ? kBase64Alphabet[(chunk >> 6) & 0x3f] : '=');
        result.push_back(i + 2u < length ? kBase64Alphabet[chunk & 0x3f] : '=');
    }
    return result;
}

int base64Value(char character)
{
    if (character >= 'A' && character <= 'Z') return character - 'A';
    if (character >= 'a' && character <= 'z') return character - 'a' + 26;
    if (character >= '0' && character <= '9') return character - '0' + 52;
    if (character == '+') return 62;
    if (character == '/') return 63;
    return -1;
}

PkByteArray decodeBase64Range(const PkString &encoded, int offset, int length)
{
    if (offset < 0 || length <= 0 || offset > encoded.size() ||
        length > encoded.size() - offset) return {};
    std::size_t padding = 0;
    if (encoded.at(offset + length - 1) == u'=') ++padding;
    if (length > 1 && encoded.at(offset + length - 2) == u'=') ++padding;
    std::size_t decodedLength = 0;
    if (!ImageShapePngData::base64DecodedSizeWithinLimit(
            static_cast<std::size_t>(length), padding, decodedLength)) return {};
    for (int i = 0; i < length; ++i) {
        if (encoded.at(offset + i) > 0x7fu) return {};
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(decodedLength);
    for (int relativeOffset = 0; relativeOffset < length; relativeOffset += 4) {
        const bool finalQuartet = relativeOffset + 4 == length;
        const int quartetOffset = offset + relativeOffset;
        std::array<char, 4> source{};
        for (int i = 0; i < 4; ++i) {
            source[static_cast<std::size_t>(i)] =
                static_cast<char>(encoded.at(quartetOffset + i));
        }
        const int first = base64Value(source[0]);
        const int second = base64Value(source[1]);
        const int third = source[2] == '=' ? -2 : base64Value(source[2]);
        const int fourth = source[3] == '=' ? -2 : base64Value(source[3]);
        if (first < 0 || second < 0 || third == -1 || fourth == -1 ||
            (!finalQuartet && (third == -2 || fourth == -2)) ||
            (third == -2 && fourth != -2)) return {};
        bytes.push_back(static_cast<std::uint8_t>((first << 2) | (second >> 4)));
        if (third == -2) {
            if ((second & 0x0f) != 0) return {};
            continue;
        }
        bytes.push_back(static_cast<std::uint8_t>((second << 4) | (third >> 2)));
        if (fourth == -2) {
            if ((third & 0x03) != 0) return {};
            continue;
        }
        bytes.push_back(static_cast<std::uint8_t>((third << 6) | fourth));
    }
    return bytes.size() == decodedLength ? PkByteArray(bytes) : PkByteArray();
}

struct PngInput {
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
    std::size_t offset = 0;
};

void pngRead(png_structp png, png_bytep output, png_size_t length)
{
    auto *input = static_cast<PngInput *>(png_get_io_ptr(png));
    if (length > input->size - input->offset) png_error(png, "truncated PNG");
    std::memcpy(output, input->data + input->offset, length);
    input->offset += length;
}

PkImage decodePngBytes(const PkByteArray &encoded)
{
    if (encoded.size() < 8 || png_sig_cmp(reinterpret_cast<png_const_bytep>(encoded.constData()), 0, 8)) return {};
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if (!png) return {};
    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, nullptr, nullptr);
        return {};
    }
    PngInput input{reinterpret_cast<const std::uint8_t *>(encoded.constData()), static_cast<std::size_t>(encoded.size()), 0};
    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        return {};
    }
    png_set_read_fn(png, &input, pngRead);
    png_read_info(png, info);
    png_uint_32 width = 0, height = 0;
    int bitDepth = 0, colorType = 0, interlace = 0, compression = 0, filter = 0;
    png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType, &interlace, &compression, &filter);
    if (width > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
        height > static_cast<png_uint_32>(std::numeric_limits<int>::max())) {
        png_destroy_read_struct(&png, &info, nullptr);
        return {};
    }
    const bool hasTransparency = png_get_valid(png, info, PNG_INFO_tRNS) != 0;
    const bool sourceHasAlpha = (colorType & PNG_COLOR_MASK_ALPHA) != 0 || hasTransparency;
    const bool preserveGray16 = bitDepth == 16 && colorType == PNG_COLOR_TYPE_GRAY && !hasTransparency;
    const bool highDepth = bitDepth == 16;
    if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (bitDepth < 8 && colorType == PNG_COLOR_TYPE_GRAY) png_set_expand_gray_1_2_4_to_8(png);
    if (hasTransparency) png_set_tRNS_to_alpha(png);
    if (!preserveGray16) {
        if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);
        if (!sourceHasAlpha) {
            png_set_add_alpha(png, highDepth ? 0xffffu : 0xffu, PNG_FILLER_AFTER);
        }
    }
    if (interlace != PNG_INTERLACE_NONE) png_set_interlace_handling(png);
    png_read_update_info(png, info);
    const std::size_t rowBytes = png_get_rowbytes(png, info);
    if (height == 0 || rowBytes > kMaxDecodedRgbaBytes || height > kMaxDecodedRgbaBytes / rowBytes) {
        png_destroy_read_struct(&png, &info, nullptr);
        return {};
    }
    std::vector<std::uint8_t> pixels(rowBytes * static_cast<std::size_t>(height));
    std::vector<png_bytep> rows(static_cast<std::size_t>(height));
    for (std::size_t y = 0; y < rows.size(); ++y) rows[y] = pixels.data() + y * rowBytes;
    png_read_image(png, rows.data());
    png_read_end(png, info);
    png_destroy_read_struct(&png, &info, nullptr);

    PkImage image(static_cast<int>(width), static_cast<int>(height),
                  preserveGray16 ? PkImage::Format_Grayscale16 :
                  (highDepth ? PkImage::Format_RGBA64 : PkImage::Format_ARGB32));
    for (std::size_t y = 0; y < height; ++y) {
        const std::uint8_t *source = pixels.data() + y * rowBytes;
        if (preserveGray16) {
            for (std::size_t x = 0; x < width; ++x) {
                const std::uint16_t value = static_cast<std::uint16_t>((source[x * 2] << 8) | source[x * 2 + 1]);
                std::memcpy(image.scanLine(static_cast<int>(y)) + x * 2, &value, 2);
            }
        } else if (highDepth) {
            for (std::size_t x = 0; x < width; ++x) {
                for (int channel = 0; channel < 4; ++channel) {
                    const std::uint8_t *component = source + x * 8 + channel * 2;
                    const std::uint16_t value = static_cast<std::uint16_t>((component[0] << 8) | component[1]);
                    std::memcpy(image.scanLine(static_cast<int>(y)) + x * 8 + channel * 2, &value, 2);
                }
            }
        } else {
            for (std::size_t x = 0; x < width; ++x) {
                const std::uint8_t *pixel = source + x * 4;
                image.setPixel(static_cast<int>(x), static_cast<int>(y),
                               (std::uint32_t(pixel[3]) << 24) | (std::uint32_t(pixel[0]) << 16) |
                               (std::uint32_t(pixel[1]) << 8) | pixel[2]);
            }
        }
    }
    return image;
}

struct JpegError {
    jpeg_error_mgr base;
    std::jmp_buf jump;
};

void jpegErrorExit(j_common_ptr common)
{
    std::longjmp(reinterpret_cast<JpegError *>(common->err)->jump, 1);
}

PkImage decodeJpeg(const PkByteArray &encoded)
{
    jpeg_decompress_struct decoder{};
    JpegError error{};
    decoder.err = jpeg_std_error(&error.base);
    error.base.error_exit = jpegErrorExit;
    if (setjmp(error.jump)) {
        jpeg_destroy_decompress(&decoder);
        return {};
    }
    jpeg_create_decompress(&decoder);
    jpeg_mem_src(&decoder, reinterpret_cast<const unsigned char *>(encoded.constData()),
                 static_cast<unsigned long>(encoded.size()));
    if (jpeg_read_header(&decoder, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&decoder);
        return {};
    }
    decoder.out_color_space = JCS_RGB;
    jpeg_start_decompress(&decoder);
    std::size_t byteCount = 0;
    if (!checkedPixelBytes(static_cast<int>(decoder.output_width), static_cast<int>(decoder.output_height), 4u, byteCount)) {
        jpeg_destroy_decompress(&decoder);
        return {};
    }
    PkImage image(static_cast<int>(decoder.output_width), static_cast<int>(decoder.output_height), PkImage::Format_ARGB32);
    std::vector<JSAMPLE> row(static_cast<std::size_t>(decoder.output_width) * decoder.output_components);
    while (decoder.output_scanline < decoder.output_height) {
        JSAMPROW rows[] = {row.data()};
        jpeg_read_scanlines(&decoder, rows, 1);
        const int y = static_cast<int>(decoder.output_scanline - 1);
        for (std::size_t x = 0; x < decoder.output_width; ++x) {
            image.setPixel(static_cast<int>(x), y, 0xff000000u |
                           (std::uint32_t(row[x * 3]) << 16) |
                           (std::uint32_t(row[x * 3 + 1]) << 8) | row[x * 3 + 2]);
        }
    }
    jpeg_finish_decompress(&decoder);
    jpeg_destroy_decompress(&decoder);
    return image;
}

struct TiffInput {
    const std::uint8_t *data = nullptr;
    std::size_t size = 0;
    toff_t offset = 0;
};

tsize_t tiffRead(thandle_t handle, tdata_t data, tsize_t size)
{
    auto &input = *static_cast<TiffInput *>(handle);
    const std::size_t available = input.offset < input.size ? input.size - static_cast<std::size_t>(input.offset) : 0;
    const std::size_t count = std::min<std::size_t>(available, static_cast<std::size_t>(size));
    std::memcpy(data, input.data + input.offset, count);
    input.offset += static_cast<toff_t>(count);
    return static_cast<tsize_t>(count);
}

tsize_t tiffNoWrite(thandle_t, tdata_t, tsize_t) { return 0; }
toff_t tiffSeek(thandle_t handle, toff_t offset, int whence)
{
    auto &input = *static_cast<TiffInput *>(handle);
    toff_t base = whence == SEEK_CUR ? input.offset : (whence == SEEK_END ? static_cast<toff_t>(input.size) : 0);
    if (offset > std::numeric_limits<toff_t>::max() - base) return static_cast<toff_t>(-1);
    input.offset = base + offset;
    return input.offset;
}
int tiffClose(thandle_t) { return 0; }
toff_t tiffSize(thandle_t handle) { return static_cast<toff_t>(static_cast<TiffInput *>(handle)->size); }
int tiffMap(thandle_t, tdata_t *, toff_t *) { return 0; }
void tiffUnmap(thandle_t, tdata_t, toff_t) {}

PkImage decodeTiff(const PkByteArray &encoded)
{
    TiffInput input{reinterpret_cast<const std::uint8_t *>(encoded.constData()), static_cast<std::size_t>(encoded.size()), 0};
    TIFF *tiff = TIFFClientOpen("memory", "r", &input, tiffRead, tiffNoWrite,
                                tiffSeek, tiffClose, tiffSize, tiffMap, tiffUnmap);
    if (!tiff) return {};
    std::uint32_t width = 0, height = 0;
    TIFFGetField(tiff, TIFFTAG_IMAGEWIDTH, &width);
    TIFFGetField(tiff, TIFFTAG_IMAGELENGTH, &height);
    std::size_t byteCount = 0;
    if (width > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint32_t>(std::numeric_limits<int>::max()) ||
        !checkedPixelBytes(static_cast<int>(width), static_cast<int>(height), 4u, byteCount)) {
        TIFFClose(tiff);
        return {};
    }
    std::vector<std::uint32_t> raster(static_cast<std::size_t>(width) * height);
    if (!TIFFReadRGBAImageOriented(tiff, width, height, raster.data(), ORIENTATION_TOPLEFT, 1)) {
        TIFFClose(tiff);
        return {};
    }
    TIFFClose(tiff);
    PkImage image(static_cast<int>(width), static_cast<int>(height), PkImage::Format_ARGB32);
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            const std::uint32_t pixel = raster[static_cast<std::size_t>(y) * width + x];
            const std::uint8_t alpha = TIFFGetA(pixel);
            image.setPixel(static_cast<int>(x), static_cast<int>(y),
                           (std::uint32_t(alpha) << 24) |
                           (std::uint32_t(unpremultiply8(TIFFGetR(pixel), alpha)) << 16) |
                           (std::uint32_t(unpremultiply8(TIFFGetG(pixel), alpha)) << 8) |
                           unpremultiply8(TIFFGetB(pixel), alpha));
        }
    }
    return image;
}
} // namespace

namespace ImageShapePngData
{
std::size_t maxDecodedCompressedBytes()
{
    return kMaxDecodedCompressedBytes;
}

bool base64DecodedSizeWithinLimit(std::size_t encodedLength,
                                  std::size_t padding,
                                  std::size_t &decodedLength)
{
    decodedLength = 0;
    if (encodedLength == 0 || encodedLength % 4u != 0u || padding > 2u) return false;
    const std::size_t quartets = encodedLength / 4u;
    if (quartets > std::numeric_limits<std::size_t>::max() / 3u) return false;
    const std::size_t upper = quartets * 3u;
    if (padding > upper) return false;
    decodedLength = upper - padding;
    return decodedLength <= kMaxDecodedCompressedBytes;
}

PkString encodeBase64(const PkImage &image)
{
    const std::string encoded = base64Encode(encodePng(image));
    return PkString::PkFromUtf8(encoded.data(), static_cast<int>(encoded.size()));
}

PkString encodeDataUri(const PkImage &image)
{
    const PkString encoded = encodeBase64(image);
    return encoded.isEmpty() ? PkString() : PkString("data:image/png;base64,") + encoded;
}

PkByteArray decodeBase64(const PkString &encoded)
{
    return decodeBase64Range(encoded, 0, encoded.size());
}

PkByteArray decodeDataUriBase64(const PkString &dataUri)
{
    constexpr char16_t marker[] = u";base64,";
    constexpr int markerLength = 8;
    if (!dataUri.startsWith("data:") || dataUri.size() < markerLength) return {};
    for (int offset = 5; offset <= dataUri.size() - markerLength; ++offset) {
        bool matches = true;
        for (int i = 0; i < markerLength; ++i) {
            if (dataUri.at(offset + i) != marker[i]) {
                matches = false;
                break;
            }
        }
        if (matches) {
            const int payloadOffset = offset + markerLength;
            return decodeBase64Range(dataUri, payloadOffset, dataUri.size() - payloadOffset);
        }
    }
    return {};
}

PkImage decodePng(const PkByteArray &encodedPng)
{
    return decodePngBytes(encodedPng);
}

PkImage decodeImage(const PkByteArray &encodedImage)
{
    if (encodedImage.isEmpty() || static_cast<std::size_t>(encodedImage.size()) > kMaxDecodedCompressedBytes) return {};
    const auto *data = reinterpret_cast<const std::uint8_t *>(encodedImage.constData());
    const std::size_t size = static_cast<std::size_t>(encodedImage.size());
    if (size >= 8 && png_sig_cmp(data, 0, 8) == 0) return decodePngBytes(encodedImage);
    if (size >= 2 && data[0] == 0xffu && data[1] == 0xd8u) return decodeJpeg(encodedImage);
    if (size >= 4 && ((data[0] == 'I' && data[1] == 'I' && data[2] == 42 && data[3] == 0) ||
                      (data[0] == 'M' && data[1] == 'M' && data[2] == 0 && data[3] == 42))) {
        return decodeTiff(encodedImage);
    }
    return {};
}
} // namespace ImageShapePngData
