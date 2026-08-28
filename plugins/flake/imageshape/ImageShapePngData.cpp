/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ImageShapePngData.h"

#include <png.h>

#include <cstdint>
#include <limits>
#include <string>
#include <vector>

namespace
{
const char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

constexpr std::size_t kMaxDecodedRgbaBytes = 256u * 1024u * 1024u;

std::string base64Encode(const PkByteArray &data)
{
    std::string result;
    const char *source = data.constData();
    const int length = data.size();
    for (int i = 0; i < length; i += 3) {
        const unsigned int chunk =
            (static_cast<unsigned char>(source[i]) << 16) |
            (i + 1 < length ? static_cast<unsigned char>(source[i + 1]) << 8 : 0u) |
            (i + 2 < length ? static_cast<unsigned char>(source[i + 2]) : 0u);
        result.push_back(kBase64Alphabet[(chunk >> 18) & 0x3f]);
        result.push_back(kBase64Alphabet[(chunk >> 12) & 0x3f]);
        result.push_back(i + 1 < length ? kBase64Alphabet[(chunk >> 6) & 0x3f] : '=');
        result.push_back(i + 2 < length ? kBase64Alphabet[chunk & 0x3f] : '=');
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

bool rgbaByteCount(unsigned int width, unsigned int height, std::size_t &byteCount)
{
    if (width == 0 || height == 0 ||
        width > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
        height > static_cast<unsigned int>(std::numeric_limits<int>::max()) ||
        width > std::numeric_limits<std::size_t>::max() / 4u) {
        return false;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(width) * 4u;
    if (height > std::numeric_limits<std::size_t>::max() / rowBytes) {
        return false;
    }
    byteCount = rowBytes * static_cast<std::size_t>(height);
    return byteCount <= kMaxDecodedRgbaBytes;
}

PkByteArray encodePng(const PkImage &image)
{
    if (image.isNull()) {
        return PkByteArray();
    }

    std::size_t byteCount = 0;
    if (!rgbaByteCount(static_cast<unsigned int>(image.width()),
                       static_cast<unsigned int>(image.height()),
                       byteCount)) {
        return PkByteArray();
    }

    std::vector<png_byte> pixels(byteCount);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const std::uint32_t argb = image.pixel(x, y);
            const std::size_t offset =
                (static_cast<std::size_t>(y) * image.width() + x) * 4u;
            pixels[offset] = static_cast<png_byte>((argb >> 16) & 0xffu);
            pixels[offset + 1u] = static_cast<png_byte>((argb >> 8) & 0xffu);
            pixels[offset + 2u] = static_cast<png_byte>(argb & 0xffu);
            pixels[offset + 3u] = static_cast<png_byte>((argb >> 24) & 0xffu);
        }
    }

    png_image pngImage{};
    pngImage.version = PNG_IMAGE_VERSION;
    pngImage.width = static_cast<png_uint_32>(image.width());
    pngImage.height = static_cast<png_uint_32>(image.height());
    pngImage.format = PNG_FORMAT_RGBA;
    png_alloc_size_t encodedSize = 0;
    if (!png_image_write_to_memory(&pngImage, nullptr, &encodedSize, 0,
                                   pixels.data(), 0, nullptr)) {
        png_image_free(&pngImage);
        return PkByteArray();
    }
    std::vector<std::uint8_t> encoded(static_cast<std::size_t>(encodedSize));
    if (!png_image_write_to_memory(&pngImage, encoded.data(), &encodedSize, 0,
                                   pixels.data(), 0, nullptr)) {
        png_image_free(&pngImage);
        return PkByteArray();
    }
    png_image_free(&pngImage);
    encoded.resize(static_cast<std::size_t>(encodedSize));
    return PkByteArray(encoded);
}
} // namespace

namespace ImageShapePngData
{

PkString encodeBase64(const PkImage &image)
{
    return PkString(base64Encode(encodePng(image)).c_str());
}

PkString encodeDataUri(const PkImage &image)
{
    const PkString encoded = encodeBase64(image);
    return encoded.isEmpty() ? PkString()
                             : PkString("data:image/png;base64,") + encoded;
}

PkByteArray decodeBase64(const PkString &encoded)
{
    const std::string source = encoded.PkToUtf8();
    if (source.empty() || source.size() % 4u != 0u) {
        return PkByteArray();
    }

    std::vector<std::uint8_t> bytes;
    bytes.reserve(source.size() / 4u * 3u);
    for (std::size_t offset = 0; offset < source.size(); offset += 4u) {
        const bool finalQuartet = offset + 4u == source.size();
        const int first = base64Value(source[offset]);
        const int second = base64Value(source[offset + 1u]);
        const int third = source[offset + 2u] == '=' ? -2 : base64Value(source[offset + 2u]);
        const int fourth = source[offset + 3u] == '=' ? -2 : base64Value(source[offset + 3u]);

        if (first < 0 || second < 0 || third == -1 || fourth == -1 ||
            (!finalQuartet && (third == -2 || fourth == -2)) ||
            (third == -2 && fourth != -2)) {
            return PkByteArray();
        }

        bytes.push_back(static_cast<std::uint8_t>((first << 2) | (second >> 4)));
        if (third == -2) {
            if ((second & 0x0f) != 0) return PkByteArray();
            continue;
        }
        bytes.push_back(static_cast<std::uint8_t>((second << 4) | (third >> 2)));
        if (fourth == -2) {
            if ((third & 0x03) != 0) return PkByteArray();
            continue;
        }
        bytes.push_back(static_cast<std::uint8_t>((third << 6) | fourth));
    }
    return PkByteArray(bytes);
}

PkImage decodePng(const PkByteArray &encodedPng)
{
    if (encodedPng.isEmpty()) {
        return PkImage();
    }

    png_image pngImage{};
    pngImage.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&pngImage,
                                          encodedPng.constData(),
                                          static_cast<std::size_t>(encodedPng.size()))) {
        png_image_free(&pngImage);
        return PkImage();
    }

    std::size_t byteCount = 0;
    if (!rgbaByteCount(pngImage.width, pngImage.height, byteCount)) {
        png_image_free(&pngImage);
        return PkImage();
    }

    pngImage.format = PNG_FORMAT_RGBA;
    std::vector<png_byte> pixels(byteCount);
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
                           (static_cast<std::uint32_t>(pixels[offset + 3u]) << 24) |
                           (static_cast<std::uint32_t>(pixels[offset]) << 16) |
                           (static_cast<std::uint32_t>(pixels[offset + 1u]) << 8) |
                           pixels[offset + 2u]);
        }
    }
    png_image_free(&pngImage);
    return image;
}

} // namespace ImageShapePngData
