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
#include <cctype>
#include <cstdint>
#include <limits>
#include <regex>
#include <string>

namespace
{

bool parseDimensions(const std::string &text, int &width, int &height)
{
    const std::string header = text.substr(0, 4096);
    const std::regex define(R"(^[ \t]*#define[ \t]+[A-Za-z0-9._]+[ \t]+([0-9]+)[ \t]*$)",
                            std::regex::ECMAScript);
    std::smatch match;
    std::size_t position = 0;
    int values[2] = {0, 0};
    for (int index = 0; index < 2; ++index) {
        std::string line;
        do {
            const std::size_t end = header.find('\n', position);
            if (end == std::string::npos) return false;
            line = header.substr(position, end - position);
            position = end + 1;
        } while (line.empty() || line[0] != '#');
        if (!std::regex_match(line, match, define)) return false;
        try {
            const unsigned long value = std::stoul(match[1].str());
            if (value == 0 || value > 32767) return false;
            values[index] = static_cast<int>(value);
        } catch (...) {
            return false;
        }
    }
    width = values[0];
    height = values[1];
    return static_cast<uint64_t>(width) * height <= (512u * 1024u * 1024u) / 4u;
}

bool hexDigit(char character, unsigned &value)
{
    if (character >= '0' && character <= '9') value = static_cast<unsigned>(character - '0');
    else if (character >= 'a' && character <= 'f') value = static_cast<unsigned>(character - 'a' + 10);
    else if (character >= 'A' && character <= 'F') value = static_cast<unsigned>(character - 'A' + 10);
    else return false;
    return true;
}

bool isXbm(const uint8_t *data, std::size_t size)
{
    if (!data || size < 16) return false;
    const std::string prefix(reinterpret_cast<const char *>(data), std::min<std::size_t>(size, 4096));
    int width = 0, height = 0;
    return parseDimensions(prefix, width, height) && prefix.find("0x") != std::string::npos;
}

PkImage decodeXbm(const uint8_t *data, std::size_t size)
{
    if (!data || size < 16) return PkImage();
    const std::string text(reinterpret_cast<const char *>(data), size);
    int width = 0, height = 0;
    if (!parseDimensions(text, width, height)) return PkImage();
    std::size_t position = text.find("0x");
    if (position == std::string::npos) return PkImage();

    PkImage image(width, height, PkImage::Format_ARGB32);
    if (image.isNull()) return PkImage();
    const std::size_t rowBytes = (static_cast<std::size_t>(width) + 7u) / 8u;
    for (int y = 0; y < height; ++y) {
        for (std::size_t bx = 0; bx < rowBytes; ++bx) {
            position = text.find("0x", position);
            if (position == std::string::npos || position + 4 > text.size()) return PkImage();
            unsigned high = 0, low = 0;
            if (!hexDigit(text[position + 2], high) || !hexDigit(text[position + 3], low)) {
                return PkImage();
            }
            const unsigned packed = (high << 4) | low;
            position += 4;
            for (unsigned bit = 0; bit < 8 && bx * 8u + bit < static_cast<std::size_t>(width); ++bit) {
                image.setPixel(static_cast<int>(bx * 8u + bit), y,
                               (packed & (1u << bit)) ? 0xFF000000u : 0xFFFFFFFFu);
            }
        }
    }
    return image;
}

} // namespace

PkImageFileDecoderHandler pkXbmImageCodecHandler()
{
    return {
        "qt.xbm", 900, {"xbm"},
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return isXbm(data, size);
        },
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return decodeXbm(data, size);
        }
    };
}
