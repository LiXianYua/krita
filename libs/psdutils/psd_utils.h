/*
 *  SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PSD_UTILS_H
#define PSD_UTILS_H


#include <PkGlobal.h>
#include <PkStream.h>
#include <PkString.h>
#include <PkAuxTypes.h>
#include <PkVariant.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <string>
#include <psd.h>
#include <resources/KoPattern.h>
#include <type_traits>

/**
 * Writing functions.
 */

// ── QtEndian 的自写替代（语义对齐 Qt 5.15 qTo*/qFrom*）────────────────
// S 线剥离后 psdutils 不再依赖 QtEndian；asl（Task 3）的 qFromBigEndian 等
// 换用这批。宽度覆盖 1/2/4/8 字节整数（含符号，按位交换后以原类型返回）。
// 在小端主机上 psdFromLittleEndian/psdToLittleEndian 是恒等；big-endian
// 变体做字节反序。转换用 memcpy 往返，不触碰严格别名规则。
inline bool psdIsLittleEndian()
{
    const std::uint16_t one = 1;
    return *reinterpret_cast<const std::uint8_t *>(&one) == 1;
}

template<typename T>
inline T psdSwapBytes(T value)
{
    static_assert(std::is_integral<T>::value, "psdSwapBytes requires an integral type");
    if (sizeof(T) == 1) {
        return value;
    }
    char tmp[sizeof(T)];
    std::memcpy(tmp, &value, sizeof(T));
    std::reverse(tmp, tmp + sizeof(T));
    T out;
    std::memcpy(&out, tmp, sizeof(T));
    return out;
}

template<typename T>
inline T psdFromBigEndian(T value)
{
    return psdIsLittleEndian() ? psdSwapBytes(value) : value;
}

template<typename T>
inline T psdToBigEndian(T value)
{
    return psdFromBigEndian(value);
}

template<typename T>
inline T psdFromLittleEndian(T value)
{
    return psdIsLittleEndian() ? value : psdSwapBytes(value);
}

template<typename T>
inline T psdToLittleEndian(T value)
{
    return psdFromLittleEndian(value);
}

// Latin-1 编码/解码（等价于 Qt 的 toLatin1()/fromLatin1()）。
// 编码：码元 < 256 直接取其低字节，其余码元写成 '?'（与 Qt toLatin1 一致）。
// 解码：字节值即码元值。
inline std::string psdToLatin1(const PkString &s)
{
    std::string out;
    out.reserve(static_cast<std::size_t>(s.size()));
    for (int i = 0; i < s.size(); ++i) {
        const char16_t c = s.at(i);
        out.push_back(static_cast<char>(c < 256 ? static_cast<unsigned char>(c) : '?'));
    }
    return out;
}

inline PkString psdFromLatin1(const char *data, int len)
{
    std::u16string u16;
    u16.reserve(static_cast<std::size_t>(len > 0 ? len : 0));
    for (int i = 0; i < len; ++i) {
        u16.push_back(static_cast<char16_t>(static_cast<unsigned char>(data[i])));
    }
    return PkVariant::PkFromStringCodeUnits(u16).toString();
}

inline bool psdwriteBE(PkStream &io, const quint8 &v)
{
    const std::array<quint8, 2> val = {v};
    const qint64 written = io.write(reinterpret_cast<const char *>(val.data()), 1);
    return written == 1;
}

inline bool psdwriteLE(PkStream &io, const quint8 &v)
{
    const std::array<quint8, 2> val = {v};
    const qint64 written = io.write(reinterpret_cast<const char *>(val.data()), 1);
    return written == 1;
}

inline bool psdwriteBE(PkStream &io, const quint16 &v)
{
    const std::array<quint8, 2> val = {
        quint8(v >> 8U),
        quint8(v),
    };
    const qint64 written = io.write(reinterpret_cast<const char *>(val.data()), 2);
    return written == 2;
}

inline bool psdwriteLE(PkStream &io, const quint16 &v)
{
    const std::array<quint8, 2> val = {
        quint8(v),
        quint8(v >> 8U),
    };
    const qint64 written = io.write(reinterpret_cast<const char *>(val.data()), 2);
    return written == 2;
}

inline bool psdwriteBE(PkStream &io, const quint32 &v)
{
    const std::array<quint8, 4> val = {
        quint8(v >> 24U),
        quint8(v >> 16U),
        quint8(v >> 8U),
        quint8(v),
    };
    const qint64 written = io.write(reinterpret_cast<const char *>(val.data()), 4);
    return written == 4;
}

inline bool psdwriteLE(PkStream &io, const quint32 &v)
{
    const std::array<quint8, 4> val = {
        quint8(v),
        quint8(v >> 8U),
        quint8(v >> 16U),
        quint8(v >> 24U),
    };
    const qint64 written = io.write(reinterpret_cast<const char *>(val.data()), 4);
    return written == 4;
}

inline bool psdwriteBE(PkStream &io, const quint64 &v)
{
    const std::array<quint8, 8> val = {
        quint8(v >> 56U),
        quint8(v >> 48U),
        quint8(v >> 40U),
        quint8(v >> 32U),
        quint8(v >> 24U),
        quint8(v >> 16U),
        quint8(v >> 8U),
        quint8(v),
    };
    const qint64 written = io.write(reinterpret_cast<const char *>(val.data()), 8);
    return written == 8;
}

inline bool psdwriteLE(PkStream &io, const quint64 &v)
{
    const std::array<quint8, 8> val = {
        quint8(v),
        quint8(v >> 8U),
        quint8(v >> 16U),
        quint8(v >> 24U),
        quint8(v >> 32U),
        quint8(v >> 40U),
        quint8(v >> 48U),
        quint8(v >> 56U),
    };
    const qint64 written = io.write(reinterpret_cast<const char *>(val.data()), 8);
    return written == 8;
}

/**
 * Templated writing fallbacks for non-integral types.
 */

template<typename T>
inline bool psdwriteBE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint8), T &> v)
{
    return psdwriteBE(io, reinterpret_cast<quint8 &>(v));
}

template<typename T>
inline bool psdwriteBE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint16), T &> v)
{
    return psdwriteBE(io, reinterpret_cast<quint16 &>(v));
}

template<typename T>
inline bool psdwriteBE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint32), T &> v)
{
    return psdwriteBE(io, reinterpret_cast<quint32 &>(v));
}

template<typename T>
inline bool psdwriteBE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint64), T &> v)
{
    return psdwriteBE(io, reinterpret_cast<quint64 &>(v));
}

template<typename T>
inline bool psdwriteLE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint8), T &> v)
{
    return psdwriteLE(io, reinterpret_cast<quint8 &>(v));
}

template<typename T>
inline bool psdwriteLE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint16), T &> v)
{
    return psdwriteLE(io, reinterpret_cast<quint16 &>(v));
}

template<typename T>
inline bool psdwriteLE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint32), T &> v)
{
    return psdwriteLE(io, reinterpret_cast<quint32 &>(v));
}

template<typename T>
inline bool psdwriteLE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint64), T &> v)
{
    return psdwriteLE(io, reinterpret_cast<quint64 &>(v));
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian, typename T>
inline std::enable_if_t<std::is_arithmetic<T>::value, bool> psdwrite(PkStream &io, T v)
{
    if (byteOrder == psd_byte_order::psdLittleEndian) {
        return psdwriteLE<T>(io, v);
    } else {
        return psdwriteBE<T>(io, v);
    }
}

inline bool psdwrite(PkStream &io, const PkString &s)
{
    const std::string b = psdToLatin1(s);
    const qint64 written = io.write(b.data(), static_cast<qint64>(b.size()));
    return written == static_cast<qint64>(b.size());
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline bool psdwrite_pascalstring(PkStream &io, const PkString &s)
{
    Q_ASSERT(s.size() < 256);
    Q_ASSERT(s.size() >= 0);
    if (s.size() < 0 || s.size() > 255)
        return false;

    if (s.isEmpty()) {
        psdwrite<byteOrder>(io, quint8(0));
        psdwrite<byteOrder>(io, quint8(0));
        return true;
    }

    quint8 length = static_cast<quint8>(s.size());
    psdwrite<byteOrder>(io, length);

    const std::string b = psdToLatin1(s);
    const qint64 written = io.write(b.data(), length);
    if (written != length)
        return false;

    if ((length & 0x01) != 0) {
        return psdwrite<byteOrder>(io, quint8(0));
    }

    return true;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline bool psdwrite_pascalstring(PkStream &io, const PkString &s, int padding)
{
    Q_ASSERT(s.size() < 256);
    Q_ASSERT(s.size() >= 0);
    if (s.size() < 0 || s.size() > 255)
        return false;

    if (s.isEmpty()) {
        psdwrite<byteOrder>(io, quint8(0));
        psdwrite<byteOrder>(io, quint8(0));
        return true;
    }
    quint8 length = static_cast<quint8>(s.size());
    psdwrite<byteOrder>(io, length);

    const std::string b = psdToLatin1(s);
    const qint64 written = io.write(b.data(), length);
    if (written != length)
        return false;

    // If the total length (length byte + content) is not a multiple of padding, add zeroes to pad
    length++;
    if ((length % padding) != 0) {
        for (int i = 0; i < (padding - (length % padding)); i++) {
            psdwrite<byteOrder>(io, quint8(0));
        }
    }

    return true;
}

inline bool psdpad(PkStream &io, quint32 padding)
{
    for (quint32 i = 0; i < padding; i++) {
        const bool written = io.putChar('\0');
        if (!written)
            return false;
    }
    return true;
}

/**
 * Reading functions.
 */

inline bool psdreadBE(PkStream &io, quint8 &v)
{
    std::array<quint8, 1> data;
    qint64 read = io.read(reinterpret_cast<char *>(data.data()), 1);
    if (read != 1)
        return false;
    v = data[0];
    return true;
}

inline bool psdreadLE(PkStream &io, quint8 &v)
{
    std::array<quint8, 1> data;
    qint64 read = io.read(reinterpret_cast<char *>(data.data()), 1);
    if (read != 1)
        return false;
    v = data[0];
    return true;
}

inline bool psdreadBE(PkStream &io, quint16 &v)
{
    std::array<quint8, 2> data;
    qint64 read = io.read(reinterpret_cast<char *>(data.data()), 2);
    if (read != 2)
        return false;
    v = quint16((quint16(data[0]) << 8U) | data[1]);
    return true;
}

inline bool psdreadLE(PkStream &io, quint16 &v)
{
    std::array<quint8, 2> data;
    qint64 read = io.read(reinterpret_cast<char *>(data.data()), 2);
    if (read != 2)
        return false;
    v = quint16((quint16(data[1]) << 8U) | data[0]);
    return true;
}

inline bool psdreadBE(PkStream &io, quint32 &v)
{
    std::array<quint8, 4> data;
    qint64 read = io.read(reinterpret_cast<char *>(data.data()), 4);
    if (read != 4)
        return false;
    v = (quint32(data[0]) << 24U) | (quint32(data[1]) << 16U) | (quint32(data[2]) << 8U) | data[3];
    return true;
}

inline bool psdreadLE(PkStream &io, quint32 &v)
{
    std::array<quint8, 4> data;
    qint64 read = io.read(reinterpret_cast<char *>(data.data()), 4);
    if (read != 4)
        return false;
    v = (quint32(data[3]) << 24U) | (quint32(data[2]) << 16U) | (quint32(data[1]) << 8U) | data[0];
    return true;
}

inline bool psdreadBE(PkStream &io, quint64 &v)
{
    std::array<quint8, 8> data;
    qint64 read = io.read(reinterpret_cast<char *>(data.data()), 8);
    if (read != 8)
        return false;
    v = (quint64(data[0]) << 56U) | (quint64(data[1]) << 48U) | (quint64(data[2]) << 40U) | (quint64(data[3]) << 32U) | (quint64(data[4]) << 24U)
        | (quint64(data[5]) << 16U) | (quint64(data[6]) << 8U) | data[7];
    return true;
}

inline bool psdreadLE(PkStream &io, quint64 &v)
{
    std::array<quint8, 8> data;
    qint64 read = io.read(reinterpret_cast<char *>(data.data()), 8);
    if (read != 8)
        return false;
    v = (quint64(data[7]) << 56U) | (quint64(data[6]) << 48U) | (quint64(data[5]) << 40U) | (quint64(data[4]) << 32U) | (quint64(data[3]) << 24U)
        | (quint64(data[2]) << 16U) | (quint64(data[1]) << 8U) | data[0];
    return true;
}

/**
 * Templated reading fallbacks for non-integral types.
 */

template<typename T>
inline bool psdreadBE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint8), T &> v)
{
    return psdreadBE(io, reinterpret_cast<quint8 &>(v));
}

template<typename T>
inline bool psdreadBE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint16), T &> v)
{
    return psdreadBE(io, reinterpret_cast<quint16 &>(v));
}

template<typename T>
inline bool psdreadBE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint32), T &> v)
{
    return psdreadBE(io, reinterpret_cast<quint32 &>(v));
}

template<typename T>
inline bool psdreadBE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint64), T &> v)
{
    return psdreadBE(io, reinterpret_cast<quint64 &>(v));
}

template<typename T>
inline bool psdreadLE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint8), T &> v)
{
    return psdreadLE(io, reinterpret_cast<quint8 &>(v));
}

template<typename T>
inline bool psdreadLE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint16), T &> v)
{
    return psdreadLE(io, reinterpret_cast<quint16 &>(v));
}

template<typename T>
inline bool psdreadLE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint32), T &> v)
{
    return psdreadLE(io, reinterpret_cast<quint32 &>(v));
}

template<typename T>
inline bool psdreadLE(PkStream &io, std::enable_if_t<sizeof(T) == sizeof(quint64), T &> v)
{
    return psdreadLE(io, reinterpret_cast<quint64 &>(v));
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian, typename T>
inline std::enable_if_t<std::is_arithmetic<T>::value, bool> psdread(PkStream &io, T &v)
{
    if (byteOrder == psd_byte_order::psdLittleEndian) {
        return psdreadLE<T>(io, v);
    } else {
        return psdreadBE<T>(io, v);
    }
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline PkByteArray psdreadBytes(PkStream &io, qint64 v)
{
    if (v <= 0) {
        return PkByteArray();
    }
    std::string buf(static_cast<std::size_t>(v), '\0');
    const qint64 read = io.read(&buf[0], v);
    PkByteArray b(buf.data(), static_cast<int>(read));
    if (byteOrder == psd_byte_order::psdLittleEndian && b.size() > 1) {
        // 空/单字节 buffer 的 data()+size() 指针算术在 size==0 时可能是 nullptr+0
        // （PkByteArray 空对象 mutable data() 语义），size>1 才做反序（1 字节反序是恒等）。
        std::reverse(b.data(), b.data() + b.size());
    }
    return b;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline bool psdread_pascalstring(PkStream &io, PkString &s, qint64 padding)
{
    quint8 length;
    if (!psdread<byteOrder>(io, length)) {
        return false;
    }

    if (length == 0) {
        // read the padding
        for (qint64 i = 0; i < padding - 1; ++i) {
            io.seek(io.pos() + 1);
        }
        return true;
    }

    std::string chars(static_cast<std::size_t>(length), '\0');
    const qint64 read = io.read(&chars[0], length);
    if (read != length) {
        return false;
    }

    // read padding byte
    quint32 paddedLength = length + 1;
    if (padding > 0) {
        while (paddedLength % padding != 0) {
            if (!io.seek(io.pos() + 1)) {
                return false;
            }
            paddedLength++;
        }
    }

    s.append(psdFromLatin1(chars.data(), static_cast<int>(chars.size())));
    if (s.size() > 0 && s.at(s.size() - 1) == 0x20) {
        s = s.left(s.size() - 1);
    }

    return true;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline bool psdread_unicodestring(PkStream &io, PkString &s)
{
    quint32 stringlen;
    if (!psdread<byteOrder>(io, stringlen)) {
        return false;
    }

    std::u16string u16(static_cast<std::size_t>(stringlen), 0x20);

    for (quint32 i = 0; i < stringlen; ++i) {
        quint16 ch(0);
        if (!psdread<byteOrder>(io, ch)) {
            return false;
        }

        // XXX: this makes it possible to append garbage
        if (ch != 0) {
            u16[i] = static_cast<char16_t>(ch);
        }
    }
    if (u16.size() > 0 && u16.back() == 0x20) {
        u16.pop_back();
    }

    s = PkVariant::PkFromStringCodeUnits(u16).toString();
    return true;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline bool psd_read_blendmode(PkStream &io, PkString &blendModeKey)
{
    PkByteArray b(psdreadBytes<byteOrder>(io, 4));
    if (b.size() != 4 || std::memcmp(b.constData(), "8BIM", 4) != 0) {
        return false;
    }
    const PkByteArray keyBytes(psdreadBytes<byteOrder>(io, 4));
    blendModeKey = psdFromLatin1(keyBytes.constData(), keyBytes.size());
    if (blendModeKey.size() != 4) {
        return false;
    }
    return true;
}

// Copied from Scribus decodePSDfloat in scimgdataloader.cpp by Franz Schmid
template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline double psdreadFixedPoint(PkStream &io)
{
    quint32 data;
    double ret = 0.0;

    if (psdread<byteOrder>(io, data)) {
        char man = (data & 0xFF000000) >> 24;
        if (man >= 0)
        {
            ret = (data & 0x00FFFFFF) / 16777215.0;
            ret = (ret + man);
        }
        else
        {
            ret = (~data & 0x00FFFFFF) / 16777215.0;
            ret = (ret + ~man) * -1;
        }
    }
    return ret;
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline void psdwriteFixedPoint(PkStream &io, double val)
{
    qint32 data;
    double max24 = 16777215.0;
    // val is bound to -16 to +16

    qint32 man = qint32(val);
    quint32 frac= quint32(fabs(val - man) * max24);
    data = (qAbs(man) << 24) | frac;
    if (val < 0) {
        data *= -1;
    };

    psdwrite<byteOrder>(io, data);
}

#endif // PSD_UTILS_H
