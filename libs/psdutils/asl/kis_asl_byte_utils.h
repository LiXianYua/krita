/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ASL_BYTE_UTILS_H
#define __KIS_ASL_BYTE_UTILS_H

#include "kritapsdutils_export.h"

class PkByteArray;
class PkString;

// ── asl 用的小字节工具 ──────────────────────────────────────────────
// 原字节容器提供 base64/hex 编码与「4 字节大端长度前缀 + zlib 数据」的压缩；
// 剥离后这些都不存在，这里补齐 asl 实际用到的几件。语义对齐 5.15：
//   - pkToBase64/pkFromBase64：标准 base64（RFC 4648），toBase64 产物即
//     fromBase64 输入；非法输入按原默认宽松解析（跳过非法字符）。
//   - pkToHex：小写十六进制（与原 toHex 默认一致）。
//   - pkQCompress/pkQUncompress：线格式 = 4 字节大端原始长度
//     前缀 + zlib deflate/inflate 数据。
KRITAPSDUTILS_EXPORT PkString pkToBase64(const PkByteArray &data);
KRITAPSDUTILS_EXPORT PkByteArray pkFromBase64(const PkString &encoded);
KRITAPSDUTILS_EXPORT PkString pkToHex(const PkByteArray &data);
KRITAPSDUTILS_EXPORT PkByteArray pkQCompress(const PkByteArray &data);
KRITAPSDUTILS_EXPORT PkByteArray pkQUncompress(const PkByteArray &data);

#endif /* __KIS_ASL_BYTE_UTILS_H */
