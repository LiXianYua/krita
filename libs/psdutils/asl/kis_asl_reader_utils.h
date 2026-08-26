/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ASL_READER_UTILS_H
#define __KIS_ASL_READER_UTILS_H

#include "psd.h"
#include "psd_utils.h"

#include <algorithm>
#include <stdexcept>
#include <string>

#include <kis_debug.h>
#include "kis_asl_byte_utils.h"

/**
 * Default value for variable read from a file
 */

#define GARBAGE_VALUE_MARK 999

namespace KisAslReaderUtils
{
/**
 * Exception that is emitted when any parse error appear.
 * Thanks to KisOffsetOnExitVerifier parsing can be continued
 * most of the time, based on the offset values written in PSD.
 */

struct KRITAPSDUTILS_EXPORT ASLParseException : public std::runtime_error {
    ASLParseException(const PkString &msg)
        : std::runtime_error(psdToLatin1(msg).c_str())
    {
    }
};

}

#define SAFE_READ_EX(byteOrder, device, varname)                                                                                                               \
    if (!psdread<byteOrder>(device, varname)) {                                                                                                                \
        PkString msg = PkString("Failed to read \'%1\' tag!").arg(#varname);                                                                                     \
        throw KisAslReaderUtils::ASLParseException(msg);                                                                                                       \
    }

#define SAFE_READ_SIGNATURE_EX(byteOrder, device, varname, expected)                                                                                           \
    if (!psdread<byteOrder>(device, varname) || varname != expected) {                                                                                         \
        PkString msg = PkString(                                                                                                                                 \
                          "Failed to check signature \'%1\' tag!\n"                                                                                            \
                          "Value: \'%2\' Expected: \'%3\'")                                                                                                    \
                          .arg(#varname)                                                                                                                       \
                          .arg(static_cast<int>(varname))                                                                                                     \
                          .arg(static_cast<int>(expected));                                                                                                   \
        throw KisAslReaderUtils::ASLParseException(msg);                                                                                                       \
    }

template<psd_byte_order byteOrder, typename T, size_t S>
inline bool TRY_READ_SIGNATURE_2OPS_EX(PkStream &device, const std::array<T, S> &expected1, const std::array<T, S> &expected2)
{
    PkByteArray bytes;
    bytes.resize(static_cast<int>(S));

    const PkStream::pk_int64 nRead = device.peek(bytes.data(), static_cast<PkStream::pk_int64>(S));
    bytes.resize(static_cast<int>(nRead));

    if (byteOrder == psd_byte_order::psdLittleEndian) {
        std::reverse(bytes.data(), bytes.data() + bytes.size());
    }

    if (bytes.size() != static_cast<int>(S)) {
        return false;
    }

    // If read successfully, adjust current position of the io device

    bool match1 = true;
    bool match2 = true;
    for (size_t i = 0; i < S; ++i) {
        if (static_cast<unsigned char>(bytes.data()[i]) != static_cast<unsigned char>(expected1[i])) {
            match1 = false;
        }
        if (static_cast<unsigned char>(bytes.data()[i]) != static_cast<unsigned char>(expected2[i])) {
            match2 = false;
        }
    }

    if (match1 || match2) {
        // read, not seek, to support sequential devices
        auto bytesRead = psdreadBytes(device, S);
        if (bytesRead.size() == static_cast<int>(S)) {
            return true;
        }
    }

    dbgFile << "Photoshop signature verification failed! Got: " << pkToHex(bytes) << "(" << PkString::PkFromUtf8(bytes.constData(), bytes.size()) << ")";
    return false;
}

template<typename T, size_t S>
inline bool TRY_READ_SIGNATURE_2OPS_EX(psd_byte_order byteOrder, PkStream &device, const std::array<T, S> &expected1, const std::array<T, S> &expected2)
{
    switch (byteOrder) {
    case psd_byte_order::psdLittleEndian:
        return TRY_READ_SIGNATURE_2OPS_EX<psd_byte_order::psdLittleEndian>(device, expected1, expected2);
    default:
        return TRY_READ_SIGNATURE_2OPS_EX<psd_byte_order::psdBigEndian>(device, expected1, expected2);
    }
}

namespace KisAslReaderUtils
{
/**
 * String fetch functions
 *
 * ASL has 4 types of strings:
 *
 * - fixed length (4 bytes)
 * - variable length (length (4 bytes) + string (var))
 * - pascal (length (1 byte) + string (var))
 * - unicode string (length (4 bytes) + null-terminated unicode string (var)
 */

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline PkString readStringCommon(PkStream &device, int length)
{
    PkByteArray data = psdreadBytes<byteOrder>(device, length);

    if (data.size() != length) {
        PkString msg = PkString(
                          "Failed to read a string! "
                          "Bytes read: %1 Expected: %2")
                          .arg(data.size())
                          .arg(length);
        throw ASLParseException(msg);
    }

    return PkString::PkFromUtf8(data.constData(), data.size());
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline PkString readFixedString(PkStream &device)
{
    return readStringCommon<byteOrder>(device, 4);
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline PkString readVarString(PkStream &device)
{
    quint32 length = 0;
    SAFE_READ_EX(byteOrder, device, length);

    if (!length) {
        length = 4;
    }

    return readStringCommon<byteOrder>(device, length);
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline PkString readPascalString(PkStream &device)
{
    quint8 length = 0;
    SAFE_READ_EX(byteOrder, device, length);

    return readStringCommon(device, length);
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline PkString readUnicodeString(PkStream &device)
{
    PkString string;

    if (!psdread_unicodestring<byteOrder>(device, string)) {
        PkString msg = PkString("Failed to read a unicode string!");
        throw ASLParseException(msg);
    }

    return string;
}
}

#endif /* __KIS_ASL_READER_UTILS_H */
