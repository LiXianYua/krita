/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_asl_byte_utils.h"

#include <zlib.h>

#include <string>
#include <vector>

#include "PkAuxTypes.h"
#include "PkString.h"

namespace {

const char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

int base64Index(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

// Qt 5.15 线格式：4 字节大端原始长度 + zlib 数据。
quint32 readBigEndianU32(const char* src)
{
    return (static_cast<quint32>(static_cast<unsigned char>(src[0])) << 24) |
           (static_cast<quint32>(static_cast<unsigned char>(src[1])) << 16) |
           (static_cast<quint32>(static_cast<unsigned char>(src[2])) << 8) |
           (static_cast<quint32>(static_cast<unsigned char>(src[3])));
}

void writeBigEndianU32(char* dst, quint32 v)
{
    dst[0] = static_cast<char>((v >> 24) & 0xFF);
    dst[1] = static_cast<char>((v >> 16) & 0xFF);
    dst[2] = static_cast<char>((v >> 8) & 0xFF);
    dst[3] = static_cast<char>(v & 0xFF);
}

} // namespace

PkString pkToBase64(const PkByteArray &data)
{
    const int size = data.size();
    const char* src = data.constData();

    std::string out;
    out.reserve(((size + 2) / 3) * 4);

    int i = 0;
    while (i < size) {
        quint32 chunk = 0;
        int n = 0;
        for (; n < 3 && i < size; ++n, ++i) {
            chunk = (chunk << 8) | static_cast<unsigned char>(src[i]);
        }
        if (n == 0) break;

        // n 已把 chunk 拼满到恰好 n 字节（每字节 8 bit 左移进来），
        // 从最高位开始取 6 bit 一组。
        const int totalBits = n * 8;
        int pos = totalBits - 6;
        while (pos >= 0) {
            const int idx = (chunk >> pos) & 0x3F;
            out.push_back(kBase64Alphabet[idx]);
            pos -= 6;
        }
        const int padding = 3 - n;
        for (int p = 0; p < padding; ++p) {
            out.push_back('=');
        }
    }

    return PkString(out.c_str());
}

PkByteArray pkFromBase64(const PkString &encoded)
{
    const std::string text = encoded.PkToUtf8();

    PkByteArray out;
    out.resize(0);

    quint32 value = 0;
    int nbits = 0;

    std::vector<char> decoded;
    decoded.reserve((text.size() * 3) / 4 + 4);

    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (c == '=') {
            break; // 填充，之后的都忽略
        }
        const int v = base64Index(c);
        if (v < 0) {
            continue; // 宽松解析：跳过非法字符（与 Qt fromBase64 一致）
        }
        value = (value << 6) | static_cast<quint32>(v);
        nbits += 6;
        if (nbits >= 8) {
            nbits -= 8;
            decoded.push_back(static_cast<char>((value >> nbits) & 0xFF));
        }
    }

    if (!decoded.empty()) {
        out.resize(static_cast<int>(decoded.size()));
        for (std::size_t i = 0; i < decoded.size(); ++i) {
            out.data()[i] = decoded[i];
        }
    }

    return out;
}

PkString pkToHex(const PkByteArray &data)
{
    const char kHex[] = "0123456789abcdef";
    const int size = data.size();
    const char* src = data.constData();

    std::string out;
    out.reserve(size * 2);
    for (int i = 0; i < size; ++i) {
        const unsigned char c = static_cast<unsigned char>(src[i]);
        out.push_back(kHex[c >> 4]);
        out.push_back(kHex[c & 0x0F]);
    }

    return PkString(out.c_str());
}

PkByteArray pkQCompress(const PkByteArray &data)
{
    const int nbytes = data.size();
    if (nbytes == 0) {
        return PkByteArray();
    }

    const uLong len = compressBound(static_cast<uLong>(nbytes));
    PkByteArray out;
    out.resize(static_cast<int>(len) + 4);

    uLong destLen = len;
    const int res = compress2(
        reinterpret_cast<uchar*>(out.data()) + 4, &destLen,
        reinterpret_cast<const uchar*>(data.constData()), static_cast<uLong>(nbytes),
        -1 /* 压缩级别：-1 即默认（Z_DEFAULT_COMPRESSION） */);

    if (res != Z_OK) {
        return PkByteArray();
    }

    out.resize(static_cast<int>(destLen) + 4);
    writeBigEndianU32(out.data(), static_cast<quint32>(nbytes));
    return out;
}

PkByteArray pkQUncompress(const PkByteArray &data)
{
    const int nbytes = data.size();
    if (nbytes <= 4) {
        return PkByteArray();
    }

    const char* src = data.constData();
    const quint32 unpackedSize = readBigEndianU32(src);
    if (unpackedSize == 0) {
        return PkByteArray();
    }

    PkByteArray out;
    out.resize(static_cast<int>(unpackedSize));

    uLong destLen = static_cast<uLong>(unpackedSize);
    const int res = uncompress(
        reinterpret_cast<uchar*>(out.data()), &destLen,
        reinterpret_cast<const uchar*>(src) + 4, static_cast<uLong>(nbytes - 4));

    if (res != Z_OK) {
        return PkByteArray();
    }

    out.resize(static_cast<int>(destLen));
    return out;
}
