/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_asl_xml_writer.h"

#include <PkColor.h>
#include <PkXmlDocument.h>
#include <PkPoint.h>
#include <PkTransform.h>
#include <PkRect.h>
#include <PkPolygon.h>
#include <PkVariant.h>
#include <PkAuxTypes.h>
#include <PkCosMemoryStream.h>

#include <resources/KoPattern.h>
#include "kis_asl_byte_utils.h"
#include <resources/KoSegmentGradient.h>
#include <resources/KoStopGradient.h>

#include <cfloat>

#include "kis_asl_writer_utils.h"
#include "kis_dom_utils.h"

struct KisAslXmlWriter::Private {
    PkXmlDocument document;
    PkXmlElement currentElement;
};

KisAslXmlWriter::KisAslXmlWriter()
    : m_d(new Private)
{
    PkXmlElement el = m_d->document.createElement("asl");
    m_d->document.appendChild(el);
    m_d->currentElement = el;
}

KisAslXmlWriter::~KisAslXmlWriter()
{
}

PkXmlDocument KisAslXmlWriter::document() const
{
    // PkXmlElement 无 operator!=：以「currentElement 是否仍是文档的直接子元素」
    // 等价判定「是否已平衡回根元素」。
    if (m_d->currentElement.isNull() || m_d->currentElement.parentNode().isElement()) {
        warnKrita << "KisAslXmlWriter::document(): unbalanced enter/leave descriptor/array";
    }

    return m_d->document;
}

void KisAslXmlWriter::enterDescriptor(const PkString &key, const PkString &name, const PkString &classId)
{
    PkXmlElement el = m_d->document.createElement("node");

    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }

    el.setAttribute("type", "Descriptor");
    el.setAttribute("name", name);
    el.setAttribute("classId", classId);

    m_d->currentElement.appendChild(el);
    m_d->currentElement = el;
}

void KisAslXmlWriter::leaveDescriptor()
{
    if (!m_d->currentElement.parentNode().toElement().isNull()) {
        m_d->currentElement = m_d->currentElement.parentNode().toElement();
    } else {
        warnKrita << "KisAslXmlWriter::leaveDescriptor(): unbalanced enter/leave descriptor";
    }
}

void KisAslXmlWriter::enterList(const PkString &key)
{
    PkXmlElement el = m_d->document.createElement("node");

    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }

    el.setAttribute("type", "List");

    m_d->currentElement.appendChild(el);
    m_d->currentElement = el;
}

void KisAslXmlWriter::leaveList()
{
    if (!m_d->currentElement.parentNode().toElement().isNull()) {
        m_d->currentElement = m_d->currentElement.parentNode().toElement();
    } else {
        warnKrita << "KisAslXmlWriter::leaveList(): unbalanced enter/leave list";
    }
}

void KisAslXmlWriter::writeDouble(const PkString &key, double value)
{
    PkXmlElement el = m_d->document.createElement("node");

    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }

    el.setAttribute("type", "Double");
    el.setAttribute("value", KisDomUtils::toString(value));

    m_d->currentElement.appendChild(el);
}

void KisAslXmlWriter::writeInteger(const PkString &key, int value)
{
    PkXmlElement el = m_d->document.createElement("node");

    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }

    el.setAttribute("type", "Integer");
    el.setAttribute("value", KisDomUtils::toString(value));

    m_d->currentElement.appendChild(el);
}

void KisAslXmlWriter::writeEnum(const PkString &key, const PkString &typeId, const PkString &value)
{
    PkXmlElement el = m_d->document.createElement("node");

    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }

    el.setAttribute("type", "Enum");
    el.setAttribute("typeId", typeId);
    el.setAttribute("value", value);

    m_d->currentElement.appendChild(el);
}

void KisAslXmlWriter::writeUnitFloat(const PkString &key, const PkString &unit, double value)
{
    PkXmlElement el = m_d->document.createElement("node");

    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }

    el.setAttribute("type", "UnitFloat");
    el.setAttribute("unit", unit);
    el.setAttribute("value", KisDomUtils::toString(value));

    m_d->currentElement.appendChild(el);
}

void KisAslXmlWriter::writeText(const PkString &key, const PkString &value)
{
    PkXmlElement el = m_d->document.createElement("node");

    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }

    el.setAttribute("type", "Text");
    el.setAttribute("value", value);

    m_d->currentElement.appendChild(el);
}

void KisAslXmlWriter::writeBoolean(const PkString &key, bool value)
{
    PkXmlElement el = m_d->document.createElement("node");

    if (!key.isEmpty()) {
        el.setAttribute("key", key);
    }

    el.setAttribute("type", "Boolean");
    el.setAttribute("value", KisDomUtils::toString(value));

    m_d->currentElement.appendChild(el);
}

void KisAslXmlWriter::writeColor(const PkString &key, const KoColor &value)
{
    PkXmlDocument doc;
    PkXmlElement el = doc.createElement("color");
    value.toXML(doc, el);
    PkXmlElement colorEl = el.firstChildElement();
    if (value.colorSpace()->colorModelId() == RGBAColorModelID) {
        enterDescriptor(key, "", "RGBC");

        double v = qBound(0.0, KisDomUtils::toDouble(colorEl.attribute("r", "0.0")) * 255.0, 255.0);
        writeDouble("Rd  ", v);
        v = qBound(0.0, KisDomUtils::toDouble(colorEl.attribute("g", "0.0")) * 255.0, 255.0);
        writeDouble("Grn ", v);
        v = qBound(0.0, KisDomUtils::toDouble(colorEl.attribute("b", "0.0")) * 255.0, 255.0);
        writeDouble("Bl  ", v);
    } else if (value.colorSpace()->colorModelId() == CMYKAColorModelID) {
        enterDescriptor(key, "", "CMYC");

        double v = qBound(0.0, KisDomUtils::toDouble(colorEl.attribute("c", "0.0")) * 100.0, 100.0);
        writeDouble("Cyn ", v);
        v = qBound(0.0, KisDomUtils::toDouble(colorEl.attribute("m", "0.0")) * 100.0, 100.0);
        writeDouble("Mgnt", v);
        v = qBound(0.0, KisDomUtils::toDouble(colorEl.attribute("y", "0.0")) * 100.0, 100.0);
        writeDouble("Ylw ", v);
        v = qBound(0.0, KisDomUtils::toDouble(colorEl.attribute("k", "0.0")) * 100.0, 100.0);
        writeDouble("Blck", v);
    } else if (value.colorSpace()->colorModelId() == LABAColorModelID) {
        enterDescriptor(key, "", "LbCl");

        double v = KisDomUtils::toDouble(colorEl.attribute("L", "0.0"));
        writeDouble("Lmnc", v);
        v = KisDomUtils::toDouble(colorEl.attribute("a", "0.0"));
        writeDouble("A   ", v);
        v = KisDomUtils::toDouble(colorEl.attribute("b", "0.0"));
        writeDouble("B   ", v);
    } else if (value.colorSpace()->colorModelId() == GrayAColorModelID) {
        enterDescriptor(key, "", "Grsc");

        double v = qBound(0.0, KisDomUtils::toDouble(colorEl.attribute("g", "0.0")) * 100.0, 100.0);
        writeDouble("Gry ", v);
    } else { // default to sRGB
        enterDescriptor(key, "", "RGBC");

        writeDouble("Rd  ", value.toQColor().red());
        writeDouble("Grn ", value.toQColor().green());
        writeDouble("Bl  ", value.toQColor().blue());
    }
    if (value.metadata().contains("psdSpotBook")) {
        PkVariant v;
        v = value.metadata().value("spotName");
        if (v.isValid()) {
            writeText("Nm  ", v.toString());
        }
        v = value.metadata().value("psdSpotBook");
        if (v.isValid()) {
            writeText("Bk  ", v.toString());
        }
        // PkVariant::toInt 无 bool* 重载；psdSpotBookId 以 Int 存入（见
        // parseColorObject 的 addMetadata("psdSpotBookId", spotValue)），
        // 用 type() 判断可转换，对齐原 variant 语义里 toInt(&ok) 的成功条件。
        v = value.metadata().value("psdSpotBookId");
        const bool ok = v.isValid() && v.type() == PkVariant::Int;
        const int bookid = v.toInt();
        if (ok) {
            writeInteger("bookID", bookid);
        }
    }

    leaveDescriptor();
}

void KisAslXmlWriter::writePoint(const PkString &key, const PkPointF &value)
{
    enterDescriptor(key, "", "CrPt");

    writeDouble("Hrzn", value.x());
    writeDouble("Vrtc", value.y());

    leaveDescriptor();
}

void KisAslXmlWriter::writePhasePoint(const PkString &key, const PkPointF &value)
{
    enterDescriptor(key, "", "Pnt ");

    writeDouble("Hrzn", value.x());
    writeDouble("Vrtc", value.y());

    leaveDescriptor();
}

void KisAslXmlWriter::writeOffsetPoint(const PkString &key, const PkPointF &value)
{
    enterDescriptor(key, "", "Pnt ");

    writeUnitFloat("Hrzn", "#Prc", value.x());
    writeUnitFloat("Vrtc", "#Prc", value.y());

    leaveDescriptor();
}

void KisAslXmlWriter::writeCurve(const PkString &key, const PkString &name, const PkVector<PkPointF> &points)
{
    enterDescriptor(key, "", "ShpC");

    writeText("Nm  ", name);

    enterList("Crv ");

    for (const PkPointF &pt : points) {
        writePoint("", pt);
    }

    leaveList();
    leaveDescriptor();
}

PkString KisAslXmlWriter::writePattern(const PkString &key, const KoPatternSP pattern)
{
    enterDescriptor(key, "", "KisPattern");

    writeText("Nm  ", pattern->name());

    PkString uuid = KisAslWriterUtils::getPatternUuidLazy(pattern);
    writeText("Idnt", uuid);

    // Write pattern data

    PkByteArray patBytes;
    PkCosMemoryStream buffer(&patBytes);
    buffer.open(PkStream::WriteOnly);
    pattern->savePatToDevice(&buffer);

    PkXmlCDATASection dataSection = m_d->document.createCDATASection(pkToBase64(pkQCompress(patBytes)));

    PkXmlElement dataElement = m_d->document.createElement("node");
    dataElement.setAttribute("type", "KisPatternData");
    dataElement.setAttribute("key", "Data");
    dataElement.appendChild(dataSection);

    m_d->currentElement.appendChild(dataElement);

    leaveDescriptor();

    return uuid;
}

void KisAslXmlWriter::writePatternRef(const PkString &key, const KoPatternSP pattern, const PkString &uuid)
{
    enterDescriptor(key, "", "Ptrn");

    writeText("Nm  ", pattern->name());
    writeText("Idnt", uuid);

    leaveDescriptor();
}

void KisAslXmlWriter::writeGradientImpl(const PkString &key,
                                        const PkString &name,
                                        PkVector<KoColor> colors,
                                        PkVector<qreal> transparencies,
                                        PkVector<qreal> positions,
                                        PkVector<PkString> types,
                                        PkVector<qreal> middleOffsets)
{
    enterDescriptor(key, "Gradient", "Grdn");

    writeText("Nm  ", name);
    writeEnum("GrdF", "GrdF", "CstS");
    writeDouble("Intr", 4096);

    enterList("Clrs");

    for (int i = 0; i < colors.size(); i++) {
        enterDescriptor("", "", "Clrt");

        writeColor("Clr ", colors[i]);
        writeEnum("Type", "Clry", types[i]);
        writeInteger("Lctn", positions[i] * 4096.0);
        writeInteger("Mdpn", middleOffsets[i] * 100.0);

        leaveDescriptor();
    };

    leaveList();

    enterList("Trns");

    for (int i = 0; i < colors.size(); i++) {
        enterDescriptor("", "", "TrnS");
        writeUnitFloat("Opct", "#Prc", transparencies[i] * 100.0);
        writeInteger("Lctn", positions[i] * 4096.0);
        writeInteger("Mdpn", middleOffsets[i] * 100.0);
        leaveDescriptor();
    };

    leaveList();

    leaveDescriptor();
}

PkString KisAslXmlWriter::getSegmentEndpointTypeString(KoGradientSegmentEndpointType segtype)
{
    switch (segtype) {
    case COLOR_ENDPOINT:
        return "UsrS";
        break;
    case FOREGROUND_ENDPOINT:
    case FOREGROUND_TRANSPARENT_ENDPOINT:
        return "FrgC";
        break;
    case BACKGROUND_ENDPOINT:
    case BACKGROUND_TRANSPARENT_ENDPOINT:
        return "BckC";
        break;
    default:
        return "UsrS";
    }
}

void KisAslXmlWriter::writeSegmentGradient(const PkString &key, const KoSegmentGradient &gradient)
{
    const PkList<KoGradientSegment *> &segments = gradient.segments();
    KIS_SAFE_ASSERT_RECOVER_RETURN(!segments.isEmpty());

    PkVector<KoColor> colors;
    PkVector<qreal> transparencies;
    PkVector<qreal> positions;
    PkVector<PkString> types;
    PkVector<qreal> middleOffsets;

    for (const KoGradientSegment *seg : segments) {
        const qreal start = seg->startOffset();
        const qreal end = seg->endOffset();
        const qreal mid = (end - start) > DBL_EPSILON ? (seg->middleOffset() - start) / (end - start) : 0.5;

        KoColor color = seg->startColor();
        qreal transparency = color.opacityF();
        color.setOpacity(1.0);

        PkString type = getSegmentEndpointTypeString(seg->startType());

        colors << color;
        transparencies << transparency;
        positions << start;
        types << type;
        middleOffsets << mid;
    }

    // last segment

    if (!segments.isEmpty()) {
        const KoGradientSegment *lastSeg = segments.last();

        KoColor color = lastSeg->endColor();
        qreal transparency = color.opacityF();
        color.setOpacity(1.0);
        PkString type = getSegmentEndpointTypeString(lastSeg->endType());

        colors << color;
        transparencies << transparency;
        positions << lastSeg->endOffset();
        types << type;
        middleOffsets << 0.5;
    }

    writeGradientImpl(key, gradient.name(), colors, transparencies, positions, types, middleOffsets);
}

void KisAslXmlWriter::writeStopGradient(const PkString &key, const KoStopGradient &gradient)
{
    PkVector<KoColor> colors;
    PkVector<qreal> transparencies;
    PkVector<qreal> positions;
    PkVector<PkString> types;
    PkVector<qreal> middleOffsets;

    for (const KoGradientStop &stop : gradient.stops()) {
        KoColor color = stop.color;
        qreal transparency = color.opacityF();
        color.setOpacity(1.0);

        PkString type;
        switch (stop.type) {
        case COLORSTOP:
            type = "UsrS";
            break;
        case FOREGROUNDSTOP:
            type = "FrgC";
            break;
        case BACKGROUNDSTOP:
            type = "BckC";
            break;
        }

        colors << color;
        transparencies << transparency;
        positions << stop.position;
        types << type;
        middleOffsets << 0.5;
    }

    writeGradientImpl(key, gradient.name(), colors, transparencies, positions, types, middleOffsets);
}

void KisAslXmlWriter::writeRawData(const PkString key, const PkByteArray *rawData)
{
    PkXmlCDATASection dataSection = m_d->document.createCDATASection(pkToBase64(*rawData));
    PkXmlElement dataElement = m_d->document.createElement("node");
    dataElement.setAttribute("type", "RawData");
    dataElement.setAttribute("key", key);
    dataElement.appendChild(dataSection);
    m_d->currentElement.appendChild(dataElement);
}

void KisAslXmlWriter::writeTransform(const PkString &key, const PkTransform &transform)
{
    enterDescriptor(key, "Transform", "Trnf");

    writeDouble("xx", transform.m11());
    writeDouble("xy", transform.m12());
    writeDouble("yx", transform.m21());
    writeDouble("yy", transform.m22());
    writeDouble("tx", transform.dx());
    writeDouble("ty", transform.dy());

    leaveDescriptor();
}

void KisAslXmlWriter::writeUnitRect(const PkString &key, const PkString &unit, const PkRectF &rect)
{
    enterDescriptor(key, "", "unitRect");

    writeInteger("unitValueQuadVersion", 1);
    writeUnitFloat("Top ", unit, rect.top());
    writeUnitFloat("Left", unit, rect.left());
    writeUnitFloat("Btom", unit, rect.bottom());
    writeUnitFloat("Rght", unit, rect.right());

    leaveDescriptor();
}

void KisAslXmlWriter::writeFloatRect(const PkString &key, const PkRectF &rect)
{
    enterDescriptor(key, "", "classFloatRect");

    writeDouble("Top ", rect.top());
    writeDouble("Left", rect.left());
    writeDouble("Btom", rect.bottom());
    writeDouble("Rght", rect.right());

    leaveDescriptor();
}

void KisAslXmlWriter::writePointRect(const PkString &key, const PkPolygonF &transformedRect)
{
    if (transformedRect.size() < 4) {
        warnKrita << "KisAslXmlWriter::writePointRect(): too few points to write descriptor.";
        return;
    }
    enterDescriptor(key, "", "null");

    writePoint("rectangleCornerA", transformedRect.at(0));
    writePoint("rectangleCornerB", transformedRect.at(1));
    writePoint("rectangleCornerC", transformedRect.at(2));
    writePoint("rectangleCornerD", transformedRect.at(3));

    leaveDescriptor();
}
