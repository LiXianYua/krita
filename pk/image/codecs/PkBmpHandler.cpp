/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "../PkImageFileDecoder.h"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <vector>

namespace
{

constexpr uint32_t kRgb = 0;
constexpr uint32_t kRle8 = 1;
constexpr uint32_t kRle4 = 2;
constexpr uint32_t kBitFields = 3;
constexpr uint64_t kMaximumPixels = (512u * 1024u * 1024u) / 4u;

class Cursor
{
public:
    Cursor(const uint8_t *data, std::size_t size) : m_data(data), m_size(size) {}
    bool seek(std::size_t position) { if (position > m_size) return false; m_position = position; return true; }
    std::size_t position() const { return m_position; }
    std::size_t remaining() const { return m_size - m_position; }
    bool u8(uint8_t &value) { if (remaining() < 1) return false; value = m_data[m_position++]; return true; }
    bool u16(uint16_t &value) {
        if (remaining() < 2) return false;
        value = static_cast<uint16_t>(m_data[m_position]) |
                static_cast<uint16_t>(m_data[m_position + 1] << 8);
        m_position += 2; return true;
    }
    bool u32(uint32_t &value) {
        if (remaining() < 4) return false;
        value = static_cast<uint32_t>(m_data[m_position]) |
                (static_cast<uint32_t>(m_data[m_position + 1]) << 8) |
                (static_cast<uint32_t>(m_data[m_position + 2]) << 16) |
                (static_cast<uint32_t>(m_data[m_position + 3]) << 24);
        m_position += 4; return true;
    }
    bool bytes(const uint8_t *&value, std::size_t count) {
        if (count > remaining()) return false;
        value = m_data + m_position; m_position += count; return true;
    }
private:
    const uint8_t *m_data;
    std::size_t m_size;
    std::size_t m_position = 0;
};

uint8_t component(uint32_t pixel, uint32_t mask, uint8_t fallback)
{
    if (!mask) return fallback;
    unsigned shift = 0;
    while ((mask & 1u) == 0u) { mask >>= 1; ++shift; }
    const uint32_t value = (pixel >> shift) & mask;
    // qbmphandler.cpp intentionally uses the integral 256/(max+1) scale.
    const uint64_t scale = 256u / (static_cast<uint64_t>(mask) + 1u);
    return static_cast<uint8_t>(value * scale);
}

uint32_t maskedPixel(uint32_t pixel, uint32_t redMask, uint32_t greenMask,
                     uint32_t blueMask, uint32_t alphaMask)
{
    return (static_cast<uint32_t>(component(pixel, alphaMask, 255)) << 24) |
           (static_cast<uint32_t>(component(pixel, redMask, 0)) << 16) |
           (static_cast<uint32_t>(component(pixel, greenMask, 0)) << 8) |
           component(pixel, blueMask, 0);
}

bool isBmp(const uint8_t *data, std::size_t size)
{
    return data && size >= 18 && data[0] == 'B' && data[1] == 'M';
}

bool decodeRle(Cursor &cursor, PkImage &image, int width, int height, bool topDown,
               uint32_t compression, const std::vector<uint32_t> &palette)
{
    int x = 0;
    int row = 0;
    auto put = [&](unsigned index) {
        if (x < 0 || x >= width || row < 0 || row >= height || index >= palette.size()) return false;
        const int y = topDown ? row : height - 1 - row;
        image.setPixel(x++, y, palette[index]);
        return true;
    };
    while (cursor.remaining() >= 2) {
        uint8_t count = 0, command = 0;
        cursor.u8(count); cursor.u8(command);
        if (count != 0) {
            for (unsigned i = 0; i < count; ++i) {
                const unsigned index = compression == kRle8 ? command :
                                       ((i & 1u) ? command & 0x0Fu : command >> 4);
                if (!put(index)) return false;
            }
            continue;
        }
        if (command == 0) { x = 0; if (++row > height) return false; continue; }
        if (command == 1) return row < height;
        if (command == 2) {
            uint8_t dx = 0, dy = 0;
            if (!cursor.u8(dx) || !cursor.u8(dy)) return false;
            x += dx; row += dy;
            if (x > width || row >= height) return false;
            continue;
        }
        const unsigned literalPixels = command;
        const unsigned literalBytes = compression == kRle8 ? literalPixels : (literalPixels + 1u) / 2u;
        for (unsigned byteIndex = 0; byteIndex < literalBytes; ++byteIndex) {
            uint8_t packed = 0;
            if (!cursor.u8(packed)) return false;
            if (compression == kRle8) {
                if (!put(packed)) return false;
            } else {
                if (!put(packed >> 4)) return false;
                if (byteIndex * 2u + 1u < literalPixels && !put(packed & 0x0Fu)) return false;
            }
        }
        if (literalBytes & 1u) { uint8_t padding = 0; if (!cursor.u8(padding)) return false; }
    }
    return false;
}

PkImage decodeBmp(const uint8_t *data, std::size_t size)
{
    if (!isBmp(data, size)) return PkImage();
    Cursor cursor(data, size);
    uint16_t signature = 0, ignored16 = 0;
    uint32_t ignored32 = 0, pixelOffset = 0, headerSize = 0;
    if (!cursor.u16(signature) || signature != 0x4D42u || !cursor.u32(ignored32) ||
        !cursor.u16(ignored16) || !cursor.u16(ignored16) || !cursor.u32(pixelOffset) ||
        !cursor.u32(headerSize)) return PkImage();

    int32_t width = 0, signedHeight = 0;
    uint16_t planes = 0, bits = 0;
    uint32_t compression = kRgb, colorsUsed = 0;
    const bool oldHeader = headerSize == 12;
    if (oldHeader) {
        uint16_t oldWidth = 0, oldHeight = 0;
        if (!cursor.u16(oldWidth) || !cursor.u16(oldHeight) ||
            !cursor.u16(planes) || !cursor.u16(bits)) return PkImage();
        width = oldWidth; signedHeight = oldHeight;
    } else {
        if (headerSize != 40 && headerSize != 64 && headerSize != 108 && headerSize != 124) return PkImage();
        uint32_t rawWidth = 0, rawHeight = 0;
        if (!cursor.u32(rawWidth) || !cursor.u32(rawHeight) || !cursor.u16(planes) ||
            !cursor.u16(bits) || !cursor.u32(compression) || !cursor.u32(ignored32) ||
            !cursor.u32(ignored32) || !cursor.u32(ignored32) || !cursor.u32(colorsUsed) ||
            !cursor.u32(ignored32)) return PkImage();
        width = static_cast<int32_t>(rawWidth); signedHeight = static_cast<int32_t>(rawHeight);
    }

    if (width <= 0 || signedHeight == 0 || signedHeight == std::numeric_limits<int32_t>::min() ||
        planes != 1 || (bits != 1 && bits != 4 && bits != 8 && bits != 16 && bits != 24 && bits != 32)) {
        return PkImage();
    }
    const bool topDown = signedHeight < 0;
    const int height = topDown ? -signedHeight : signedHeight;
    if (static_cast<uint64_t>(width) * static_cast<uint64_t>(height) > kMaximumPixels ||
        (compression != kRgb && compression != kRle4 && compression != kRle8 && compression != kBitFields) ||
        (compression == kRle4 && bits != 4) || (compression == kRle8 && bits != 8) ||
        (compression == kBitFields && bits != 16 && bits != 32) ||
        (topDown && (compression == kRle4 || compression == kRle8))) return PkImage();

    uint32_t redMask = bits == 16 ? 0x7C00u : 0x00FF0000u;
    uint32_t greenMask = bits == 16 ? 0x03E0u : 0x0000FF00u;
    uint32_t blueMask = bits == 16 ? 0x001Fu : 0x000000FFu;
    uint32_t alphaMask = 0;
    const std::size_t headerEnd = 14u + headerSize;
    if (!cursor.seek(headerEnd)) return PkImage();
    if (headerSize >= 108) {
        Cursor masks(data + 14u + 40u, size - (14u + 40u));
        if (!masks.u32(redMask) || !masks.u32(greenMask) || !masks.u32(blueMask) ||
            !masks.u32(alphaMask)) return PkImage();
    } else if (compression == kBitFields) {
        if (!cursor.u32(redMask) || !cursor.u32(greenMask) || !cursor.u32(blueMask)) return PkImage();
    }

    std::vector<uint32_t> palette;
    if (bits <= 8) {
        const uint32_t maximumColors = 1u << bits;
        const uint32_t count = colorsUsed ? colorsUsed : maximumColors;
        if (count == 0 || count > maximumColors) return PkImage();
        palette.reserve(count);
        for (uint32_t index = 0; index < count; ++index) {
            uint8_t blue = 0, green = 0, red = 0, reserved = 0;
            if (!cursor.u8(blue) || !cursor.u8(green) || !cursor.u8(red) ||
                (!oldHeader && !cursor.u8(reserved))) return PkImage();
            palette.push_back(0xFF000000u | (static_cast<uint32_t>(red) << 16) |
                              (static_cast<uint32_t>(green) << 8) | blue);
        }
    }
    if (pixelOffset < cursor.position() || !cursor.seek(pixelOffset)) return PkImage();

    PkImage image(width, height, PkImage::Format_ARGB32);
    if (image.isNull()) return PkImage();
    image.fill(palette.empty() ? 0xFF000000u : palette[0]);
    if (compression == kRle4 || compression == kRle8) {
        return decodeRle(cursor, image, width, height, topDown, compression, palette) ? image : PkImage();
    }

    const uint64_t rowBits = static_cast<uint64_t>(width) * bits;
    const uint64_t rowBytes64 = ((rowBits + 31u) / 32u) * 4u;
    if (rowBytes64 > std::numeric_limits<std::size_t>::max() ||
        rowBytes64 * static_cast<uint64_t>(height) > cursor.remaining()) return PkImage();
    const std::size_t rowBytes = static_cast<std::size_t>(rowBytes64);
    for (int sourceRow = 0; sourceRow < height; ++sourceRow) {
        const uint8_t *row = nullptr;
        if (!cursor.bytes(row, rowBytes)) return PkImage();
        const int y = topDown ? sourceRow : height - 1 - sourceRow;
        for (int x = 0; x < width; ++x) {
            uint32_t argb = 0;
            if (bits == 1) {
                const unsigned index = (row[x / 8] >> (7 - (x & 7))) & 1u;
                if (index >= palette.size()) return PkImage();
                argb = palette[index];
            } else if (bits == 4) {
                const unsigned index = (x & 1) ? row[x / 2] & 0x0Fu : row[x / 2] >> 4;
                if (index >= palette.size()) return PkImage();
                argb = palette[index];
            } else if (bits == 8) {
                const unsigned index = row[x];
                if (index >= palette.size()) return PkImage();
                argb = palette[index];
            } else if (bits == 16) {
                const uint32_t pixel = static_cast<uint32_t>(row[x * 2]) |
                                       (static_cast<uint32_t>(row[x * 2 + 1]) << 8);
                argb = maskedPixel(pixel, redMask, greenMask, blueMask, alphaMask);
            } else if (bits == 24) {
                const uint8_t *pixel = row + x * 3;
                argb = 0xFF000000u | (static_cast<uint32_t>(pixel[2]) << 16) |
                       (static_cast<uint32_t>(pixel[1]) << 8) | pixel[0];
            } else {
                const uint8_t *bytes = row + x * 4;
                const uint32_t pixel = static_cast<uint32_t>(bytes[0]) |
                                       (static_cast<uint32_t>(bytes[1]) << 8) |
                                       (static_cast<uint32_t>(bytes[2]) << 16) |
                                       (static_cast<uint32_t>(bytes[3]) << 24);
                argb = maskedPixel(pixel, redMask, greenMask, blueMask, alphaMask);
            }
            image.setPixel(x, y, argb);
        }
    }
    return image;
}

} // namespace

PkImageFileDecoderHandler pkBmpImageCodecHandler()
{
    return {
        "qt.bmp", 900, {"bmp"},
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return isBmp(data, size);
        },
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return decodeBmp(data, size);
        }
    };
}
