/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 * SPDX-FileCopyrightText: 2022 Julian Schmidt <julisch1107@web.de>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "RulerAssistant.h"


#include <PkTransform.h>

#include <kis_dom_utils.h>

#include <math.h>

RulerAssistant::RulerAssistant()
    : RulerAssistant("ruler", PkString("Ruler assistant"))
{
}

RulerAssistant::RulerAssistant(const PkString& id, const PkString& name)
    : KisPaintingAssistant(id, name)
{
}

KisPaintingAssistantSP RulerAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new RulerAssistant(*this, handleMap));
}

RulerAssistant::RulerAssistant(const RulerAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
    , m_subdivisions(rhs.m_subdivisions)
    , m_minorSubdivisions(rhs.m_minorSubdivisions)
    , m_hasFixedLength(rhs.m_hasFixedLength)
    , m_fixedLength(rhs.m_fixedLength)
    , m_fixedLengthUnit(rhs.m_fixedLengthUnit)
{
}

PkPointF RulerAssistant::project(const PkPointF& pt) const
{
    assert(isAssistantComplete());
    PkPointF pt1 = *handles()[0];
    PkPointF pt2 = *handles()[1];
    
    PkPointF a = pt - pt1;
    PkPointF u = pt2 - pt1;
    
    qreal u_norm = sqrt(u.x() * u.x() + u.y() * u.y());
    
    if(u_norm == 0) return pt;
    
    u /= u_norm;
    
    double t = a.x() * u.x() + a.y() * u.y();
    
    if(t < 0.0) return pt1;
    if(t > u_norm) return pt2;
    
    return t * u + pt1;
}

PkPointF RulerAssistant::adjustPosition(const PkPointF& pt, const PkPointF& /*strokeBegin*/, const bool /*snapToAny*/, qreal /*moveThresholdPt*/)
{
    return project(pt);
}

void RulerAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    point = project(point);
    strokeBegin = project(strokeBegin);
}





PkPointF RulerAssistant::getDefaultEditorPosition() const
{
    return (*handles()[0] + *handles()[1]) * 0.5;
}

bool RulerAssistant::isAssistantComplete() const
{
    return handles().size() >= 2;
}

int RulerAssistant::subdivisions() const {
    return m_subdivisions;
}

void RulerAssistant::setSubdivisions(int subdivisions) {
    if (subdivisions < 0) {
        m_subdivisions = 0;
    } else {
        m_subdivisions = subdivisions;
    }
}

int RulerAssistant::minorSubdivisions() const {
    return m_minorSubdivisions;
}

void RulerAssistant::setMinorSubdivisions(int subdivisions) {
    if (subdivisions < 0) {
        m_minorSubdivisions = 0;
    } else {
        m_minorSubdivisions = subdivisions;
    }
}

bool RulerAssistant::hasFixedLength() const {
    return m_hasFixedLength;
}

void RulerAssistant::enableFixedLength(bool enabled) {
    m_hasFixedLength = enabled;
}

qreal RulerAssistant::fixedLength() const {
    return m_fixedLength;
}

void RulerAssistant::setFixedLength(qreal length) {
    if (length < 0.0) {
        m_fixedLength = 0.0;
    } else {
        m_fixedLength = length;
    }
}

PkString RulerAssistant::fixedLengthUnit() const {
    return m_fixedLengthUnit;
}

void RulerAssistant::setFixedLengthUnit(PkString unit) {
    if (unit.isEmpty()) {
        m_fixedLengthUnit = "px";
    } else {
        m_fixedLengthUnit = unit;
    }
}

void RulerAssistant::ensureLength() {
    if (!hasFixedLength() || fixedLength() < 1e-3) {
        return;
    }
    
    PkPointF center = *handles()[0];
    PkPointF handle = *handles()[1];
    PkPointF direction = handle - center;
    qreal distance = sqrt(KisPaintingAssistant::norm2(direction));
    PkPointF delta = direction / distance * fixedLength();
    *handles()[1] = center + delta;
    uncache();
}

void RulerAssistant::saveCustomXml(PkXmlStreamWriter *xml) {
    if (xml) {
        xml->writeStartElement("subdivisions");
        xml->writeAttribute("value", KisDomUtils::toString(subdivisions()));
        xml->writeEndElement();
        xml->writeStartElement("minorSubdivisions");
        xml->writeAttribute("value", KisDomUtils::toString(minorSubdivisions()));
        xml->writeEndElement();
        xml->writeStartElement("fixedLength");
        xml->writeAttribute("value", KisDomUtils::toString(fixedLength()));
        xml->writeAttribute("enabled", KisDomUtils::toString((int)hasFixedLength()));
        xml->writeAttribute("unit", fixedLengthUnit());
        xml->writeEndElement();
    }
}

bool RulerAssistant::loadCustomXml(PkXmlStreamReader *xml) {
    if (xml) {
        if (xml->name() == "subdivisions") {
            setSubdivisions(KisDomUtils::toInt(xml->attributes().value("value")));
        }
        else if (xml->name() == "minorSubdivisions") {
            setMinorSubdivisions(KisDomUtils::toInt(xml->attributes().value("value")));
        }
        else if (xml->name() == "fixedLength") {
            setFixedLength(KisDomUtils::toDouble(xml->attributes().value("value")));
            enableFixedLength(0 != KisDomUtils::toInt(xml->attributes().value("enabled")));
            setFixedLengthUnit(xml->attributes().value("unit"));
        }
    }
    return true;
}



RulerAssistantFactory::RulerAssistantFactory() = default;

RulerAssistantFactory::~RulerAssistantFactory() = default;

PkString RulerAssistantFactory::id() const
{
    return "ruler";
}

PkString RulerAssistantFactory::name() const
{
    return PkString("Ruler");
}

KisPaintingAssistant* RulerAssistantFactory::createPaintingAssistant() const
{
    return new RulerAssistant;
}
