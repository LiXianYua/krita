/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _KIS_EXIV2_COMMON_H_
#define _KIS_EXIV2_COMMON_H_

#include <exiv2/exiv2.hpp>

#include <cstdio>

#include "pk/container/PkMap.h"
#include "pk/container/PkStringList.h"
#include "pk/pointer/PkScopedPointer.h"
#include "pk/port/PkStream.h"
#include "pk/string/PkString.h"
#include "pk/time/PkDateTime.h"
#include "pk/variant/PkAuxTypes.h"
#include "pk/variant/PkVariant.h"

#include <kis_debug.h>
#include <kis_meta_data_value.h>

// base64 encode/decode 的局部替代（header-only，S-04 Task 3）。
// 照 libs/global/KoProperties.cpp 的 pkBase64Encode/pkBase64Decode 同款实现。
namespace KisExiv2IODeviceDetail {

inline const char kBase64Alphabet[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

inline std::string pkBase64Encode(const PkByteArray &data)
{
    std::string out;
    const char *src = data.constData();
    const int len = data.size();
    for (int i = 0; i < len; i += 3) {
        const unsigned int n =
            (static_cast<unsigned char>(src[i]) << 16) |
            (i + 1 < len ? static_cast<unsigned char>(src[i + 1]) << 8 : 0u) |
            (i + 2 < len ? static_cast<unsigned char>(src[i + 2]) : 0u);
        out.push_back(kBase64Alphabet[(n >> 18) & 0x3f]);
        out.push_back(kBase64Alphabet[(n >> 12) & 0x3f]);
        out.push_back(i + 1 < len ? kBase64Alphabet[(n >> 6) & 0x3f] : '=');
        out.push_back(i + 2 < len ? kBase64Alphabet[n & 0x3f] : '=');
    }
    return out;
}

inline int pkBase64Value(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+') return 62;
    if (c == '/') return 63;
    return -1;
}

inline PkByteArray pkBase64Decode(const std::string &s)
{
    std::vector<unsigned char> out;
    unsigned int buf = 0;
    int bits = 0;
    // 解码：遇 '='（padding）终止，对齐旧实现的 padding 语义。
    // 非法字符处理与旧实现不同：旧实现跳过非法字符继续，本 helper 停止。
    // 对规范 base64（toBase64 产物）两者逐字节一致；非规范输入属损坏数据，
    // Pk 侧按 R-31 显式失败，不静默继续。
    for (char c : s) {
        if (c == '=') break;
        const int v = pkBase64Value(c);
        if (v < 0) break;
        buf = (buf << 6) | static_cast<unsigned int>(v);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<unsigned char>((buf >> bits) & 0xff));
        }
    }
    return PkByteArray(out);
}

inline PkByteArray readAllFromStream(PkStream *stream)
{
    if (!stream) {
        return {};
    }
    std::vector<char> bytes;
    char chunk[8192];
    for (PkStream::pk_int64 count = 0;
         (count = stream->read(chunk, sizeof(chunk))) > 0;) {
        bytes.insert(bytes.end(), chunk, chunk + count);
    }
    if (bytes.empty()) {
        return {};
    }
    return PkByteArray(bytes.data(), static_cast<int>(bytes.size()));
}

} // namespace KisExiv2IODeviceDetail

// ---- Generic conversion functions ---- //

// Convert an exiv value to a KisMetaData value
inline KisMetaData::Value
#if EXIV2_TEST_VERSION(0,28,0)
exivValueToKMDValue(const Exiv2::Value::UniquePtr &value, bool forceSeq, KisMetaData::Value::ValueType arrayType = KisMetaData::Value::UnorderedArray)
#else
exivValueToKMDValue(const Exiv2::Value::AutoPtr &value, bool forceSeq, KisMetaData::Value::ValueType arrayType = KisMetaData::Value::UnorderedArray)
#endif
{
    switch (value->typeId()) {
    case Exiv2::signedByte:
    case Exiv2::invalidTypeId:
    case Exiv2::lastTypeId:
    case Exiv2::directory:
        dbgMetaData << "Invalid value :" << static_cast<int>(value->typeId()) << " value =" << value->toString().c_str();
        return {};
    case Exiv2::undefined: {
        dbgMetaData << "Undefined value :" << static_cast<int>(value->typeId()) << " value =" << value->toString().c_str();
        PkByteArray array;
        array.resize(static_cast<int>(value->count()));
        value->copy(reinterpret_cast<Exiv2::byte *>(array.data()), Exiv2::invalidByteOrder);
        return {PkString(KisExiv2IODeviceDetail::pkBase64Encode(array).c_str())};
    }
    case Exiv2::unsignedByte:
    case Exiv2::unsignedShort:
    case Exiv2::unsignedLong:
    case Exiv2::signedShort:
    case Exiv2::signedLong: {
        if (value->count() == 1 && !forceSeq) {
#if EXIV2_TEST_VERSION(0,28,0)
            return {static_cast<int>(value->toUint32())};
#else
            return {static_cast<int>(value->toLong())};
#endif
        } else {
            PkList<KisMetaData::Value> array;
            for (decltype(value->count()) i = 0; i < value->count(); ++i)
#if EXIV2_TEST_VERSION(0,28,0)
                array.push_back({static_cast<int>(value->toUint32(i))});
#else
                array.push_back({static_cast<int>(value->toLong(i))});
#endif
            return {array, arrayType};
        }
    }
    case Exiv2::asciiString:
    case Exiv2::string:
    case Exiv2::comment: // look at kexiv2 for the problem about decoding correctly that tag
        return {PkString(value->toString().c_str())};
    case Exiv2::unsignedRational:
        if (value->count() == 1 && !forceSeq) {
            if (value->size() < 2) {
                dbgMetaData << "Invalid size :" << value->size() << " value =" << value->toString().c_str();
                return {};
            }
            return {KisMetaData::Rational(value->toRational().first, value->toRational().second)};
        } else {
            PkList<KisMetaData::Value> array;
#if EXIV2_TEST_VERSION(0,28,0)
            for (size_t i = 0; i < value->count(); i++) {
#else
            for (long i = 0; i < value->count(); i++) {
#endif
                array.push_back(KisMetaData::Rational(value->toRational(i).first, value->toRational(i).second));
            }
            return {array, arrayType};
        }
    case Exiv2::signedRational:
        if (value->count() == 1 && !forceSeq) {
            if (value->size() < 2) {
                dbgMetaData << "Invalid size :" << value->size() << " value =" << value->toString().c_str();
                return {};
            }
            return {KisMetaData::Rational(value->toRational().first, value->toRational().second)};
        } else {
            PkList<KisMetaData::Value> array;
#if EXIV2_TEST_VERSION(0,28,0)
            for (size_t i = 0; i < value->count(); i++) {
#else
            for (long i = 0; i < value->count(); i++) {
#endif
                array.push_back(KisMetaData::Rational(value->toRational(i).first, value->toRational(i).second));
            }
            return {array, arrayType};
        }
    case Exiv2::date:
    case Exiv2::time:
        return {PkDateTime::fromString(value->toString(), PkDateTime::DateFormat::ISODate)};
    case Exiv2::xmpText:
    case Exiv2::xmpAlt:
    case Exiv2::xmpBag:
    case Exiv2::xmpSeq:
    case Exiv2::langAlt:
    default: {
        dbgMetaData << "Unknown type id :" << static_cast<int>(value->typeId()) << " value =" << value->toString().c_str();
        // This point must never be reached.
        return {};
    }
    }
    dbgMetaData << "Unknown type id :" << static_cast<int>(value->typeId()) << " value =" << value->toString().c_str();
    // This point must never be reached.
    return {};
}

// C locale 下按 "yyyy:MM:dd hh:mm:ss" 输出 EXIF 规范格式。
static std::string formatExifDateTime(const PkDateTime &dt)
{
    if (!dt.isValid()) {
        return {};
    }

    char buf[32];
    std::snprintf(buf, sizeof(buf), "%04d:%02d:%02d %02d:%02d:%02d",
                  dt.date().year(), dt.date().month(), dt.date().day(),
                  dt.time().hour(), dt.time().minute(), dt.time().second());
    return buf;
}

// Convert a PkVariant to an Exiv value
inline Exiv2::Value *variantToExivValue(const PkVariant &variant, Exiv2::TypeId type)
{
    switch (type) {
    case Exiv2::undefined: {
        const PkByteArray arr = KisExiv2IODeviceDetail::pkBase64Decode(variant.toString().PkToUtf8());
        return new Exiv2::DataValue(reinterpret_cast<const Exiv2::byte *>(arr.data()), arr.size());
    }
    case Exiv2::unsignedByte:
        return new Exiv2::ValueType<uint16_t>((uint16_t)variant.toUInt());
    case Exiv2::unsignedShort:
        return new Exiv2::ValueType<uint16_t>((uint16_t)variant.toUInt());
    case Exiv2::unsignedLong:
        return new Exiv2::ValueType<uint32_t>((uint32_t)variant.toUInt());
    case Exiv2::signedShort:
        return new Exiv2::ValueType<int16_t>((int16_t)variant.toInt());
    case Exiv2::signedLong:
        return new Exiv2::ValueType<int32_t>((int32_t)variant.toInt());
    case Exiv2::date: {
        PkDate date = variant.toDate();
        return new Exiv2::DateValue(date.year(), date.month(), date.day());
    }
    case Exiv2::asciiString:
        if (variant.type() == PkVariant::DateTime) {
            return new Exiv2::AsciiValue(formatExifDateTime(variant.toDateTime()));
        } else
            return new Exiv2::AsciiValue(variant.toString().PkToUtf8());
    case Exiv2::string: {
        if (variant.type() == PkVariant::DateTime) {
            return new Exiv2::StringValue(formatExifDateTime(variant.toDateTime()));
        } else
            return new Exiv2::StringValue(variant.toString().PkToUtf8());
    }
    case Exiv2::comment:
        return new Exiv2::CommentValue(variant.toString().PkToUtf8());
    default:
        dbgMetaData << "Unhandled type:" << static_cast<int>(type);
        // Unhandled values are rejected explicitly.
        return nullptr;
    }
}

template<typename T>
Exiv2::Value *arrayToExivValue(const KisMetaData::Value &value)
{
    Exiv2::ValueType<T> *exivValue = new Exiv2::ValueType<T>();
    const PkList<KisMetaData::Value> array = value.asArray();
    for (const KisMetaData::Value &item : array) {
        exivValue->value_.push_back(static_cast<T>(item.asVariant().toInt()));
    }
    return exivValue;
}

/// Convert a KisMetaData to an Exiv value
inline Exiv2::Value *kmdValueToExivValue(const KisMetaData::Value &value, Exiv2::TypeId type)
{
    switch (value.type()) {
    case KisMetaData::Value::Invalid:
        return Exiv2::Value::create(Exiv2::invalidTypeId).release();
    case KisMetaData::Value::Variant: {
        return variantToExivValue(value.asVariant(), type);
    }
    case KisMetaData::Value::Rational:
        // Rational values require a signed or unsigned rational Exiv2 type.
        if (type == Exiv2::signedRational) {
            return new Exiv2::RationalValue({value.asRational().numerator, value.asRational().denominator});
        } else {
            return new Exiv2::URationalValue({value.asRational().numerator, value.asRational().denominator});
        }
    case KisMetaData::Value::OrderedArray:
        [[fallthrough]];
    case KisMetaData::Value::UnorderedArray:
        [[fallthrough]];
    case KisMetaData::Value::LangArray:
        [[fallthrough]];
    case KisMetaData::Value::AlternativeArray: {
        switch (type) {
        case Exiv2::unsignedByte:
            [[fallthrough]];
        case Exiv2::unsignedShort:
            return arrayToExivValue<uint16_t>(value);
        case Exiv2::unsignedLong:
            return arrayToExivValue<uint32_t>(value);
        case Exiv2::signedShort:
            return arrayToExivValue<int16_t>(value);
        case Exiv2::signedLong:
            return arrayToExivValue<int32_t>(value);
        case Exiv2::asciiString:
            [[fallthrough]];
        case Exiv2::string: {
            // Using toLatin1 here is not lossy for asciiString,
            // but definitely is for string. IPTC allows UTF-8
            // encoding which supersets ASCII. See:
            // https://www.iptc.org/std/photometadata/specification/IPTC-PhotoMetadata#iim-properties
            // https://doc.qt.io/qt-5/qstring.html#toLatin1
            const PkList<KisMetaData::Value> list = value.asArray();
            PkStringList result;
            for (const KisMetaData::Value &v : list) {
                result.push_back(v.asVariant().toString());
            }
            return new Exiv2::StringValue(result.join(',').PkToUtf8());
        }
        case Exiv2::signedRational: {
            Exiv2::RationalValue *exivValue = new Exiv2::RationalValue();
            const PkList<KisMetaData::Value> array = value.asArray();
            exivValue->value_.reserve(static_cast<size_t>(array.size()));
            for (const KisMetaData::Value &item : array) {
                const KisMetaData::Rational value = item.asRational();
                exivValue->value_.emplace_back(value.numerator, value.denominator);
            }
            return exivValue;
        }
        case Exiv2::unsignedRational: {
            Exiv2::URationalValue *exivValue = new Exiv2::URationalValue();
            const PkList<KisMetaData::Value> array = value.asArray();
            exivValue->value_.reserve(static_cast<size_t>(array.size()));
            for (const KisMetaData::Value &item : array) {
                const KisMetaData::Rational value = item.asRational();
                exivValue->value_.emplace_back(static_cast<uint32_t>(value.numerator), static_cast<uint32_t>(value.denominator));
            }
            return exivValue;
        }
        default:
            warnMetaData << static_cast<int>(type) << " " << static_cast<int>(value.type()) << value;
            KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(0 && "Unknown alternative array type", nullptr);
            break;
        }
        break;
    } break;
    default:
        warnMetaData << static_cast<int>(type) << " " << static_cast<int>(value.type()) << value;
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(false && "Unknown array type", nullptr);
        break;
    }
    return nullptr;
}

/// Convert a KisMetaData to an Exiv value, without knowing the targeted Exiv2::TypeId
/// This function should be used for saving to XMP.
inline Exiv2::Value *kmdValueToExivXmpValue(const KisMetaData::Value &value)
{
    // Structures are handled by the XMP backend before this conversion.
    switch (value.type()) {
    case KisMetaData::Value::Invalid:
        return new Exiv2::DataValue(Exiv2::invalidTypeId);
    case KisMetaData::Value::Variant: {
        PkVariant var = value.asVariant();
        if (var.type() == PkVariant::Bool) {
            if (var.toBool()) {
                return new Exiv2::XmpTextValue("True");
            } else {
                return new Exiv2::XmpTextValue("False");
            }
        } else {
            // This conversion expects a string-compatible variant.
            return new Exiv2::XmpTextValue(var.toString().PkToUtf8());
        }
    }
    case KisMetaData::Value::Rational: {
        PkString rat = "%1 / %2";
        rat = rat.arg(value.asRational().numerator);
        rat = rat.arg(value.asRational().denominator);
        return new Exiv2::XmpTextValue(rat.PkToUtf8());
    }
    case KisMetaData::Value::AlternativeArray:
    case KisMetaData::Value::OrderedArray:
    case KisMetaData::Value::UnorderedArray: {
        Exiv2::XmpArrayValue *arrV = new Exiv2::XmpArrayValue();
        switch (value.type()) {
        case KisMetaData::Value::OrderedArray:
            arrV->setXmpArrayType(Exiv2::XmpValue::xaSeq);
            break;
        case KisMetaData::Value::UnorderedArray:
            arrV->setXmpArrayType(Exiv2::XmpValue::xaBag);
            break;
        case KisMetaData::Value::AlternativeArray:
            arrV->setXmpArrayType(Exiv2::XmpValue::xaAlt);
            break;
        default:
            // Cannot happen
            ;
        }
        for (const KisMetaData::Value &item : value.asArray()) {
            PkScopedPointer<Exiv2::Value> exivValue(kmdValueToExivXmpValue(item));
            if (exivValue) {
                arrV->read(exivValue->toString());
            }
        }
        return arrV;
    }
    case KisMetaData::Value::LangArray: {
        Exiv2::Value *arrV = new Exiv2::LangAltValue;
        PkMap<PkString, KisMetaData::Value> langArray = value.asLangArray();
        for (auto it = langArray.begin(); it != langArray.end(); ++it) {
            PkString exivVal;
            if (it.key() != "x-default") {
                exivVal = PkString("lang=") + it.key() + PkString(" ");
            }
            // Language-array entries are scalar variants here.
            PkVariant var = it.value().asVariant();
            // Language-array values are strings here.
            exivVal += var.toString();
            arrV->read(exivVal.PkToUtf8());
        }
        return arrV;
    }
    case KisMetaData::Value::Structure:
    default: {
        warnKrita << "KisExiv2: Unhandled value type";
        return nullptr;
    }
    }
    warnKrita << "KisExiv2: Unhandled value type";
    return nullptr;
}
#endif
