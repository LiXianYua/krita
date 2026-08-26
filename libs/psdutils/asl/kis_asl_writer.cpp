/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_asl_writer.h"

#include <PkXmlDocument.h>
#include <PkStream.h>

#include "kis_dom_utils.h"

#include "kis_debug.h"
#include "psd.h"
#include "psd_utils.h"

#include "kis_asl_byte_utils.h"
#include "kis_asl_patterns_writer.h"
#include "kis_asl_writer_utils.h"

namespace Private
{
using namespace KisAslWriterUtils;

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void parseElement(const PkXmlElement &el, PkStream &device, bool forceTypeInfo = false)
{
    KIS_ASSERT_RECOVER_RETURN(el.tagName() == "node");

    PkString type = el.attribute("type", "<unknown>");
    PkString key = el.attribute("key", "");

    // should be filtered on a higher level
    KIS_ASSERT_RECOVER_RETURN(key != ResourceType::Patterns);

    if (type == "Descriptor") {
        if (!key.isEmpty()) {
            writeVarString<byteOrder>(key, device);
        }

        if (!key.isEmpty() || forceTypeInfo) {
            writeFixedString<byteOrder>("Objc", device);
        }

        PkString classId = el.attribute("classId", "");
        PkString name = el.attribute("name", "");

        writeUnicodeString<byteOrder>(name, device);
        writeVarString<byteOrder>(classId, device);

        quint32 numChildren = static_cast<quint32>(el.childNodes().size());
        SAFE_WRITE_EX(byteOrder, device, numChildren);

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            parseElement<byteOrder>(child.toElement(), device);
            child = child.nextSibling();
        }

    } else if (type == "List") {
        writeVarString<byteOrder>(key, device);
        writeFixedString<byteOrder>("VlLs", device);

        quint32 numChildren = static_cast<quint32>(el.childNodes().size());
        SAFE_WRITE_EX(byteOrder, device, numChildren);

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            parseElement<byteOrder>(child.toElement(), device, true);
            child = child.nextSibling();
        }
    } else if (type == "Double") {
        double v = KisDomUtils::toDouble(el.attribute("value", "0"));

        writeVarString<byteOrder>(key, device);
        writeFixedString<byteOrder>("doub", device);
        SAFE_WRITE_EX(byteOrder, device, v);

    } else if (type == "UnitFloat") {
        double v = KisDomUtils::toDouble(el.attribute("value", "0"));
        PkString unit = el.attribute("unit", "#Pxl");

        if (!key.isEmpty()) {
            writeVarString<byteOrder>(key, device);
        }
        writeFixedString<byteOrder>("UntF", device);
        writeFixedString<byteOrder>(unit, device);
        SAFE_WRITE_EX(byteOrder, device, v);
    } else if (type == "Text") {
        PkString v = el.attribute("value", "");
        writeVarString<byteOrder>(key, device);
        writeFixedString<byteOrder>("TEXT", device);
        writeUnicodeString<byteOrder>(v, device);
    } else if (type == "Enum") {
        PkString v = el.attribute("value", "");
        PkString typeId = el.attribute("typeId", "DEAD");
        writeVarString<byteOrder>(key, device);
        writeFixedString<byteOrder>("enum", device);
        writeVarString<byteOrder>(typeId, device);
        writeVarString<byteOrder>(v, device);
    } else if (type == "Integer") {
        quint32 v = static_cast<quint32>(KisDomUtils::toInt(el.attribute("value", "0")));
        writeVarString<byteOrder>(key, device);
        writeFixedString<byteOrder>("long", device);
        SAFE_WRITE_EX(byteOrder, device, v);
    } else if (type == "Boolean") {
        quint8 v = static_cast<quint8>(KisDomUtils::toInt(el.attribute("value", "0")));

        writeVarString<byteOrder>(key, device);
        writeFixedString<byteOrder>("bool", device);
        SAFE_WRITE_EX(byteOrder, device, v);
    } else if (type == "RawData" && key == "EngineData") {
        writeVarString<byteOrder>(key, device);
        writeFixedString<byteOrder>("tdta", device);

        PkXmlNode dataNode = el.firstChild();

        if (!dataNode.isCDATASection()) {
            warnKrita << "WARNING: failed to parse RawData XML section!";
            return;
        }

        PkXmlCDATASection dataSection = dataNode.toCDATASection();
        PkByteArray data = pkFromBase64(dataSection.data());

        if (data.isEmpty()) {
            warnKrita << "WARNING: failed to parse RawData XML section!";
        }
        quint32 length = data.size();
        SAFE_WRITE_EX(byteOrder, device, length);
        device.write(data.constData(), data.size());
    } else {
        warnKrita << "WARNING: XML (ASL) Unknown element type:" << type << ppVar(key);
    }
}

int calculateNumStyles(const PkXmlElement &root)
{
    int numStyles = 0;
    PkXmlNode child = root.firstChild();

    while (!child.isNull()) {
        PkXmlElement el = child.toElement();
        PkString classId = el.attribute("classId", "");

        if (classId == "null") {
            numStyles++;
        }

        child = child.nextSibling();
    }

    return numStyles;
}

// No need for endianness, Photoshop-specific
void writeFileImpl(PkStream &device, const PkXmlDocument &doc)
{
    {
        quint16 stylesVersion = 2;
        SAFE_WRITE_EX(psd_byte_order::psdBigEndian, device, stylesVersion);
    }

    {
        PkString signature("8BSL");
        if (!device.write(psdToLatin1(signature).data(), 4)) {
            throw ASLWriteException("Failed to write ASL signature");
        }
    }

    {
        quint16 patternsVersion = 3;
        SAFE_WRITE_EX(psd_byte_order::psdBigEndian, device, patternsVersion);
    }

    {
        KisAslWriterUtils::OffsetStreamPusher<quint32, psd_byte_order::psdBigEndian> patternsSizeField(device);

        KisAslPatternsWriter patternsWriter(doc, device, psd_byte_order::psdBigEndian);
        patternsWriter.writePatterns();
    }

    PkXmlElement root = doc.documentElement();
    KIS_ASSERT_RECOVER_RETURN(root.tagName() == "asl");

    int numStyles = calculateNumStyles(root);
    KIS_ASSERT_RECOVER_RETURN(numStyles > 0);

    {
        const quint32 numStylesTag = static_cast<quint32>(numStyles);
        SAFE_WRITE_EX(psd_byte_order::psdBigEndian, device, numStylesTag);
    }

    PkXmlNode child = root.firstChild();

    for (int styleIndex = 0; styleIndex < numStyles; styleIndex++) {
        KisAslWriterUtils::OffsetStreamPusher<quint32, psd_byte_order::psdBigEndian> theOnlyStyleSizeField(device);

        KIS_ASSERT_RECOVER_RETURN(!child.isNull());

        {
            quint32 stylesFormatVersion = 16;
            SAFE_WRITE_EX(psd_byte_order::psdBigEndian, device, stylesFormatVersion);
        }

        while (!child.isNull()) {
            PkXmlElement el = child.toElement();
            PkString key = el.attribute("key", "");

            if (key != ResourceType::Patterns)
                break;

            child = child.nextSibling();
        }

        parseElement(child.toElement(), device);
        child = child.nextSibling();

        {
            quint32 stylesFormatVersion = 16;
            SAFE_WRITE_EX(psd_byte_order::psdBigEndian, device, stylesFormatVersion);
        }

        parseElement(child.toElement(), device);
        child = child.nextSibling();

        // ASL files' size should be 4-bytes aligned
        const qint64 paddingSize = 4 - (device.pos() & 0x3);
        if (paddingSize != 4) {
            PkByteArray padding;
            padding.resize(static_cast<int>(paddingSize));
            device.write(padding.constData(), padding.size());
        }
    }
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void writePsdLfx2SectionImpl(PkStream &device, const PkXmlDocument &doc)
{
    PkXmlElement root = doc.documentElement();
    KIS_ASSERT_RECOVER_RETURN(root.tagName() == "asl");

    int numStyles = calculateNumStyles(root);
    KIS_ASSERT_RECOVER_RETURN(numStyles == 1);

    {
        quint32 objectEffectsVersion = 0;
        SAFE_WRITE_EX(byteOrder, device, objectEffectsVersion);
    }

    {
        quint32 descriptorVersion = 16;
        SAFE_WRITE_EX(byteOrder, device, descriptorVersion);
    }

    PkXmlNode child = root.firstChild();

    while (!child.isNull()) {
        PkXmlElement el = child.toElement();
        PkString key = el.attribute("key", "");

        if (key != ResourceType::Patterns)
            break;

        child = child.nextSibling();
    }

    parseElement<byteOrder>(child.toElement(), device);
    child = child.nextSibling();

    // ASL files' size should be 4-bytes aligned
    const qint64 paddingSize = 4 - (device.pos() & 0x3);
    if (paddingSize != 4) {
        PkByteArray padding;
        padding.resize(static_cast<int>(paddingSize));
        device.write(padding.constData(), padding.size());
    }
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void writeFillLayerSectionImpl(PkStream &device, const PkXmlDocument &doc)
{
    PkXmlElement root = doc.documentElement();
    KIS_ASSERT_RECOVER_RETURN(root.tagName() == "asl");

    {
        quint32 descriptorVersion = 16;
        SAFE_WRITE_EX(byteOrder, device, descriptorVersion);
    }

    PkXmlNode child = root.firstChild();

    while (!child.isNull()) {
        PkXmlElement el = child.toElement();
        PkString key = el.attribute("key", "");

        if (key != ResourceType::Patterns)
            break;

        child = child.nextSibling();
    }

    parseElement<byteOrder>(child.toElement(), device);
    child = child.nextSibling();

    // ASL files' size should be 4-bytes aligned
    const qint64 paddingSize = 4 - (device.pos() & 0x3);
    if (paddingSize != 4) {
        PkByteArray padding;
        padding.resize(static_cast<int>(paddingSize));
        device.write(padding.constData(), padding.size());
    }
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void writeTypeToolSectionImpl(PkStream &device, const PkXmlDocument &doc, const PkXmlDocument &warpDoc, const PkTransform tf, const PkRectF bounds)
{
    PkXmlElement root = doc.documentElement();
    KIS_ASSERT_RECOVER_RETURN(root.tagName() == "asl");

    {
        quint16 descriptorVersion = 1;
        SAFE_WRITE_EX(byteOrder, device, descriptorVersion);
    }

    {
        SAFE_WRITE_EX(byteOrder, device, double(tf.m11()));
        SAFE_WRITE_EX(byteOrder, device, double(tf.m12()));
        SAFE_WRITE_EX(byteOrder, device, double(tf.m21()));
        SAFE_WRITE_EX(byteOrder, device, double(tf.m22()));
        SAFE_WRITE_EX(byteOrder, device, double(tf.dx()));
        SAFE_WRITE_EX(byteOrder, device, double(tf.dy()));
    }

    {
        quint16 textVersion = 50;
        SAFE_WRITE_EX(byteOrder, device, textVersion);
        quint32 descriptorVersion = 16;
        SAFE_WRITE_EX(byteOrder, device, descriptorVersion);
    }

    PkXmlNode child = root.firstChild();
    parseElement<byteOrder>(child.toElement(), device);

    // warp data
    {
        quint16 textVersion = 1;
        SAFE_WRITE_EX(byteOrder, device, textVersion);
        quint32 descriptorVersion = 16;
        SAFE_WRITE_EX(byteOrder, device, descriptorVersion);
    }

    PkXmlElement warpRoot = warpDoc.documentElement();
    KIS_ASSERT_RECOVER_RETURN(warpRoot.tagName() == "asl");

    PkXmlNode warpChild = warpRoot.firstChild();
    parseElement<byteOrder>(warpChild.toElement(), device);

    {
        SAFE_WRITE_EX(byteOrder, device, double(bounds.left()));
        SAFE_WRITE_EX(byteOrder, device, double(bounds.top()));
        SAFE_WRITE_EX(byteOrder, device, double(bounds.right()));
        SAFE_WRITE_EX(byteOrder, device, double(bounds.bottom()));
    }

    // ASL files' size should be 4-bytes aligned
    const qint64 paddingSize = 4 - (device.pos() & 0x3);
    if (paddingSize != 4) {
        PkByteArray padding;
        padding.resize(static_cast<int>(paddingSize));
        device.write(padding.constData(), padding.size());
    }
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void writeVectorStrokeDataImpl(PkStream &device, const PkXmlDocument &doc)
{
    PkXmlElement root = doc.documentElement();
    KIS_ASSERT_RECOVER_RETURN(root.tagName() == "asl");

    {
        quint32 descriptorVersion = 16;
        SAFE_WRITE_EX(byteOrder, device, descriptorVersion);
    }

    PkXmlNode child = root.firstChild();
    parseElement<byteOrder>(child.toElement(), device);

    // ASL files' size should be 4-bytes aligned
    const qint64 paddingSize = 4 - (device.pos() & 0x3);
    if (paddingSize != 4) {
        PkByteArray padding;
        padding.resize(static_cast<int>(paddingSize));
        device.write(padding.constData(), padding.size());
    }
}

template<psd_byte_order byteOrder = psd_byte_order::psdBigEndian>
void writeVectorOriginationDataImpl(PkStream &device, const PkXmlDocument &doc)
{
    PkXmlElement root = doc.documentElement();
    KIS_ASSERT_RECOVER_RETURN(root.tagName() == "asl");

    {
        quint32 version = 1;
        SAFE_WRITE_EX(byteOrder, device, version);
    }
    {
        quint32 descriptorVersion = 16;
        SAFE_WRITE_EX(byteOrder, device, descriptorVersion);
    }

    PkXmlNode child = root.firstChild();
    parseElement<byteOrder>(child.toElement(), device);

    // ASL files' size should be 4-bytes aligned
    const qint64 paddingSize = 4 - (device.pos() & 0x3);
    if (paddingSize != 4) {
        PkByteArray padding;
        padding.resize(static_cast<int>(paddingSize));
        device.write(padding.constData(), padding.size());
    }
}

} // namespace

KisAslWriter::KisAslWriter(psd_byte_order byteOrder)
    : m_byteOrder(byteOrder)
{
}

void KisAslWriter::writeFile(PkStream &device, const PkXmlDocument &doc)
{
    try {
        Private::writeFileImpl(device, doc);
    } catch (Private::ASLWriteException &e) {
        warnKrita << "WARNING: ASL:" << e.what();
    }
}

void KisAslWriter::writeFillLayerSectionEx(PkStream &device, const PkXmlDocument &doc)
{
    switch (m_byteOrder) {
    case psd_byte_order::psdLittleEndian:
        Private::writeFillLayerSectionImpl<psd_byte_order::psdLittleEndian>(device, doc);
        break;
    default:
        Private::writeFillLayerSectionImpl(device, doc);
        break;
    }
}

void KisAslWriter::writePsdLfx2SectionEx(PkStream &device, const PkXmlDocument &doc)
{
    switch (m_byteOrder) {
    case psd_byte_order::psdLittleEndian:
        Private::writePsdLfx2SectionImpl<psd_byte_order::psdLittleEndian>(device, doc);
        break;
    default:
        Private::writePsdLfx2SectionImpl(device, doc);
        break;
    }
}

void KisAslWriter::writeTypeToolObjectSettings(PkStream &device, const PkXmlDocument &doc, const PkXmlDocument &warpDoc, const PkTransform tf, const PkRectF bounds)
{
    switch (m_byteOrder) {
    case psd_byte_order::psdLittleEndian:
        Private::writeTypeToolSectionImpl<psd_byte_order::psdLittleEndian>(device, doc, warpDoc, tf, bounds);
        break;
    default:
        Private::writeTypeToolSectionImpl(device, doc, warpDoc, tf, bounds);
        break;
    }
}

void KisAslWriter::writeVectorStrokeDataEx(PkStream &device, const PkXmlDocument &doc)
{
    switch (m_byteOrder) {
    case psd_byte_order::psdLittleEndian:
        Private::writeVectorStrokeDataImpl<psd_byte_order::psdLittleEndian>(device, doc);
        break;
    default:
        Private::writeVectorStrokeDataImpl(device, doc);
        break;
    }
}

void KisAslWriter::writeVectorOriginationDataEx(PkStream &device, const PkXmlDocument &doc)
{
    switch (m_byteOrder) {
    case psd_byte_order::psdLittleEndian:
        Private::writeVectorOriginationDataImpl<psd_byte_order::psdLittleEndian>(device, doc);
        break;
    default:
        Private::writeVectorOriginationDataImpl(device, doc);
        break;
    }
}
