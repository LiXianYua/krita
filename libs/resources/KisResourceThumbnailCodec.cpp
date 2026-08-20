/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KisResourceThumbnailCodec.h"

#include <png.h>

#include <cstdio>
#include <limits>
#include <vector>

namespace KisResourceThumbnailCodec
{

PkImage loadPng(const PkString &path)
{
    FILE *file = std::fopen(path.PkToUtf8().c_str(), "rb");
    if (!file) {
        return PkImage();
    }

    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, nullptr);
        std::fclose(file);
        return PkImage();
    }

    png_init_io(png, file);
    png_read_info(png, info);

    png_uint_32 width = 0;
    png_uint_32 height = 0;
    int bitDepth = 0;
    int colorType = 0;
    png_get_IHDR(png, info, &width, &height, &bitDepth, &colorType,
                 nullptr, nullptr, nullptr);
    if (width == 0 || height == 0 ||
        width > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
        height > static_cast<png_uint_32>(std::numeric_limits<int>::max())) {
        png_error(png, "unsupported thumbnail dimensions");
    }

    if (bitDepth == 16) png_set_strip_16(png);
    if (colorType == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (colorType == PNG_COLOR_TYPE_GRAY || colorType == PNG_COLOR_TYPE_GRAY_ALPHA) {
        png_set_gray_to_rgb(png);
    }
    if ((colorType & PNG_COLOR_MASK_ALPHA) == 0 &&
        !png_get_valid(png, info, PNG_INFO_tRNS)) {
        png_set_add_alpha(png, 0xff, PNG_FILLER_AFTER);
    }
    png_read_update_info(png, info);

    const png_size_t rowBytes = png_get_rowbytes(png, info);
    if (rowBytes < width * 4u || height > std::numeric_limits<std::size_t>::max() / rowBytes) {
        png_error(png, "unsupported thumbnail row size");
    }
    std::vector<png_byte> pixels(rowBytes * height);
    std::vector<png_bytep> rows(height);
    for (png_uint_32 y = 0; y < height; ++y) {
        rows[y] = pixels.data() + y * rowBytes;
    }
    png_read_image(png, rows.data());
    png_read_end(png, nullptr);
    png_destroy_read_struct(&png, &info, nullptr);
    std::fclose(file);

    PkImage image(static_cast<int>(width), static_cast<int>(height), PkImage::Format_ARGB32);
    for (png_uint_32 y = 0; y < height; ++y) {
        const png_bytep row = rows[y];
        for (png_uint_32 x = 0; x < width; ++x) {
            const png_bytep rgba = row + x * 4u;
            const uint32_t argb = (static_cast<uint32_t>(rgba[3]) << 24) |
                                  (static_cast<uint32_t>(rgba[0]) << 16) |
                                  (static_cast<uint32_t>(rgba[1]) << 8) |
                                  static_cast<uint32_t>(rgba[2]);
            image.setPixel(static_cast<int>(x), static_cast<int>(y), argb);
        }
    }
    return image;
}

bool savePng(const PkString &path, const PkImage &image)
{
    if (image.isNull()) {
        return false;
    }
    FILE *file = std::fopen(path.PkToUtf8().c_str(), "wb");
    if (!file) {
        return false;
    }

    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    png_infop info = png ? png_create_info_struct(png) : nullptr;
    if (!png || !info || setjmp(png_jmpbuf(png))) {
        png_destroy_write_struct(&png, &info);
        std::fclose(file);
        return false;
    }

    png_init_io(png, file);
    png_set_IHDR(png, info,
                 static_cast<png_uint_32>(image.width()),
                 static_cast<png_uint_32>(image.height()),
                 8, PNG_COLOR_TYPE_RGBA, PNG_INTERLACE_NONE,
                 PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    std::vector<png_byte> row(static_cast<std::size_t>(image.width()) * 4u);
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const uint32_t argb = image.pixel(x, y);
            row[static_cast<std::size_t>(x) * 4u + 0] = static_cast<png_byte>((argb >> 16) & 0xffu);
            row[static_cast<std::size_t>(x) * 4u + 1] = static_cast<png_byte>((argb >> 8) & 0xffu);
            row[static_cast<std::size_t>(x) * 4u + 2] = static_cast<png_byte>(argb & 0xffu);
            row[static_cast<std::size_t>(x) * 4u + 3] = static_cast<png_byte>((argb >> 24) & 0xffu);
        }
        png_write_row(png, row.data());
    }
    png_write_end(png, nullptr);
    png_destroy_write_struct(&png, &info);
    return std::fclose(file) == 0;
}

}
