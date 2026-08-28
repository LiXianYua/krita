/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ParallelRulerAssistant.h"


#include <PkTransform.h>

#include <kis_algebra_2d.h>
#include <kis_dom_utils.h>

#include <math.h>

ParallelRulerAssistant::ParallelRulerAssistant()
    : KisPaintingAssistant("parallel ruler", PkString("Parallel Ruler assistant"))
{
}

KisPaintingAssistantSP ParallelRulerAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new ParallelRulerAssistant(*this, handleMap));
}

ParallelRulerAssistant::ParallelRulerAssistant(const ParallelRulerAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
{
}

PkPointF ParallelRulerAssistant::project(const PkPointF& pt, const PkPointF& strokeBegin, qreal /*moveThresholdPt*/)
{
    assert(isAssistantComplete());

    if (isLocal() && isAssistantComplete()) {
        if (getLocalRect().contains(pt)) {
            m_hasBeenInsideLocalRect = true;
        } else if (isLocal() && !m_hasBeenInsideLocalRect) {
            return PkPointF(qQNaN(), qQNaN());
        }
    }

    //dbgKrita<<strokeBegin<< ", " <<*handles()[0];
    PkLineF snapLine = PkLineF(*handles()[0], *handles()[1]);
    PkPointF translation = (*handles()[0]-strokeBegin)*-1.0;
    snapLine = snapLine.translated(translation);

    qreal dx = snapLine.dx();
    qreal dy = snapLine.dy();

    const qreal
            dx2 = dx * dx,
            dy2 = dy * dy,
            invsqrlen = 1.0 / (dx2 + dy2);
    PkPointF r(dx2 * pt.x() + dy2 * snapLine.x1() + dx * dy * (pt.y() - snapLine.y1()),
              dx2 * snapLine.y1() + dy2 * pt.y() + dx * dy * (pt.x() - snapLine.x1()));
    r *= invsqrlen;
    return r;
}

PkPointF ParallelRulerAssistant::adjustPosition(const PkPointF& pt, const PkPointF& strokeBegin, const bool /*snapToAny*/, qreal moveThresholdPt)
{
    return project(pt, strokeBegin, moveThresholdPt);
}

void ParallelRulerAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    point = project(point, strokeBegin, 0.0);
}



KisPaintingAssistantHandleSP ParallelRulerAssistant::firstLocalHandle() const
{
    return handles().size() > 2 ? handles()[2] : 0;
}

KisPaintingAssistantHandleSP ParallelRulerAssistant::secondLocalHandle() const
{
    return handles().size() > 3 ? handles()[3] : 0;
}

PkPointF ParallelRulerAssistant::getDefaultEditorPosition() const
{
    if (handles().size() > 1) {
        return (*handles()[0] + *handles()[1]) * 0.5;
    } else if (handles().size() > 0) {
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(false, *handles()[0]);
        return *handles()[0];
    } else {
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(false, PkPointF(0, 0));
        return PkPointF(0, 0);
    }
}

bool ParallelRulerAssistant::isAssistantComplete() const
{
    return handles().size() >= numHandles();
}

bool ParallelRulerAssistant::canBeLocal() const
{
    return true;
}

void ParallelRulerAssistant::saveCustomXml(PkXmlStreamWriter *xml)
{
    xml->writeStartElement("isLocal");
    xml->writeAttribute("value", KisDomUtils::toString( (int)this->isLocal()));
    xml->writeEndElement();
}

bool ParallelRulerAssistant::loadCustomXml(PkXmlStreamReader *xml)
{
    if (xml && xml->name() == "isLocal") {
        this->setLocal((bool)KisDomUtils::toInt(xml->attributes().value("value")));
    }
    return true;
}

ParallelRulerAssistantFactory::ParallelRulerAssistantFactory()
{
}

ParallelRulerAssistantFactory::~ParallelRulerAssistantFactory()
{
}

PkString ParallelRulerAssistantFactory::id() const
{
    return "parallel ruler";
}

PkString ParallelRulerAssistantFactory::name() const
{
    return PkString("Parallel Ruler");
}

KisPaintingAssistant* ParallelRulerAssistantFactory::createPaintingAssistant() const
{
    return new ParallelRulerAssistant;
}
