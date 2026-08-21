/*
    SPDX-FileCopyrightText: 2026 S-03-a
    SPDX-License-Identifier: LGPL-2.1-or-later

    PkUtf16 —— UTF-16BE 解码的零 Qt 替代（S 线剥 Qt 用）。
    消费方：resources/KoColorSet.cpp readUnicodeString（.ggr/.kpl/.pal 里的
    UTF-16BE 字符串块）。
    实现：每 2 字节一个大端 UTF-16 码元 → UTF-8；代理对（surrogate pair）正确
    合并；孤立代理按 U+FFFD 处理（对齐 Qt toUnicode 的宽容语义）。
 */

#ifndef PK_UTF16_H
#define PK_UTF16_H

#include <cstdint>
#include <string>

#include <PkString.h>       // PkString
#include <PkAuxTypes.h>     // PkByteArray

namespace {
inline void pkAppendUtf8(std::string &out, std::uint32_t cp)
{
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}
}

inline PkString pkUtf16BEToUtf8(const PkByteArray &utf16be)
{
    std::string utf8;
    const std::uint8_t *data = reinterpret_cast<const std::uint8_t *>(utf16be.data());
    const std::size_t size = static_cast<std::size_t>(utf16be.size());

    for (std::size_t i = 0; i + 1 < size; i += 2) {
        const std::uint32_t unit = (static_cast<std::uint32_t>(data[i]) << 8) | data[i + 1];
        if (unit >= 0xD800 && unit <= 0xDBFF) {
            // 高代理：期待下一个是低代理。
            if (i + 3 < size) {
                const std::uint32_t lo = (static_cast<std::uint32_t>(data[i + 2]) << 8) | data[i + 3];
                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                    const std::uint32_t cp = 0x10000 + ((unit - 0xD800) << 10) + (lo - 0xDC00);
                    pkAppendUtf8(utf8, cp);
                    i += 2;
                    continue;
                }
            }
            pkAppendUtf8(utf8, 0xFFFD);   // 孤立高代理
        } else if (unit >= 0xDC00 && unit <= 0xDFFF) {
            pkAppendUtf8(utf8, 0xFFFD);   // 孤立低代理
        } else {
            pkAppendUtf8(utf8, unit);
        }
    }
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

#endif // PK_UTF16_H
