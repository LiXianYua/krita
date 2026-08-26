/* This file is part of the KDE project
 * Copyright 2011 (C) Silvio Heinrich <plassy@web.de>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_kra_utils.h"

#include <algorithm>
#include <string>

PkString KRA::flagsToString(const PkBitArray& flags, int size, char trueToken, char falseToken, bool defaultTrue)
{
    size = (size < 0) ? flags.count() : size;

    // 对拍原 Qt 语义：字符串(size, fill) 先建 size 个 defaultTrue 位，再覆盖
    // [0, min(size, flags.count()))。PkString 无 (int, char) 构造，用 std::string
    // 逐位拼（ASCII trueToken/falseToken 的 UTF-8 单字节，PkFromUtf8 等价）。
    std::string s;
    s.reserve(static_cast<std::size_t>(size));
    for (int i = 0; i < size; ++i) {
        const bool bit = (i < flags.count()) ? flags.at(i) : defaultTrue;
        s.push_back(bit ? trueToken : falseToken);
    }
    return PkString::PkFromUtf8(s.c_str(), static_cast<int>(s.size()));
}

PkBitArray KRA::stringToFlags(const PkString& string, int size, char token, bool defaultTrue)
{
    size = (size < 0) ? string.size() : size;

    PkBitArray flags(size, defaultTrue);

    // 对拍原 Qt 语义：flags[i] = (string[i]==token) ? !defaultTrue : defaultTrue。
    // PkBitArray 用 setBit(i, bool)，无 operator[] 赋值。
    for (int i = 0; i < std::min(size, string.size()); ++i) {
        flags.setBit(i, (string[i] == token) ? !defaultTrue : defaultTrue);
    }

    return flags;
}

namespace {

const char kKraB64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int kKraB64Value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

} // namespace

std::string KRA::base64Encode(const PkByteArray &data)
{
    std::string out;
    const char *src = data.constData();
    const int len = data.size();
    for (int i = 0; i < len; i += 3) {
        const unsigned int n =
            (static_cast<unsigned char>(src[i]) << 16) |
            (i + 1 < len ? static_cast<unsigned char>(src[i + 1]) << 8 : 0u) |
            (i + 2 < len ? static_cast<unsigned char>(src[i + 2]) : 0u);
        out.push_back(kKraB64Alphabet[(n >> 18) & 0x3f]);
        out.push_back(kKraB64Alphabet[(n >> 12) & 0x3f]);
        out.push_back(i + 1 < len ? kKraB64Alphabet[(n >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < len ? kKraB64Alphabet[n & 0x3f] : '=');
    }
    return out;
}

PkByteArray KRA::base64Decode(const std::string &s)
{
    std::vector<unsigned char> out;
    unsigned int buf = 0;
    int bits = 0;
    for (char c : s) {
        if (c == '=') break;
        const int v = kKraB64Value(c);
        if (v < 0) break;
        buf = (buf << 6) | static_cast<unsigned int>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((buf >> bits) & 0xff));
        }
    }
    return PkByteArray(reinterpret_cast<const char *>(out.data()),
                       static_cast<int>(out.size()));
}
