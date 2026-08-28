/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "kis_exif_io.h"

#include <exiv2/error.hpp>
#include <exiv2/exif.hpp>

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>

#include <PkAuxTypes.h>
#include <PkDateTime.h>
#include <PkStream.h>
#include <PkVariant.h>

#include <kis_debug.h>
#include <kis_exiv2_common.h>
#include <kis_meta_data_entry.h>
#include <kis_meta_data_schema.h>
#include <kis_meta_data_schema_registry.h>
#include <kis_meta_data_store.h>
#include <kis_meta_data_tags.h>
#include <kis_meta_data_value.h>

namespace
{
PkByteArray zeroedBytes(int size)
{
    PkByteArray bytes;
    bytes.resize(size);
    return bytes;
}

void appendBytes(PkByteArray &destination, const char *data, int size)
{
    if (size <= 0) {
        return;
    }
    const int oldSize = destination.size();
    destination.resize(oldSize + size);
    std::memcpy(destination.data() + oldSize, data, static_cast<std::size_t>(size));
}

int indexOfByte(const PkByteArray &bytes, char value, int start)
{
    for (int i = start; i < bytes.size(); ++i) {
        if (bytes.constData()[i] == value) {
            return i;
        }
    }
    return -1;
}

int indexOfZeroPair(const PkByteArray &bytes, int start)
{
    for (int i = start; i + 1 < bytes.size(); ++i) {
        if (bytes.constData()[i] == '\0' && bytes.constData()[i + 1] == '\0') {
            return i;
        }
    }
    return -1;
}

PkString fromLatin1(const char *data, int size = -1)
{
    if (!data) {
        return {};
    }
    if (size < 0) {
        size = static_cast<int>(std::strlen(data));
    }
    std::string utf8;
    utf8.reserve(static_cast<std::size_t>(size) * 2);
    for (int i = 0; i < size; ++i) {
        const auto c = static_cast<unsigned char>(data[i]);
        if (c < 0x80) {
            utf8.push_back(static_cast<char>(c));
        } else {
            utf8.push_back(static_cast<char>(0xc0 | (c >> 6)));
            utf8.push_back(static_cast<char>(0x80 | (c & 0x3f)));
        }
    }
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

std::string toLatin1(const PkString &text)
{
    const std::u16string utf16 = text.PkToU16();
    std::string latin1;
    latin1.reserve(utf16.size());
    for (const char16_t codeUnit : utf16) {
        latin1.push_back(codeUnit <= 0xff ? static_cast<char>(codeUnit) : '?');
    }
    return latin1;
}

void appendUtf8CodePoint(std::string &utf8, std::uint32_t codePoint)
{
    if (codePoint <= 0x7f) {
        utf8.push_back(static_cast<char>(codePoint));
    } else if (codePoint <= 0x7ff) {
        utf8.push_back(static_cast<char>(0xc0 | (codePoint >> 6)));
        utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else if (codePoint <= 0xffff) {
        utf8.push_back(static_cast<char>(0xe0 | (codePoint >> 12)));
        utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    } else {
        utf8.push_back(static_cast<char>(0xf0 | (codePoint >> 18)));
        utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 12) & 0x3f)));
        utf8.push_back(static_cast<char>(0x80 | ((codePoint >> 6) & 0x3f)));
        utf8.push_back(static_cast<char>(0x80 | (codePoint & 0x3f)));
    }
}

std::uint16_t byteSwap16(std::uint16_t value)
{
    return static_cast<std::uint16_t>((value >> 8) | (value << 8));
}

PkString fromUtf16Bytes(const char *data, int byteCount)
{
    if (!data || byteCount < 2) {
        return {};
    }
    std::vector<std::uint16_t> units(static_cast<std::size_t>(byteCount / 2));
    std::memcpy(units.data(), data, units.size() * sizeof(std::uint16_t));
    bool swap = false;
    std::size_t index = 0;
    if (units[0] == 0xfeff) {
        index = 1;
    } else if (units[0] == 0xfffe) {
        swap = true;
        index = 1;
    }
    std::string utf8;
    while (index < units.size()) {
        std::uint16_t first = swap ? byteSwap16(units[index]) : units[index];
        ++index;
        std::uint32_t codePoint = first;
        if (first >= 0xd800 && first <= 0xdbff && index < units.size()) {
            const std::uint16_t second = swap ? byteSwap16(units[index]) : units[index];
            if (second >= 0xdc00 && second <= 0xdfff) {
                ++index;
                codePoint = 0x10000u + ((first - 0xd800u) << 10) + (second - 0xdc00u);
            }
        }
        appendUtf8CodePoint(utf8, codePoint);
    }
    return PkString::PkFromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

void appendUtf16WithBom(PkByteArray &destination, const PkString &text)
{
    const std::uint16_t bom = 0xfeff;
    appendBytes(destination, reinterpret_cast<const char *>(&bom), sizeof(bom));
    const std::u16string units = text.PkToU16();
    appendBytes(destination,
                reinterpret_cast<const char *>(units.data()),
                static_cast<int>(units.size() * sizeof(char16_t)));
}

template<typename T>
T byteSwap(T value)
{
    using Unsigned = std::make_unsigned_t<T>;
    Unsigned input = static_cast<Unsigned>(value);
    Unsigned output = 0;
    for (std::size_t i = 0; i < sizeof(T); ++i) {
        output = static_cast<Unsigned>((output << 8) | (input & 0xffu));
        input >>= 8;
    }
    return static_cast<T>(output);
}
} // namespace

// ---- Exception conversion functions ---- //

// convert ExifVersion and FlashpixVersion to a KisMetaData value
#if EXIV2_TEST_VERSION(0,28,0)
KisMetaData::Value exifVersionToKMDValue(const Exiv2::Value::UniquePtr value)
#else
KisMetaData::Value exifVersionToKMDValue(const Exiv2::Value::AutoPtr value)
#endif
{
    const Exiv2::DataValue *dvalue = dynamic_cast<const Exiv2::DataValue *>(&*value);
    if (dvalue) {
        assert(dvalue);
        PkByteArray array = zeroedBytes(static_cast<int>(dvalue->count()));
        dvalue->copy((Exiv2::byte *)array.data());
        return KisMetaData::Value(PkString::PkFromUtf8(array.constData(), array.size()));
    } else {
        assert(value->typeId() == Exiv2::asciiString);
        return KisMetaData::Value(fromLatin1(value->toString().c_str()));
    }
}

// convert from KisMetaData value to ExifVersion and FlashpixVersion
Exiv2::Value *kmdValueToExifVersion(const KisMetaData::Value &value)
{
    Exiv2::DataValue *dvalue = new Exiv2::DataValue;
    PkString ver = value.asVariant().toString();
    const std::string encoded = ver.PkToUtf8();
    dvalue->read((const Exiv2::byte *)encoded.data(), encoded.size());
    return dvalue;
}

// Convert an exif array of integer string to a KisMetaData array of integer
#if EXIV2_TEST_VERSION(0,28,0)
KisMetaData::Value exifArrayToKMDIntOrderedArray(const Exiv2::Value::UniquePtr value)
#else
KisMetaData::Value exifArrayToKMDIntOrderedArray(const Exiv2::Value::AutoPtr value)
#endif
{
    PkList<KisMetaData::Value> v;
    const Exiv2::DataValue *dvalue = dynamic_cast<const Exiv2::DataValue *>(&*value);
    if (dvalue) {
#if EXIV2_TEST_VERSION(0,28,0)
        for (size_t i = 0; i < dvalue->count(); i++) {
            v.push_back({(int)dvalue->toUint32(i)});
#else
        for (long i = 0; i < dvalue->count(); i++) {
            v.push_back({(int)dvalue->toLong(i)});
#endif
        }
    } else {
        assert(value->typeId() == Exiv2::asciiString);
        PkString str = fromLatin1(value->toString().c_str());
        v.push_back(KisMetaData::Value(str.toInt()));
    }
    return KisMetaData::Value(v, KisMetaData::Value::OrderedArray);
}

// Convert a KisMetaData array of integer to an exif array of integer string
Exiv2::Value *kmdIntOrderedArrayToExifArray(const KisMetaData::Value &value)
{
    std::vector<Exiv2::byte> v;
    for (const KisMetaData::Value &it : value.asArray()) {
        v.push_back(static_cast<uint8_t>(it.asVariant().toInt()));
    }
    return new Exiv2::DataValue(v.data(), static_cast<long>(v.size()));
}

#if EXIV2_TEST_VERSION(0,28,0)
PkDateTime exivValueToDateTime(const Exiv2::Value::UniquePtr value)
#else
PkDateTime exivValueToDateTime(const Exiv2::Value::AutoPtr value)
#endif
{
    return PkDateTime::fromString(value->toString(), PkDateTime::DateFormat::ISODate);
}

template<typename T>
inline T fixEndianness(T v, Exiv2::ByteOrder order)
{
    switch (order) {
    case Exiv2::invalidByteOrder:
        return v;
    case Exiv2::littleEndian:
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
        return v;
#else
        return byteSwap(v);
#endif
    case Exiv2::bigEndian:
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
        return v;
#else
        return byteSwap(v);
#endif
    }
    warnKrita << "KisExifIO: unknown byte order";
    return v;
}

Exiv2::ByteOrder invertByteOrder(Exiv2::ByteOrder order)
{
    switch (order) {
    case Exiv2::littleEndian:
        return Exiv2::bigEndian;
    case Exiv2::bigEndian:
        return Exiv2::littleEndian;
    case Exiv2::invalidByteOrder:
        warnKrita << "KisExifIO: Can't invert Exiv2::invalidByteOrder";
        return Exiv2::invalidByteOrder;
    }
    return Exiv2::invalidByteOrder;
}

#if EXIV2_TEST_VERSION(0,28,0)
KisMetaData::Value exifOECFToKMDOECFStructure(const Exiv2::Value::UniquePtr value, Exiv2::ByteOrder order)
#else
KisMetaData::Value exifOECFToKMDOECFStructure(const Exiv2::Value::AutoPtr value, Exiv2::ByteOrder order)
#endif
{
    PkMap<PkString, KisMetaData::Value> oecfStructure;
    const Exiv2::DataValue *dvalue = dynamic_cast<const Exiv2::DataValue *>(&*value);
    assert(dvalue);
    PkByteArray array = zeroedBytes(static_cast<int>(dvalue->count()));

    dvalue->copy((Exiv2::byte *)array.data());
    std::uint16_t columns =
        fixEndianness<std::uint16_t>((reinterpret_cast<std::uint16_t *>(array.data()))[0], order);
    std::uint16_t rows =
        fixEndianness<std::uint16_t>((reinterpret_cast<std::uint16_t *>(array.data()))[1], order);

    if ((static_cast<std::size_t>(columns) * rows * 8 + 4)
        > dvalue->count()) { // Sometime byteOrder get messed up (especially if metadata got saved with kexiv2 library,
                             // or any library that doesn't save back with the same byte order as the camera)
        order = invertByteOrder(order);
        columns = fixEndianness<std::uint16_t>((reinterpret_cast<std::uint16_t *>(array.data()))[0], order);
        rows = fixEndianness<std::uint16_t>((reinterpret_cast<std::uint16_t *>(array.data()))[1], order);
        if (static_cast<std::size_t>(columns) * rows * 8 + 4 > dvalue->count()) {
            return {};
        }
    }
    const PkVariant qcolumns(static_cast<unsigned int>(columns));
    const PkVariant qrows(static_cast<unsigned int>(rows));
    oecfStructure["Columns"] = KisMetaData::Value(qcolumns);
    oecfStructure["Rows"] = KisMetaData::Value(qrows);
    int index = 4;
    PkList<KisMetaData::Value> names;
#if EXIV2_TEST_VERSION(0,28,0)
    for (size_t i = 0; i < columns; i++) {
#else
    for (int i = 0; i < columns; i++) {
#endif
        int lastIndex = indexOfByte(array, '\0', index);
        PkString name = lastIndex >= index
            ? fromLatin1(array.constData() + index, lastIndex - index)
            : PkString();
        if (index != lastIndex) {
            index = lastIndex + 1;
            dbgMetaData << "Name [" << i << "] =" << name;
            names.append(KisMetaData::Value(name));
        } else {
            names.append(KisMetaData::Value(""));
        }
    }

    oecfStructure["Names"] = KisMetaData::Value(names, KisMetaData::Value::OrderedArray);
    PkList<KisMetaData::Value> values;
    std::int32_t *dataIt = reinterpret_cast<std::int32_t *>(array.data() + index);
#if EXIV2_TEST_VERSION(0,28,0)
    for (size_t i = 0; i < columns; i++) {
        for (size_t j = 0; j < rows; j++) {
#else
    for (int i = 0; i < columns; i++) {
        for (int j = 0; j < rows; j++) {
#endif
            values.append(KisMetaData::Value(
                KisMetaData::Rational(fixEndianness<std::int32_t>(dataIt[0], order), fixEndianness<std::int32_t>(dataIt[1], order))));
            dataIt += 2;
        }
    }
    oecfStructure["Values"] = KisMetaData::Value(values, KisMetaData::Value::OrderedArray);
    dbgMetaData << "OECF: " << ppVar(columns) << ppVar(rows) << ppVar(dvalue->count());
    return KisMetaData::Value(oecfStructure);
}

Exiv2::Value *kmdOECFStructureToExifOECF(const KisMetaData::Value &value)
{
    PkMap<PkString, KisMetaData::Value> oecfStructure = value.asStructure();
    const std::uint16_t columns = static_cast<std::uint16_t>(oecfStructure["Columns"].asVariant().toUInt());
    const std::uint16_t rows = static_cast<std::uint16_t>(oecfStructure["Rows"].asVariant().toUInt());

    PkList<KisMetaData::Value> names = oecfStructure["Names"].asArray();
    PkList<KisMetaData::Value> values = oecfStructure["Values"].asArray();
    assert(columns * rows == values.size());
    int length = 4 + rows * columns * 8; // The 4 byte for storing rows/columns and the rows*columns*sizeof(rational)
    bool saveNames = (!names.empty() && names[0].asVariant().toString().size() > 0);
    std::vector<std::string> encodedNames;
    if (saveNames) {
        encodedNames.reserve(columns);
        for (int i = 0; i < columns; i++) {
            encodedNames.push_back(toLatin1(names[i].asVariant().toString()));
            length += static_cast<int>(encodedNames.back().size()) + 1;
        }
    }
    PkByteArray array = zeroedBytes(length);
    (reinterpret_cast<std::uint16_t *>(array.data()))[0] = columns;
    (reinterpret_cast<std::uint16_t *>(array.data()))[1] = rows;
    int index = 4;
    if (saveNames) {
        for (int i = 0; i < columns; i++) {
            const std::string &name = encodedNames[static_cast<std::size_t>(i)];
            memcpy(array.data() + index, name.data(), name.size());
            index += static_cast<int>(name.size());
            array.data()[index++] = '\0';
        }
    }
    std::int32_t *dataIt = reinterpret_cast<std::int32_t *>(array.data() + index);
    for (const KisMetaData::Value &it : values) {
        dataIt[0] = it.asRational().numerator;
        dataIt[1] = it.asRational().denominator;
        dataIt += 2;
    }
    return new Exiv2::DataValue((const Exiv2::byte *)array.data(), array.size());
}

#if EXIV2_TEST_VERSION(0,28,0)
KisMetaData::Value deviceSettingDescriptionExifToKMD(const Exiv2::Value::UniquePtr value)
#else
KisMetaData::Value deviceSettingDescriptionExifToKMD(const Exiv2::Value::AutoPtr value)
#endif
{
    PkMap<PkString, KisMetaData::Value> deviceSettingStructure;
    PkByteArray array;

    const Exiv2::DataValue *dvalue = dynamic_cast<const Exiv2::DataValue *>(&*value);
    if (dvalue) {
        array.resize(dvalue->count());
        dvalue->copy((Exiv2::byte *)array.data());
    } else {
        assert(value->typeId() == Exiv2::unsignedShort);
        array.resize(2 * value->count());
        value->copy((Exiv2::byte *)array.data(), Exiv2::littleEndian);
    }
    int columns = (reinterpret_cast<std::uint16_t *>(array.data()))[0];
    int rows = (reinterpret_cast<std::uint16_t *>(array.data()))[1];
    deviceSettingStructure["Columns"] = KisMetaData::Value(columns);
    deviceSettingStructure["Rows"] = KisMetaData::Value(rows);
    PkList<KisMetaData::Value> settings;
    for (int index = 4; index < array.size();) {
        const int lastIndex = indexOfZeroPair(array, index);
        if (lastIndex < 0)
            break; // Data is not a String, ignore
        const int numChars = (lastIndex - index) / 2; // including trailing zero

        PkString setting = fromUtf16Bytes(array.constData() + index, numChars * 2);
        index = lastIndex + 2;
        dbgMetaData << "Setting << " << setting;
        settings.append(KisMetaData::Value(setting));
    }
    deviceSettingStructure["Settings"] = KisMetaData::Value(settings, KisMetaData::Value::OrderedArray);
    return KisMetaData::Value(deviceSettingStructure);
}

Exiv2::Value *deviceSettingDescriptionKMDToExif(const KisMetaData::Value &value)
{
    PkMap<PkString, KisMetaData::Value> deviceSettingStructure = value.asStructure();
    const std::uint16_t columns = static_cast<std::uint16_t>(deviceSettingStructure["Columns"].asVariant().toUInt());
    std::uint16_t rows = static_cast<std::uint16_t>(deviceSettingStructure["Rows"].asVariant().toUInt());

    PkList<KisMetaData::Value> settings = deviceSettingStructure["Settings"].asArray();
    PkByteArray array = zeroedBytes(4);
    (reinterpret_cast<std::uint16_t *>(array.data()))[0] = columns;
    (reinterpret_cast<std::uint16_t *>(array.data()))[1] = rows;
    for (const KisMetaData::Value &v : settings) {
        const PkString str = v.asVariant().toString();
        appendUtf16WithBom(array, str);
    }
    return new Exiv2::DataValue((const Exiv2::byte *)array.data(), array.size());
}

#if EXIV2_TEST_VERSION(0,28,0)
KisMetaData::Value cfaPatternExifToKMD(const Exiv2::Value::UniquePtr value, Exiv2::ByteOrder order)
#else
KisMetaData::Value cfaPatternExifToKMD(const Exiv2::Value::AutoPtr value, Exiv2::ByteOrder order)
#endif
{
    PkMap<PkString, KisMetaData::Value> cfaPatternStructure;
    const Exiv2::DataValue *dvalue = dynamic_cast<const Exiv2::DataValue *>(&*value);
    assert(dvalue);
    PkByteArray array = zeroedBytes(static_cast<int>(dvalue->count()));
    dvalue->copy((Exiv2::byte *)array.data());
#if EXIV2_TEST_VERSION(0,28,0)
    size_t columns = fixEndianness<std::ptrdiff_t>((reinterpret_cast<std::ptrdiff_t *>(array.data()))[0], order);
    size_t rows = fixEndianness<std::ptrdiff_t>((reinterpret_cast<std::ptrdiff_t *>(array.data()))[1], order);
#else
    int columns = fixEndianness<std::uint16_t>((reinterpret_cast<std::uint16_t *>(array.data()))[0], order);
    int rows = fixEndianness<std::uint16_t>((reinterpret_cast<std::uint16_t *>(array.data()))[1], order);
#endif
    if ((columns * rows + 4)
        != dvalue->count()) { // Sometime byteOrder get messed up (especially if metadata got saved with kexiv2 library,
                              // or any library that doesn't save back with the same byte order as the camera)
        order = invertByteOrder(order);
        columns = fixEndianness<std::uint16_t>((reinterpret_cast<std::uint16_t *>(array.data()))[0], order);
        rows = fixEndianness<std::uint16_t>((reinterpret_cast<std::uint16_t *>(array.data()))[1], order);
    }
    PkVariant qcolumns, qrows;
    qcolumns.setValue(columns);
    qrows.setValue(rows);
    cfaPatternStructure["Columns"] = KisMetaData::Value(qcolumns);
    cfaPatternStructure["Rows"] = KisMetaData::Value(qrows);
    PkList<KisMetaData::Value> values;
    std::size_t index = 4;
    for (std::size_t i = 0; i < columns * rows; ++i) {
        values.append(KisMetaData::Value(*(array.data() + index)));
        ++index;
    }
    cfaPatternStructure["Values"] = KisMetaData::Value(values, KisMetaData::Value::OrderedArray);
    dbgMetaData << "CFAPattern " << ppVar(columns) << " " << ppVar(rows) << ppVar(values.size())
                << ppVar(dvalue->count());
    return KisMetaData::Value(cfaPatternStructure);
}

Exiv2::Value *cfaPatternKMDToExif(const KisMetaData::Value &value)
{
    PkMap<PkString, KisMetaData::Value> cfaStructure = value.asStructure();
    const std::uint16_t columns = static_cast<std::uint16_t>(cfaStructure["Columns"].asVariant().toUInt());
    const std::uint16_t rows = static_cast<std::uint16_t>(cfaStructure["Rows"].asVariant().toUInt());

    PkList<KisMetaData::Value> values = cfaStructure["Values"].asArray();
    assert(columns * rows == values.size());
    PkByteArray array = zeroedBytes(4 + columns * rows);
    (reinterpret_cast<std::uint16_t *>(array.data()))[0] = columns;
    (reinterpret_cast<std::uint16_t *>(array.data()))[1] = rows;
    for (int i = 0; i < columns * rows; i++) {
        const std::uint8_t val = static_cast<std::uint8_t>(values[i].asVariant().toUInt());
        *(array.data() + 4 + i) = (char)val;
    }
    dbgMetaData << "Cfa Array " << ppVar(columns) << ppVar(rows) << ppVar(array.size());
    return new Exiv2::DataValue((const Exiv2::byte *)array.data(), array.size());
}

// Read and write Flash //

#if EXIV2_TEST_VERSION(0,28,0)
KisMetaData::Value flashExifToKMD(const Exiv2::Value::UniquePtr value)
#else
KisMetaData::Value flashExifToKMD(const Exiv2::Value::AutoPtr value)
#endif
{
#if EXIV2_TEST_VERSION(0,28,0)
    const uint16_t v = static_cast<uint16_t>(value->toUint32());
#else
    const uint16_t v = static_cast<uint16_t>(value->toLong());
#endif
    PkMap<PkString, KisMetaData::Value> flashStructure;
    bool fired = (v & 0x01); // bit 1 is whether flash was fired or not
    flashStructure["Fired"] = PkVariant(fired);
    int ret = ((v >> 1) & 0x03); // bit 2 and 3 are Return
    flashStructure["Return"] = PkVariant(ret);
    int mode = ((v >> 3) & 0x03); // bit 4 and 5 are Mode
    flashStructure["Mode"] = PkVariant(mode);
    bool function = ((v >> 5) & 0x01); // bit 6 if function
    flashStructure["Function"] = PkVariant(function);
    bool redEye = ((v >> 6) & 0x01); // bit 7 if function
    flashStructure["RedEyeMode"] = PkVariant(redEye);
    return KisMetaData::Value(flashStructure);
}

Exiv2::Value *flashKMDToExif(const KisMetaData::Value &value)
{
    uint16_t v = 0;
    PkMap<PkString, KisMetaData::Value> flashStructure = value.asStructure();
    v = flashStructure["Fired"].asVariant().toBool();
    v |= ((flashStructure["Return"].asVariant().toInt() & 0x03) << 1);
    v |= ((flashStructure["Mode"].asVariant().toInt() & 0x03) << 3);
    v |= ((flashStructure["Function"].asVariant().toInt() & 0x03) << 5);
    v |= ((flashStructure["RedEyeMode"].asVariant().toInt() & 0x03) << 6);
    return new Exiv2::ValueType<uint16_t>(v);
}

// ---- Implementation of KisExifIO ----//
KisExifIO::KisExifIO()
    : KisMetaData::IOBackend()
{
}

KisExifIO::~KisExifIO()
{
}

bool KisExifIO::saveTo(const KisMetaData::Store *store, PkStream *ioDevice, HeaderType headerType) const
{
    ioDevice->open(PkStream::WriteOnly);
    Exiv2::ExifData exifData;
    if (headerType == KisMetaData::IOBackend::JpegHeader) {
        static const char header[] = {'E', 'x', 'i', 'f', '\0', '\0'};
        ioDevice->write(header, sizeof(header));
    }

    for (const KisMetaData::Entry &entry : *store) {
        try {
            dbgMetaData << "Trying to save: " << entry.name() << " of " << entry.schema()->prefix() << ":"
                        << entry.schema()->uri();
            PkString exivKey;
            if (entry.schema()->uri() == KisMetaData::Schema::TIFFSchemaUri) {
                exivKey = PkString("Exif.Image.") + entry.name();
            } else if (entry.schema()->uri()
                       == KisMetaData::Schema::EXIFSchemaUri) { // Distinguish between exif and gps
                if (entry.name().left(3) == "GPS") {
                    exivKey = PkString("Exif.GPSInfo.") + entry.name();
                } else {
                    exivKey = PkString("Exif.Photo.") + entry.name();
                }
            } else if (entry.schema()->uri() == KisMetaData::Schema::DublinCoreSchemaUri) {
                if (entry.name() == "description") {
                    exivKey = "Exif.Image.ImageDescription";
                } else if (entry.name() == "creator") {
                    exivKey = "Exif.Image.Artist";
                } else if (entry.name() == "rights") {
                    exivKey = "Exif.Image.Copyright";
                }
            } else if (entry.schema()->uri() == KisMetaData::Schema::XMPSchemaUri) {
                if (entry.name() == "ModifyDate") {
                    exivKey = "Exif.Image.DateTime";
                } else if (entry.name() == "CreatorTool") {
                    exivKey = "Exif.Image.Software";
                }
            } else if (entry.schema()->uri() == KisMetaData::Schema::MakerNoteSchemaUri) {
                if (entry.name() == "RawData") {
                    exivKey = "Exif.Photo.MakerNote";
                }
            }
            dbgMetaData << "Saving " << entry.name() << " to " << exivKey;
            if (exivKey.isEmpty()) {
                dbgMetaData << entry.qualifiedName() << " is unsavable to EXIF";
            } else {
                Exiv2::ExifKey exifKey(exivKey.PkToUtf8());
                Exiv2::Value *v = 0;
                if (exivKey == "Exif.Photo.ExifVersion" || exivKey == "Exif.Photo.FlashpixVersion") {
                    v = kmdValueToExifVersion(entry.value());
                } else if (exivKey == "Exif.Photo.FileSource") {
                    char s[] = {0x03};
                    v = new Exiv2::DataValue((const Exiv2::byte *)s, 1);
                } else if (exivKey == "Exif.Photo.SceneType") {
                    char s[] = {0x01};
                    v = new Exiv2::DataValue((const Exiv2::byte *)s, 1);
                } else if (exivKey == "Exif.Photo.ComponentsConfiguration") {
                    v = kmdIntOrderedArrayToExifArray(entry.value());
                } else if (exivKey == "Exif.Image.Artist") { // load as dc:creator
                    KisMetaData::Value creator = entry.value();
                    if (entry.value().asArray().size() > 0) {
                        creator = entry.value().asArray()[0];
                    }
#if !EXIV2_TEST_VERSION(0, 21, 0)
                    v = kmdValueToExivValue(creator, Exiv2::ExifTags::tagType(exifKey.tag(), exifKey.ifdId()));
#else
                    v = kmdValueToExivValue(creator, exifKey.defaultTypeId());
#endif
                } else if (exivKey == "Exif.Photo.OECF") {
                    v = kmdOECFStructureToExifOECF(entry.value());
                } else if (exivKey == "Exif.Photo.DeviceSettingDescription") {
                    v = deviceSettingDescriptionKMDToExif(entry.value());
                } else if (exivKey == "Exif.Photo.CFAPattern") {
                    v = cfaPatternKMDToExif(entry.value());
                } else if (exivKey == "Exif.Photo.Flash") {
                    v = flashKMDToExif(entry.value());
                } else if (exivKey == "Exif.Photo.UserComment") {
                    assert(entry.value().type() == KisMetaData::Value::LangArray);
                    PkMap<PkString, KisMetaData::Value> langArr = entry.value().asLangArray();
                    if (langArr.contains("x-default")) {
#if !EXIV2_TEST_VERSION(0, 21, 0)
                        v = kmdValueToExivValue(langArr.value("x-default"),
                                                Exiv2::ExifTags::tagType(exifKey.tag(), exifKey.ifdId()));
#else
                        v = kmdValueToExivValue(langArr.value("x-default"), exifKey.defaultTypeId());
#endif
                    } else if (langArr.size() > 0) {
#if !EXIV2_TEST_VERSION(0, 21, 0)
                        v = kmdValueToExivValue(langArr.begin().value(),
                                                Exiv2::ExifTags::tagType(exifKey.tag(), exifKey.ifdId()));
#else
                        v = kmdValueToExivValue(langArr.begin().value(), exifKey.defaultTypeId());
#endif
                    }
                } else {
                    dbgMetaData << exifKey.tag();
#if !EXIV2_TEST_VERSION(0, 21, 0)
                    v = kmdValueToExivValue(entry.value(), Exiv2::ExifTags::tagType(exifKey.tag(), exifKey.ifdId()));
#else
                    v = kmdValueToExivValue(entry.value(), exifKey.defaultTypeId());
#endif
                }
                if (v && v->typeId() != Exiv2::invalidTypeId) {
                    dbgMetaData << "Saving key" << exivKey << " of KMD value" << entry.value();
                    exifData.add(exifKey, v);
                } else {
                    dbgMetaData << "No exif value was created for" << entry.qualifiedName() << " as"
                                << exivKey; // << " of KMD value" << entry.value();
                }
            }
#if EXIV2_TEST_VERSION(0,28,0)
        } catch (Exiv2::Error &e) {
#else
        } catch (Exiv2::AnyError &e) {
#endif
            dbgMetaData << "exiv error " << e.what();
        }
    }
#if !EXIV2_TEST_VERSION(0, 18, 0)
    Exiv2::DataBuf rawData = exifData.copy();
    ioDevice->write((const char *)rawData.pData_, rawData.size_);
#else
    Exiv2::Blob rawData;
    Exiv2::ExifParser::encode(rawData, Exiv2::littleEndian, exifData);
    ioDevice->write((const char *)&*rawData.begin(), static_cast<int>(rawData.size()));
#endif
    ioDevice->close();
    return true;
}

bool KisExifIO::canSaveAllEntries(KisMetaData::Store * /*store*/) const
{
    return false; // It's a known fact that exif can't save all information, but TODO: write the check
}

bool KisExifIO::loadFrom(KisMetaData::Store *store, PkStream *ioDevice) const
{
    if (!ioDevice->open(PkStream::ReadOnly)) {
        return false;
    }
    PkByteArray arr(KisExiv2IODeviceDetail::readAllFromStream(ioDevice));
    Exiv2::ExifData exifData;
    Exiv2::ByteOrder byteOrder;
#if !EXIV2_TEST_VERSION(0, 18, 0)
    exifData.load((const Exiv2::byte *)arr.data(), arr.size());
    byteOrder = exifData.byteOrder();
#else
    try {
        byteOrder =
            Exiv2::ExifParser::decode(exifData, (const Exiv2::byte *)arr.data(), static_cast<uint32_t>(arr.size()));
    } catch (const std::exception &ex) {
        warnKrita << "Received exception trying to parse exiv data" << ex.what();
        return false;
    } catch (...) {
        dbgKrita << "Received unknown exception trying to parse exiv data";
        return false;
    }
#endif
    dbgMetaData << "Byte order = " << byteOrder << ppVar(Exiv2::bigEndian) << ppVar(Exiv2::littleEndian);
    dbgMetaData << "There are" << exifData.count() << " entries in the exif section";
    const KisMetaData::Schema *tiffSchema =
        KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::TIFFSchemaUri);
    assert(tiffSchema);
    const KisMetaData::Schema *exifSchema =
        KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::EXIFSchemaUri);
    assert(exifSchema);
    const KisMetaData::Schema *dcSchema =
        KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::DublinCoreSchemaUri);
    assert(dcSchema);
    const KisMetaData::Schema *xmpSchema =
        KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::XMPSchemaUri);
    assert(xmpSchema);
    const KisMetaData::Schema *makerNoteSchema =
        KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::MakerNoteSchemaUri);
    assert(makerNoteSchema);

    for (const Exiv2::Exifdatum &it : exifData) {
        const uint16_t tag = it.tag();

        if (tag == Exif::Image::StripOffsets || tag == Exif::Image::RowsPerStrip || tag == Exif::Image::StripByteCounts
            || tag == Exif::Image::JPEGInterchangeFormat || tag == Exif::Image::JPEGInterchangeFormatLength
            || it.tagName() == "0x0000") {
            dbgMetaData << it.key().c_str() << " is ignored";
        } else if (tag == Exif::Photo::MakerNote) {
            store->addEntry({makerNoteSchema, "RawData", exivValueToKMDValue(it.getValue(), false)});
        } else if (tag == Exif::Image::DateTime) { // load as xmp:ModifyDate
            store->addEntry({xmpSchema, "ModifyDate", exivValueToKMDValue(it.getValue(), false)});
        } else if (tag == Exif::Image::ImageDescription) { // load as "dc:description"
            store->addEntry({dcSchema, "description", exivValueToKMDValue(it.getValue(), false)});
        } else if (tag == Exif::Image::Software) { // load as "xmp:CreatorTool"
            store->addEntry({xmpSchema, "CreatorTool", exivValueToKMDValue(it.getValue(), false)});
        } else if (tag == Exif::Image::Artist) { // load as dc:creator
            PkList<KisMetaData::Value> creators = {exivValueToKMDValue(it.getValue(), false)};
            store->addEntry({dcSchema, "creator", {creators, KisMetaData::Value::OrderedArray}});
        } else if (tag == Exif::Image::Copyright) { // load as dc:rights
            store->addEntry({dcSchema, "rights", exivValueToKMDValue(it.getValue(), false)});
        } else if (it.groupName() == "Image") {
            // Tiff tags
            const PkString fixedTN(it.tagName().c_str());
            if (tag == Exif::Image::ExifTag || tag == Exif::Image::GPSTag) {
                dbgMetaData << "Ignoring " << it.key().c_str();
            } else if (KisMetaData::Entry::isValidName(fixedTN)) {
                store->addEntry({tiffSchema, fixedTN, exivValueToKMDValue(it.getValue(), false)});
            } else {
                dbgMetaData << "Invalid tag name: " << fixedTN;
            }
        } else if (it.groupName() == "Photo") {
            // Exif tags
            KisMetaData::Value metaDataValue;
            if (tag == Exif::Photo::ExifVersion || tag == Exif::Photo::FlashpixVersion) {
                metaDataValue = exifVersionToKMDValue(it.getValue());
            } else if (tag == Exif::Photo::FileSource) {
                metaDataValue = KisMetaData::Value(3);
            } else if (tag == Exif::Photo::SceneType) {
                metaDataValue = KisMetaData::Value(1);
            } else if (tag == Exif::Photo::ComponentsConfiguration) {
                metaDataValue = exifArrayToKMDIntOrderedArray(it.getValue());
            } else if (tag == Exif::Photo::OECF) {
                metaDataValue = exifOECFToKMDOECFStructure(it.getValue(), byteOrder);
            } else if (tag == Exif::Photo::DateTimeDigitized || tag == Exif::Photo::DateTimeOriginal) {
                metaDataValue = exivValueToKMDValue(it.getValue(), false);
            } else if (tag == Exif::Photo::DeviceSettingDescription) {
                metaDataValue = deviceSettingDescriptionExifToKMD(it.getValue());
            } else if (tag == Exif::Photo::CFAPattern) {
                metaDataValue = cfaPatternExifToKMD(it.getValue(), byteOrder);
            } else if (tag == Exif::Photo::Flash) {
                metaDataValue = flashExifToKMD(it.getValue());
            } else if (tag == Exif::Photo::UserComment) {
                if (it.getValue()->typeId() != Exiv2::undefined) {
                    KisMetaData::Value vUC = exivValueToKMDValue(it.getValue(), false);
                    assert(vUC.type() == KisMetaData::Value::Variant);
                    PkVariant commentVar = vUC.asVariant();
                    PkString comment;
                    if (commentVar.type() == PkVariant::String) {
                        comment = commentVar.toString();
                    } else if (commentVar.type() == PkVariant::ByteArray) {
                        const PkByteArray commentString = commentVar.toByteArray();
                        comment = fromLatin1(commentString.constData(), commentString.size());
                    } else {
                        warnKrita << "KisExifIO: Unhandled UserComment value type.";
                    }
                    KisMetaData::Value vcomment(comment);
                    vcomment.addPropertyQualifier("xml:lang", KisMetaData::Value("x-default"));
                    PkList<KisMetaData::Value> alt;
                    alt.append(vcomment);
                    metaDataValue = KisMetaData::Value(alt, KisMetaData::Value::LangArray);
                }
            } else {
                bool forceSeq = false;
                KisMetaData::Value::ValueType arrayType = KisMetaData::Value::UnorderedArray;
                if (tag == Exif::Photo::ISOSpeedRatings) {
                    forceSeq = true;
                    arrayType = KisMetaData::Value::OrderedArray;
                }
                metaDataValue = exivValueToKMDValue(it.getValue(), forceSeq, arrayType);
            }
            if (tag == Exif::Photo::InteroperabilityTag || tag == 0xea1d
                || metaDataValue.type() == KisMetaData::Value::Invalid) { // InteroperabilityTag isn't useful for XMP,
                // 0xea1d isn't a valid Exif tag
                warnMetaData << "Ignoring " << it.key().c_str();

            } else {
                store->addEntry({exifSchema, it.tagName().c_str(), metaDataValue});
            }
        } else if (it.groupName() == "Thumbnail") {
            dbgMetaData << "Ignoring thumbnail tag :" << it.key().c_str();
        } else if (it.groupName() == "GPSInfo") {
            store->addEntry({exifSchema, it.tagName().c_str(), exivValueToKMDValue(it.getValue(), false)});
        } else {
            dbgMetaData << "Unknown exif tag, cannot load:" << it.key().c_str();
        }
    }
    ioDevice->close();
    return true;
}
