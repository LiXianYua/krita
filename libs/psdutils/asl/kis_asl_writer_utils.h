/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ASL_WRITER_UTILS_H
#define __KIS_ASL_WRITER_UTILS_H

#include "kritapsdutils_export.h"

#include <random>
#include <stdexcept>
#include <string>

#include <PkStream.h>

#include <kis_debug.h>
#include <resources/KoPattern.h>

#include "psd.h"
#include "psd_utils.h"

namespace KisAslWriterUtils
{
/**
 * Exception that is emitted when any write error appear.
 */
struct KRITAPSDUTILS_EXPORT ASLWriteException : public std::runtime_error {
    ASLWriteException(const PkString &msg)
        : std::runtime_error(psdToLatin1(msg).c_str())
    {
    }
};

}

#define SAFE_WRITE_EX(byteOrder, device, varname)                                                                                                              \
    if (!psdwrite<byteOrder>(device, varname)) {                                                                                                               \
        PkString msg = PkString("Failed to write \'%1\' tag!").arg(#varname);                                                                                    \
        throw KisAslWriterUtils::ASLWriteException(msg);                                                                                                       \
    }

namespace KisAslWriterUtils
{
// XXX: rect uses variable-sized type, is this correct?
template<psd_byte_order byteOrder>
inline void writeRect(const PkRect &rect, PkStream &device)
{
    {
        const qint32 rectY0 = static_cast<qint32>(rect.y());
        SAFE_WRITE_EX(byteOrder, device, rectY0);
    }
    {
        const qint32 rectX0 = static_cast<qint32>(rect.x());
        SAFE_WRITE_EX(byteOrder, device, rectX0);
    }
    {
        const qint32 rectY1 = static_cast<qint32>(rect.y() + rect.height());
        SAFE_WRITE_EX(byteOrder, device, rectY1);
    }
    {
        const qint32 rectX1 = static_cast<qint32>(rect.x() + rect.width());
        SAFE_WRITE_EX(byteOrder, device, rectX1);
    }
}

template<psd_byte_order byteOrder>
inline void writeUnicodeString(const PkString &value, PkStream &device)
{
    const quint32 len = static_cast<quint32>(value.size() + 1);
    SAFE_WRITE_EX(byteOrder, device, len);

    // PkString 无 utf16()，用 PkToU16() 取 UTF-16 码元数组；
    // std::u16string 保证 [size()] == u'\0'，循环含结尾的 NUL（与 Qt utf16() 一致）。
    const std::u16string u16 = value.PkToU16();
    for (quint32 i = 0; i < len; i++) {
        const quint16 c = static_cast<quint16>(u16[i]);
        SAFE_WRITE_EX(byteOrder, device, c);
    }
}

template<psd_byte_order byteOrder>
inline void writeVarString(const PkString &value, PkStream &device)
{
    const quint32 lenTag = static_cast<quint32>(value.size() != 4 ? value.size() : 0);
    SAFE_WRITE_EX(byteOrder, device, lenTag);

    const std::string latin1 = psdToLatin1(value);
    if (!device.write(latin1.data(), value.size())) {
        warnKrita << "WARNING: ASL: Failed to write ASL string" << ppVar(value);
        return;
    }
}

template<psd_byte_order byteOrder>
inline void writePascalString(const PkString &value, PkStream &device)
{
    KIS_ASSERT_RECOVER_RETURN(value.size() < 256);
    KIS_ASSERT_RECOVER_RETURN(value.size() >= 0);
    const quint8 lenTag = static_cast<quint8>(value.size());
    SAFE_WRITE_EX(byteOrder, device, lenTag);

    const std::string latin1 = psdToLatin1(value);
    if (!device.write(latin1.data(), value.size())) {
        warnKrita << "WARNING: ASL: Failed to write ASL string" << ppVar(value);
        return;
    }
}

template<psd_byte_order byteOrder>
inline void writeFixedString(const PkString &value, PkStream &device)
{
    KIS_ASSERT_RECOVER_RETURN(value.size() == 4);

    const std::string latin1 = psdToLatin1(value);
    PkByteArray data(latin1.data(), static_cast<int>(latin1.size()));

    if (byteOrder == psd_byte_order::psdLittleEndian) {
        std::reverse(data.data(), data.data() + data.size());
    }

    if (!device.write(data.data(), value.size())) {
        warnKrita << "WARNING: ASL: Failed to write ASL string" << ppVar(value);
        return;
    }
}

// UUID 以「无花括号的 36 字符规范形式」的 PkString 表示，空串 = null。
// aslParseUuid：对齐原 UUID 构造的严格解析（8-4-4-4-12，组间连字符，可带花括号），
// 非法输入返回空串（原 UUID 类型解析失败时保持 null）。
inline PkString aslParseUuid(const PkString &s)
{
    PkString t = s.trimmed();
    if (t.size() == 38 && t.startsWith("{") && t.at(t.size() - 1) == u'}') {
        t = t.mid(1, 36);
    }
    if (t.size() != 36) {
        return PkString();
    }

    const int dashPositions[] = {8, 13, 18, 23};
    int dashIdx = 0;
    for (int i = 0; i < 36; ++i) {
        const char16_t c = t.at(i);
        if (dashIdx < 4 && i == dashPositions[dashIdx]) {
            if (c != u'-') return PkString();
            ++dashIdx;
        } else {
            const bool isHex = (c >= u'0' && c <= u'9') ||
                               (c >= u'a' && c <= u'f') ||
                               (c >= u'A' && c <= u'F');
            if (!isHex) return PkString();
        }
    }
    return t;
}

// 生成 RFC 4122 v4（随机）UUID，无花括号 36 字符。语义对齐原 createUuid()
// 的格式；随机性来源用 std::random_device（值本身无测试依赖，只保格式）。
inline PkString aslCreateUuid()
{
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> dist(0, 255);

    unsigned char bytes[16];
    for (int i = 0; i < 16; ++i) {
        bytes[i] = static_cast<unsigned char>(dist(gen));
    }
    bytes[6] = static_cast<unsigned char>((bytes[6] & 0x0F) | 0x40); // version 4
    bytes[8] = static_cast<unsigned char>((bytes[8] & 0x3F) | 0x80); // variant 10xx

    const char hex[] = "0123456789abcdef";
    std::string s;
    s.reserve(36);
    for (int i = 0; i < 16; ++i) {
        if (i == 4 || i == 6 || i == 8 || i == 10) {
            s.push_back('-');
        }
        s.push_back(hex[bytes[i] >> 4]);
        s.push_back(hex[bytes[i] & 0x0F]);
    }
    return PkString(s.c_str());
}

// Write UUID fetched from the file name or generate
template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
inline PkString getPatternUuidLazy(const KoPatternSP pattern)
{
    PkString uuid; // 空 = null
    PkString patternFileName = pattern->filename();

    // PkString 无 endsWith / 大小写不敏感比较：比较小写后缀。
    if (patternFileName.size() >= 4 && patternFileName.right(4).toLower() == PkString(".pat")) {
        PkString strUuid = patternFileName.left(patternFileName.size() - 4);

        uuid = aslParseUuid(strUuid);
    }

    if (uuid.isEmpty()) {
        warnKrita << "WARNING: Saved pattern doesn't have a UUID, generating...";
        warnKrita << ppVar(patternFileName) << ppVar(pattern->name());
        uuid = aslCreateUuid();
    }

    // 原 toString() 带花括号再 .mid(1, 36) 剥掉；这里直接以
    // 36 字符无花括号形式持有，返回结果一致。
    return uuid;
}

/**
 * Align the pointer \p pos by alignment. Grow the pointer
 * if needed.
 *
 * \return the lowest integer not smaller than \p pos that divides by
 *         alignment
 */
inline qint64 alignOffsetCeil(qint64 pos, qint64 alignment)
{
    qint64 mask = alignment - 1;
    return (pos + mask) & ~mask;
}

template<class OffsetType, psd_byte_order byteOrder>
class OffsetStreamPusher
{
public:
    OffsetStreamPusher(PkStream &device, qint64 alignOnExit = 0, qint64 externalSizeTagOffset = -1)
        : m_device(device)
        , m_alignOnExit(alignOnExit)
        , m_externalSizeTagOffset(externalSizeTagOffset)
    {
        m_chunkStartPos = m_device.pos();

        if (externalSizeTagOffset < 0) {
            const OffsetType fakeObjectSize = OffsetType(0xdeadbeef);
            SAFE_WRITE_EX(byteOrder, m_device, fakeObjectSize);
        }
    }

    ~OffsetStreamPusher()
    {
        try {
            if (m_alignOnExit) {
                qint64 currentPos = m_device.pos();
                const qint64 alignedPos = alignOffsetCeil(currentPos, m_alignOnExit);

                for (; currentPos < alignedPos; currentPos++) {
                    quint8 padding = 0;
                    SAFE_WRITE_EX(byteOrder, m_device, padding);
                }
            }

            const qint64 currentPos = m_device.pos();

            qint64 writtenDataSize = 0;
            qint64 sizeFiledOffset = 0;

            if (m_externalSizeTagOffset >= 0) {
                writtenDataSize = currentPos - m_chunkStartPos;
                sizeFiledOffset = m_externalSizeTagOffset;
            } else {
                writtenDataSize = currentPos - m_chunkStartPos - sizeof(OffsetType);
                sizeFiledOffset = m_chunkStartPos;
            }

            m_device.seek(sizeFiledOffset);
            const OffsetType realObjectSize = writtenDataSize;
            SAFE_WRITE_EX(byteOrder, m_device, realObjectSize);
            m_device.seek(currentPos);
        } catch (ASLWriteException &e) {
            warnKrita << PREPEND_METHOD(e.what());
        }
    }

private:
    qint64 m_chunkStartPos;
    PkStream &m_device;
    qint64 m_alignOnExit;
    qint64 m_externalSizeTagOffset;
};

}

#endif /* __KIS_ASL_WRITER_UTILS_H */
