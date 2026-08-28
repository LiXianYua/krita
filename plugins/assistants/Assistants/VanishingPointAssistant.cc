/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "VanishingPointAssistant.h"


#include <PkTransform.h>

#include <kis_algebra_2d.h>
#include <kis_dom_utils.h>
#include <math.h>

VanishingPointAssistant::VanishingPointAssistant()
    : KisPaintingAssistant("vanishing point", PkString("Vanishing Point assistant"))
{
}

VanishingPointAssistant::VanishingPointAssistant(const VanishingPointAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
    , m_referenceLineDensity(rhs.m_referenceLineDensity)
{
}

KisPaintingAssistantSP VanishingPointAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new VanishingPointAssistant(*this, handleMap));
}

PkPointF VanishingPointAssistant::project(const PkPointF& pt, const PkPointF& strokeBegin, qreal /*moveThresholdPt*/)
{
    //assert(handles().size() == 1 || handles().size() == 5);

    if (isLocal() && isAssistantComplete()) {
        if (getLocalRect().contains(pt)) {
            m_hasBeenInsideLocalRect = true;
        } else if (!m_hasBeenInsideLocalRect) { // isn't inside and wasn't inside before
            return PkPointF(qQNaN(), qQNaN());
        }
    }

    //dbgKrita<<strokeBegin<< ", " <<*handles()[0];
    PkLineF snapLine = PkLineF(*handles()[0], strokeBegin);


    qreal dx = snapLine.dx();
    qreal dy = snapLine.dy();

    const qreal dx2 = dx * dx;
    const qreal dy2 = dy * dy;
    const qreal invsqrlen = 1.0 / (dx2 + dy2);

    PkPointF r(dx2 * pt.x() + dy2 * snapLine.x1() + dx * dy * (pt.y() - snapLine.y1()),
              dx2 * snapLine.y1() + dy2 * pt.y() + dx * dy * (pt.x() - snapLine.x1()));

    r *= invsqrlen;
    return r;
}

PkPointF VanishingPointAssistant::adjustPosition(const PkPointF& pt, const PkPointF& strokeBegin, const bool /*snapToAny*/, qreal moveThresholdPt)
{
    return project(pt, strokeBegin, moveThresholdPt);
}

void VanishingPointAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    point = project(point, strokeBegin, 0.0);
}



KisPaintingAssistantHandleSP VanishingPointAssistant::firstLocalHandle() const
{
    if (handles().size() > LocalFirstHandle) {
        return handles().at(LocalFirstHandle);
    } else {
        return nullptr;
    }
}

KisPaintingAssistantHandleSP VanishingPointAssistant::secondLocalHandle() const
{
    if (handles().size() > LocalSecondHandle) {
        return handles().at(LocalSecondHandle);
    } else {
        return nullptr;
    }
}

PkPointF VanishingPointAssistant::getDefaultEditorPosition() const
{
    int pointHandle = 0;
    if (handles().size() > pointHandle) {
        return *handles().at(pointHandle);
    } else {
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(false, PkPointF(0, 0));
        return PkPointF(0, 0);
    }
}

void VanishingPointAssistant::setReferenceLineDensity(float value)
{
    // cannot have less than 1 degree value
    if (value < 1.0) {
        value = 1.0;
    }

    m_referenceLineDensity = value;
}

float VanishingPointAssistant::referenceLineDensity()
{
    return m_referenceLineDensity;
}

bool VanishingPointAssistant::isAssistantComplete() const
{
    return handles().size() >= numHandles();
}

bool VanishingPointAssistant::canBeLocal() const
{
    return true;
}

void VanishingPointAssistant::saveCustomXml(PkXmlStreamWriter* xml)
{
    xml->writeStartElement("angleDensity");
    xml->writeAttribute("value", KisDomUtils::toString( this->referenceLineDensity()));
    xml->writeEndElement();
    xml->writeStartElement("isLocal");
    xml->writeAttribute("value", KisDomUtils::toString( (int)this->isLocal()));
    xml->writeEndElement();
}

bool VanishingPointAssistant::loadCustomXml(PkXmlStreamReader* xml)
{
    if (xml && xml->name() == "angleDensity") {
        this->setReferenceLineDensity((float)KisDomUtils::toDouble(xml->attributes().value("value")));
    }
    if (xml && xml->name() == "isLocal") {
        this->setLocal((bool)KisDomUtils::toInt(xml->attributes().value("value")));
    }

    return true;
}


VanishingPointAssistantFactory::VanishingPointAssistantFactory()
{
}

VanishingPointAssistantFactory::~VanishingPointAssistantFactory()
{
}

PkString VanishingPointAssistantFactory::id() const
{
    return "vanishing point";
}

PkString VanishingPointAssistantFactory::name() const
{
    return PkString("Vanishing Point");
}

KisPaintingAssistant* VanishingPointAssistantFactory::createPaintingAssistant() const
{
    return new VanishingPointAssistant;
}
