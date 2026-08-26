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

    // 对拍原 Qt 语义：QString(size, fill) 先建 size 个 defaultTrue 位，再覆盖
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
