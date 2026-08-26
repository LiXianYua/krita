/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisSensorData.h"

#include <KisDynamicSensorIds.h>

#include <PkXmlDocument.h>
#include <PkXmlElement.h>

KisSensorData::KisSensorData(const KoID &sensorId)
    : id(sensorId),
      curve(DEFAULT_CURVE_STRING)
{
}

KisSensorData::~KisSensorData()
{
}

void KisSensorData::setBaseCurveRange(const PkRectF &rect)
{
    Q_UNUSED(rect);
    KIS_SAFE_ASSERT_RECOVER_NOOP(0 && "setBaseCurveRange is not implemented for standard Krita sensors");
}

PkRectF KisSensorData::baseCurveRange() const
{
    return PkRectF(0.0,0.0,1.0,1.0);
}

void KisSensorData::write(PkXmlDocument& doc, PkXmlElement &e) const
{
    e.setAttribute("id", id.id());
    if (curve != DEFAULT_CURVE_STRING) {
        PkXmlElement curve_elt = doc.createElement("curve");
        PkXmlText text = doc.createTextNode(curve);
        curve_elt.appendChild(text);
        e.appendChild(curve_elt);
    }
}

void KisSensorData::read(const PkXmlElement& e)
{
    KIS_ASSERT(e.attribute("id", "") == id.id());
    PkXmlElement curve_elt = e.firstChildElement("curve");
    if (!curve_elt.isNull()) {
        curve = curve_elt.text();
    } else {
        curve = DEFAULT_CURVE_STRING;
    }
}

void KisSensorData::reset()
{
    *this = KisSensorData(id);
}

KisSensorWithLengthData::KisSensorWithLengthData(const KoID &sensorId, const QLatin1String &lengthTag)
    : KisSensorData(sensorId)
    , m_lengthTag(lengthTag.isEmpty() ? QLatin1String("length") : lengthTag)
{
    if (sensorId == FadeId) {
        isPeriodic = false;
        length = 1000;
    } else if (sensorId == DistanceId) {
        isPeriodic = false;
        length = 30;
    } else if (sensorId == TimeId) {
        isPeriodic = false;
        length = 30;
    } else {
        qFatal("This sensor type \"%s\" has no length associated!", sensorId.id().PkToUtf8().c_str());
    }
}

void KisSensorWithLengthData::write(PkXmlDocument &doc, PkXmlElement &e) const
{
    KisSensorData::write(doc, e);
    e.setAttribute("periodic", isPeriodic ? "1" : "0");
    e.setAttribute(m_lengthTag, PkString(std::to_string(length).c_str()));
}

void KisSensorWithLengthData::read(const PkXmlElement &e)
{
    reset();
    KisSensorData::read(e);

    if (e.hasAttribute("periodic")) {
        isPeriodic = e.attribute("periodic").toInt();
    }

    if (e.hasAttribute(m_lengthTag)) {
        length = e.attribute(m_lengthTag).toInt();
    }
}

void KisSensorWithLengthData::reset()
{
    *this = KisSensorWithLengthData(id, m_lengthTag);
}

KisDrawingAngleSensorData::KisDrawingAngleSensorData()
    : KisSensorData(DrawingAngleId)
{
}

void KisDrawingAngleSensorData::write(PkXmlDocument &doc, PkXmlElement &e) const
{
    KisSensorData::write(doc, e);
    e.setAttribute("fanCornersEnabled", fanCornersEnabled ? "1" : "0");
    e.setAttribute("fanCornersStep", PkString(std::to_string(fanCornersStep).c_str()));
    e.setAttribute("angleOffset", PkString(std::to_string(angleOffset).c_str()));
    e.setAttribute("lockedAngleMode", lockedAngleMode ? "1" : "0");
}

void KisDrawingAngleSensorData::read(const PkXmlElement &e)
{
    reset();
    KisSensorData::read(e);

    if (e.hasAttribute("fanCornersEnabled")) {
        fanCornersEnabled = e.attribute("fanCornersEnabled").toInt();
    }
    if (e.hasAttribute("fanCornersStep")) {
        fanCornersStep = e.attribute("fanCornersStep").toInt();
    }
    if (e.hasAttribute("angleOffset")) {
        angleOffset = e.attribute("angleOffset").toDouble();
    }
    if (e.hasAttribute("lockedAngleMode")) {
        lockedAngleMode = e.attribute("lockedAngleMode").toInt();
    }
}

void KisDrawingAngleSensorData::reset()
{
    *this = KisDrawingAngleSensorData();
}
