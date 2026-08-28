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

#include <cctype>
#include <cstdint>
#include <limits>
#include <string>

namespace
{

constexpr std::size_t kMaximumDecodedBytes = 512u * 1024u * 1024u;

class Cursor
{
public:
    Cursor(const uint8_t *data, std::size_t size) : m_data(data), m_size(size) {}

    bool byte(uint8_t &value)
    {
        if (m_position >= m_size) return false;
        value = m_data[m_position++];
        return true;
    }

    bool integer(uint32_t &value)
    {
        uint8_t c = 0;
        for (;;) {
            if (!byte(c)) return false;
            if (c == '#') {
                while (byte(c) && c != '\n') {}
            } else if (!std::isspace(c)) {
                break;
            }
        }
        if (!std::isdigit(c)) return false;
        uint32_t result = 0;
        do {
            const uint32_t digit = static_cast<uint32_t>(c - '0');
            if (result > (std::numeric_limits<uint32_t>::max() - digit) / 10u) return false;
            result = result * 10u + digit;
            if (!byte(c)) {
                value = result;
                return true;
            }
        } while (std::isdigit(c));
        if (c == '#') {
            while (byte(c) && c != '\n') {}
        } else if (!std::isspace(c)) {
            return false;
        }
        value = result;
        return true;
    }

    bool bytes(uint8_t *output, std::size_t count)
    {
        if (count > m_size - m_position) return false;
        for (std::size_t i = 0; i < count; ++i) output[i] = m_data[m_position + i];
        m_position += count;
        return true;
    }

private:
    const uint8_t *m_data;
    std::size_t m_size;
    std::size_t m_position = 0;
};

bool dimensions(uint32_t width, uint32_t height)
{
    return width > 0 && height > 0 && width <= 32767u && height <= 32767u &&
           static_cast<uint64_t>(width) * height <= kMaximumDecodedBytes / 4u;
}

uint8_t scale(uint32_t value, uint32_t maximum)
{
    if (value > maximum) value = maximum;
    return static_cast<uint8_t>((static_cast<uint64_t>(value) * 65535u / maximum) >> 8);
}

bool isPnm(const uint8_t *data, std::size_t size)
{
    return data && size >= 3 && data[0] == 'P' && data[1] >= '1' && data[1] <= '6' &&
           std::isspace(data[2]);
}

PkImage decodePnm(const uint8_t *data, std::size_t size)
{
    if (!isPnm(data, size)) return PkImage();
    Cursor cursor(data + 3, size - 3);
    uint32_t width = 0, height = 0, maximum = 1;
    if (!cursor.integer(width) || !cursor.integer(height) ||
        ((data[1] != '1' && data[1] != '4') && !cursor.integer(maximum)) ||
        !dimensions(width, height) || maximum == 0 || maximum > 65535u) {
        return PkImage();
    }

    PkImage image(static_cast<int>(width), static_cast<int>(height), PkImage::Format_ARGB32);
    if (image.isNull()) return PkImage();
    const bool ascii = data[1] <= '3';

    for (uint32_t y = 0; y < height; ++y) {
        if (data[1] == '4') {
            const std::size_t rowBytes = (width + 7u) / 8u;
            for (std::size_t byteIndex = 0; byteIndex < rowBytes; ++byteIndex) {
                uint8_t packed = 0;
                if (!cursor.byte(packed)) return PkImage();
                for (unsigned bit = 0; bit < 8 && byteIndex * 8u + bit < width; ++bit) {
                    const bool black = (packed & (0x80u >> bit)) != 0;
                    image.setPixel(static_cast<int>(byteIndex * 8u + bit), static_cast<int>(y),
                                   black ? 0xFF000000u : 0xFFFFFFFFu);
                }
            }
            continue;
        }

        for (uint32_t x = 0; x < width; ++x) {
            uint32_t red = 0, green = 0, blue = 0;
            if (ascii) {
                if (!cursor.integer(red)) return PkImage();
                if (data[1] == '3') {
                    if (!cursor.integer(green) || !cursor.integer(blue)) return PkImage();
                } else {
                    green = blue = red;
                }
            } else {
                auto component = [&cursor, maximum](uint32_t &value) {
                    uint8_t high = 0, low = 0;
                    if (!cursor.byte(high)) return false;
                    if (maximum < 256u) {
                        value = high;
                        return true;
                    }
                    if (!cursor.byte(low)) return false;
                    value = (static_cast<uint32_t>(high) << 8) | low;
                    return true;
                };
                if (!component(red)) return PkImage();
                if (data[1] == '6') {
                    if (!component(green) || !component(blue)) return PkImage();
                } else {
                    green = blue = red;
                }
            }
            if (red > maximum || green > maximum || blue > maximum) return PkImage();
            if (data[1] == '1') {
                image.setPixel(static_cast<int>(x), static_cast<int>(y),
                               red ? 0xFF000000u : 0xFFFFFFFFu);
            } else {
                image.setPixel(static_cast<int>(x), static_cast<int>(y),
                    0xFF000000u | (static_cast<uint32_t>(scale(red, maximum)) << 16) |
                    (static_cast<uint32_t>(scale(green, maximum)) << 8) |
                    scale(blue, maximum));
            }
        }
    }
    return image;
}

} // namespace

PkImageFileDecoderHandler pkPnmImageCodecHandler()
{
    return {
        "qt.pnm", 900, {"pbm", "pgm", "ppm"},
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return isPnm(data, size);
        },
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return decodePnm(data, size);
        }
    };
}
