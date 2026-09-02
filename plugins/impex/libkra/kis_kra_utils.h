/* This file is part of the KDE project
 * Copyright 2011 (C) Silvio Heinrich <plassy@web.de>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef _KIS_KRA_UTILS_
#define _KIS_KRA_UTILS_

#include <PkString.h>
#include <PkBitArray.h>
#include <PkByteArray.h>

#include <string>

namespace KRA {

PkString   flagsToString(const PkBitArray& flags, int size=-1, char trueToken='1', char falseToken='0', bool defaultTrue=true);
PkBitArray stringToFlags(const PkString& string, int size=-1, char token='0', bool defaultTrue=true);

// 字节数组的 toBase64 / fromBase64 的零 Qt 对应（PkByteArray 无 Base64，
// 本地实现，复制自 libs/global/KoProperties.cpp 的文件局部 helper）。
std::string base64Encode(const PkByteArray &data);
PkByteArray base64Decode(const std::string &s);

}

#endif // _KIS_KRA_UTILS_
