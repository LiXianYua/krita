/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef LCMS_STRING_UTILS_H
#define LCMS_STRING_UTILS_H

#include <PkString.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace LcmsStringUtils
{
inline void appendUtf8CodePoint(std::string &output, std::uint32_t codePoint)
{
    if (codePoint <= 0x7f) {
        output.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7ff) {
        output.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else if (codePoint <= 0xffff) {
        output.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else {
        output.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        output.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
}

inline PkString fromWideString(const wchar_t *text)
{
    std::string utf8;
    for (std::size_t i = 0; text[i] != 0; ++i) {
        std::uint32_t codePoint = static_cast<std::uint32_t>(text[i]);
        if constexpr (sizeof(wchar_t) == 2) {
            if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
                const std::uint32_t low = static_cast<std::uint32_t>(text[i + 1]);
                if (low >= 0xdc00 && low <= 0xdfff) {
                    codePoint = 0x10000 + ((codePoint - 0xd800) << 10) + (low - 0xdc00);
                    ++i;
                }
            }
        }
        if (codePoint > 0x10ffff || (codePoint >= 0xd800 && codePoint <= 0xdfff)) {
            codePoint = 0xfffd;
        }
        appendUtf8CodePoint(utf8, codePoint);
    }
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}
} // namespace LcmsStringUtils

#endif // LCMS_STRING_UTILS_H
