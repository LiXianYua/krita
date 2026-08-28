/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the plugins of the Qt Toolkit.
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

#include <cstdint>
#include <limits>
#include <vector>

namespace
{

constexpr uint64_t kMaximumPixels = (512u * 1024u * 1024u) / 4u;

uint16_t u16(const uint8_t *data) { return static_cast<uint16_t>(data[0] | (data[1] << 8)); }
uint32_t u32(const uint8_t *data) {
    return static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
           (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24);
}

bool range(std::size_t offset, std::size_t count, std::size_t size)
{
    return offset <= size && count <= size - offset;
}

bool isIco(const uint8_t *data, std::size_t size)
{
    return data && size >= 6 && u16(data) == 0 && (u16(data + 2) == 1 || u16(data + 2) == 2) &&
           u16(data + 4) > 0;
}

PkImage decodeDib(const uint8_t *data, std::size_t size)
{
    if (!data || size < 40) return PkImage();
    const uint32_t headerSize = u32(data);
    if (headerSize < 40 || headerSize > size) return PkImage();
    const int32_t width = static_cast<int32_t>(u32(data + 4));
    const int32_t combinedHeight = static_cast<int32_t>(u32(data + 8));
    const uint16_t planes = u16(data + 12);
    const uint16_t bits = u16(data + 14);
    const uint32_t compression = u32(data + 16);
    const uint32_t colorsUsed = u32(data + 32);
    if (width <= 0 || combinedHeight == 0 || combinedHeight == std::numeric_limits<int32_t>::min() ||
        planes != 1 || (bits != 1 && bits != 4 && bits != 8 && bits != 16 && bits != 24 && bits != 32) ||
        compression != 0) return PkImage();
    const int64_t absoluteHeight = combinedHeight < 0 ? -static_cast<int64_t>(combinedHeight) : combinedHeight;
    if ((absoluteHeight & 1) != 0) return PkImage();
    const int height = static_cast<int>(absoluteHeight / 2);
    if (height <= 0 || width > 256 || height > 256 ||
        static_cast<uint64_t>(width) * height > kMaximumPixels) return PkImage();
    const bool topDown = combinedHeight < 0;

    std::size_t position = headerSize;
    std::vector<uint32_t> palette;
    if (bits <= 8) {
        const uint32_t maximum = 1u << bits;
        const uint32_t count = colorsUsed ? colorsUsed : maximum;
        if (count == 0 || count > maximum || !range(position, static_cast<std::size_t>(count) * 4u, size)) {
            return PkImage();
        }
        palette.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            const uint8_t *entry = data + position + i * 4u;
            palette.push_back(0xFF000000u | (static_cast<uint32_t>(entry[2]) << 16) |
                              (static_cast<uint32_t>(entry[1]) << 8) | entry[0]);
        }
        position += static_cast<std::size_t>(count) * 4u;
    }

    const uint64_t xorRow64 = ((static_cast<uint64_t>(width) * bits + 31u) / 32u) * 4u;
    const uint64_t maskRow64 = ((static_cast<uint64_t>(width) + 31u) / 32u) * 4u;
    if (xorRow64 > std::numeric_limits<std::size_t>::max() ||
        maskRow64 > std::numeric_limits<std::size_t>::max()) return PkImage();
    const std::size_t xorRow = static_cast<std::size_t>(xorRow64);
    const std::size_t maskRow = static_cast<std::size_t>(maskRow64);
    const uint64_t xorBytes64 = xorRow64 * static_cast<uint64_t>(height);
    const uint64_t maskBytes64 = maskRow64 * static_cast<uint64_t>(height);
    if (xorBytes64 > size ||
        !range(position, static_cast<std::size_t>(xorBytes64), size)) {
        return PkImage();
    }
    const uint8_t *xorData = data + position;
    const uint8_t *maskData = nullptr;
    if (bits != 32) {
        if (maskBytes64 > size ||
            !range(position + static_cast<std::size_t>(xorBytes64),
                   static_cast<std::size_t>(maskBytes64), size)) {
            return PkImage();
        }
        maskData = xorData + static_cast<std::size_t>(xorBytes64);
    }

    PkImage image(width, height, PkImage::Format_ARGB32);
    if (image.isNull()) return PkImage();
    for (int sourceRow = 0; sourceRow < height; ++sourceRow) {
        const int y = topDown ? sourceRow : height - 1 - sourceRow;
        const uint8_t *row = xorData + static_cast<std::size_t>(sourceRow) * xorRow;
        for (int x = 0; x < width; ++x) {
            uint32_t argb = 0;
            if (bits == 32) {
                const uint8_t *pixel = row + x * 4;
                if (pixel[3] != 0) {
                    argb = (static_cast<uint32_t>(pixel[3]) << 24) |
                           (static_cast<uint32_t>(pixel[2]) << 16) |
                           (static_cast<uint32_t>(pixel[1]) << 8) | pixel[0];
                }
            } else if (bits == 24) {
                const uint8_t *pixel = row + x * 3;
                argb = 0xFF000000u | (static_cast<uint32_t>(pixel[2]) << 16) |
                       (static_cast<uint32_t>(pixel[1]) << 8) | pixel[0];
            } else if (bits == 16) {
                const uint16_t pixel = static_cast<uint16_t>(row[x * 2]) |
                                       static_cast<uint16_t>(row[x * 2 + 1] << 8);
                argb = 0xFF000000u |
                       (static_cast<uint32_t>((pixel >> 10) & 0x1Fu) * 8u << 16) |
                       (static_cast<uint32_t>((pixel >> 5) & 0x1Fu) * 8u << 8) |
                       (static_cast<uint32_t>(pixel & 0x1Fu) * 8u);
            } else {
                unsigned index = 0;
                if (bits == 8) index = row[x];
                else if (bits == 4) index = (x & 1) ? row[x / 2] & 0x0Fu : row[x / 2] >> 4;
                else index = (row[x / 8] >> (7 - (x & 7))) & 1u;
                if (index >= palette.size()) return PkImage();
                argb = palette[index];
            }
            image.setPixel(x, y, argb);
        }
    }

    // Qt's ICO reader treats a 32-bit XOR bitmap as authoritative ARGB and
    // does not consume its legacy AND mask.  Lower depths obtain alpha from
    // the mask.
    if (bits == 32) return image;
    for (int sourceRow = 0; sourceRow < height; ++sourceRow) {
        const int y = topDown ? sourceRow : height - 1 - sourceRow;
        const uint8_t *row = maskData + static_cast<std::size_t>(sourceRow) * maskRow;
        for (int x = 0; x < width; ++x) {
            const bool transparent = (row[x / 8] & (0x80u >> (x & 7))) != 0;
            uint32_t pixel = image.pixel(x, y);
            if (transparent) pixel &= 0x00FFFFFFu;
            else pixel |= 0xFF000000u;
            image.setPixel(x, y, pixel);
        }
    }
    return image;
}

PkImage decodeIco(const uint8_t *data, std::size_t size)
{
    if (!isIco(data, size)) return PkImage();
    const uint16_t count = u16(data + 4);
    if (count > 1024 || !range(6, static_cast<std::size_t>(count) * 16u, size)) return PkImage();
    // QtIcoHandler starts at current image index zero and an ordinary read
    // never scans for a larger or deeper directory entry.
    const std::size_t entry = 6u;
    const uint32_t bytes = u32(data + entry + 8);
    const uint32_t offset = u32(data + entry + 12);
    if (bytes == 0 || !range(offset, bytes, size)) return PkImage();
    const uint8_t *payload = data + offset;
    if (bytes >= 8 && payload[0] == 0x89 && payload[1] == 'P' && payload[2] == 'N' && payload[3] == 'G') {
        return PkImageFileDecoder::decode(payload, bytes, "embedded.png");
    }
    return decodeDib(payload, bytes);
}

} // namespace

PkImageFileDecoderHandler pkIcoImageCodecHandler()
{
    return {
        "qt.ico", 900, {"ico", "cur"},
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return isIco(data, size);
        },
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return decodeIco(data, size);
        }
    };
}
