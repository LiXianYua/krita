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
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

std::vector<std::string> quotedStrings(const std::string &text)
{
    std::vector<std::string> result;
    for (std::size_t position = 0; position < text.size();) {
        position = text.find('"', position);
        if (position == std::string::npos) break;
        ++position;
        std::string value;
        bool closed = false;
        while (position < text.size()) {
            const char character = text[position++];
            if (character == '"') {
                closed = true;
                break;
            }
            if (character != '\\') {
                value.push_back(character);
                continue;
            }
            if (position >= text.size()) return {};
            char escaped = text[position++];
            if (escaped >= '0' && escaped <= '7') {
                unsigned octal = static_cast<unsigned>(escaped - '0');
                for (int digit = 1; digit < 3 && position < text.size() &&
                     text[position] >= '0' && text[position] <= '7'; ++digit) {
                    octal = octal * 8u + static_cast<unsigned>(text[position++] - '0');
                }
                value.push_back(static_cast<char>(octal));
            } else {
                value.push_back(escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped);
            }
        }
        if (!closed) return {};
        result.push_back(std::move(value));
    }
    return result;
}

bool hex(char character, unsigned &value)
{
    if (character >= '0' && character <= '9') value = static_cast<unsigned>(character - '0');
    else if (character >= 'a' && character <= 'f') value = static_cast<unsigned>(character - 'a' + 10);
    else if (character >= 'A' && character <= 'F') value = static_cast<unsigned>(character - 'A' + 10);
    else return false;
    return true;
}

bool parseHexComponent(const std::string &text, std::size_t begin, std::size_t digits, uint8_t &value)
{
    unsigned component = 0;
    for (std::size_t i = 0; i < digits; ++i) {
        unsigned digit = 0;
        if (!hex(text[begin + i], digit)) return false;
        component = component * 16u + digit;
    }
    const unsigned maximum = (1u << (4u * static_cast<unsigned>(digits))) - 1u;
    value = static_cast<uint8_t>((component * 255u + maximum / 2u) / maximum);
    return true;
}

bool color(const std::string &source, uint32_t &argb)
{
    std::string value = source;
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char c) {
        return !std::isspace(c);
    }));
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower == "none") {
        argb = 0;
        return true;
    }
    static const std::unordered_map<std::string, uint32_t> named {
        {"black", 0xFF000000u}, {"white", 0xFFFFFFFFu}, {"red", 0xFFFF0000u},
        {"green", 0xFF008000u}, {"blue", 0xFF0000FFu}, {"yellow", 0xFFFFFF00u},
        {"magenta", 0xFFFF00FFu}, {"cyan", 0xFF00FFFFu}, {"gray", 0xFF808080u},
        {"grey", 0xFF808080u}, {"lightblue", 0xFFADD8E6u},
        {"darkgreen", 0xFF006400u}, {"transparent", 0x00000000u}
    };
    const auto namedColor = named.find(lower);
    if (namedColor != named.end()) {
        argb = namedColor->second;
        return true;
    }
    if (value.empty() || value[0] != '#' ||
        (value.size() != 4 && value.size() != 7 && value.size() != 13)) return false;
    const std::size_t digits = (value.size() - 1) / 3;
    uint8_t red = 0, green = 0, blue = 0;
    if (!parseHexComponent(value, 1, digits, red) ||
        !parseHexComponent(value, 1 + digits, digits, green) ||
        !parseHexComponent(value, 1 + 2 * digits, digits, blue)) return false;
    argb = 0xFF000000u | (static_cast<uint32_t>(red) << 16) |
           (static_cast<uint32_t>(green) << 8) | blue;
    return true;
}

bool isXpm(const uint8_t *data, std::size_t size)
{
    if (!data || size < 8) return false;
    const std::string prefix(reinterpret_cast<const char *>(data), std::min<std::size_t>(size, 256));
    return prefix.find("XPM") != std::string::npos;
}

PkImage decodeXpm(const uint8_t *data, std::size_t size)
{
    if (!isXpm(data, size)) return PkImage();
    const std::string text(reinterpret_cast<const char *>(data), size);
    std::vector<std::string> lines = quotedStrings(text);
    if (lines.empty()) return PkImage();

    uint64_t width = 0, height = 0, count = 0, charsPerPixel = 0;
    std::istringstream header(lines[0]);
    if (!(header >> width >> height >> count >> charsPerPixel) ||
        width == 0 || height == 0 || count == 0 || charsPerPixel == 0 ||
        width > 32767 || height > 32767 || count > 65536 || charsPerPixel > 8 ||
        width * height > (512u * 1024u * 1024u) / 4u ||
        lines.size() < 1u + count + height) return PkImage();

    std::unordered_map<std::string, uint32_t> colors;
    colors.reserve(static_cast<std::size_t>(count));
    for (uint64_t index = 0; index < count; ++index) {
        const std::string &line = lines[1u + static_cast<std::size_t>(index)];
        if (line.size() < charsPerPixel) return PkImage();
        const std::string key = line.substr(0, static_cast<std::size_t>(charsPerPixel));
        const std::string attributes = line.substr(static_cast<std::size_t>(charsPerPixel));
        std::istringstream stream(attributes);
        std::vector<std::string> tokens;
        for (std::string token; stream >> token;) tokens.push_back(std::move(token));
        auto isField = [](const std::string &token) {
            return token == "c" || token == "g" || token == "g4" || token == "m" || token == "s";
        };
        std::size_t fieldIndex = tokens.size();
        for (const char *candidate : {"c", "g", "g4", "m"}) {
            const auto found = std::find(tokens.begin(), tokens.end(), candidate);
            if (found != tokens.end()) { fieldIndex = static_cast<std::size_t>(found - tokens.begin()); break; }
        }
        std::string selected;
        for (std::size_t token = fieldIndex + 1; token < tokens.size() && !isField(tokens[token]); ++token) {
            selected += tokens[token];
        }
        uint32_t argb = 0;
        if (selected.empty() || !color(selected, argb)) return PkImage();
        colors.emplace(key, argb);
    }

    PkImage image(static_cast<int>(width), static_cast<int>(height), PkImage::Format_ARGB32);
    if (image.isNull()) return PkImage();
    for (uint64_t y = 0; y < height; ++y) {
        const std::string &row = lines[1u + static_cast<std::size_t>(count + y)];
        if (row.size() < width * charsPerPixel) return PkImage();
        for (uint64_t x = 0; x < width; ++x) {
            const auto found = colors.find(row.substr(static_cast<std::size_t>(x * charsPerPixel),
                                                       static_cast<std::size_t>(charsPerPixel)));
            if (found == colors.end()) return PkImage();
            image.setPixel(static_cast<int>(x), static_cast<int>(y), found->second);
        }
    }
    return image;
}

} // namespace

PkImageFileDecoderHandler pkXpmImageCodecHandler()
{
    return {
        "qt.xpm", 900, {"xpm"},
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return isXpm(data, size);
        },
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return decodeXpm(data, size);
        }
    };
}
