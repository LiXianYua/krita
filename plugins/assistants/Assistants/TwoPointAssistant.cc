/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2021 Nabil Maghfur Usman <nmaghfurusman@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "TwoPointAssistant.h"

#include <PkTransform.h>

#include <kis_algebra_2d.h>
#include <kis_dom_utils.h>
#include <cmath>
#include <kis_assert.h>

TwoPointAssistant::TwoPointAssistant()
    : KisPaintingAssistant("two point", PkString("Two point assistant"))
{
}

TwoPointAssistant::TwoPointAssistant(const TwoPointAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
    , m_snapLine(rhs.m_snapLine)
    , m_gridDensity(rhs.m_gridDensity)
    , m_useVertical(rhs.m_useVertical)
    , m_lastUsedPoint(rhs.m_lastUsedPoint)
{
}

KisPaintingAssistantSP TwoPointAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new TwoPointAssistant(*this, handleMap));
}

PkPointF TwoPointAssistant::project(const PkPointF& point, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThreshold)
{
    assert(isAssistantComplete());

    PkPointF best_pt = point;
    double best_dist = DBL_MAX;
    PkList<int> possibleHandles;

    // must be above or equal to 0;
    // if useVertical, then last used point must be below 3, because 2 means vertical
    //     and it's the last possible point here (sanity check)
    // if !useVertical, then it must be below 2, because 2 means vertical
    bool isLastUsedPointCorrectNow = m_lastUsedPoint >= 0 && (m_useVertical ? m_lastUsedPoint < 3 : m_lastUsedPoint < 2);

    if (isLocal() && handles().size() == 5) {
        // here we can just return since we don't want to do anything
        // so we're returning a NaN
        // but only if we don't have a point/axes it was already using

        PkRectF rect = getLocalRect();
        bool insideLocalRect = rect.contains(point);
        if (!insideLocalRect && (!isLastUsedPointCorrectNow || !m_hasBeenInsideLocalRect)) {
            return PkPointF(qQNaN(), qQNaN());
        } else if (insideLocalRect) {
            m_hasBeenInsideLocalRect = true;
        }
    }

    if (!isLastUsedPointCorrectNow && KisAlgebra2D::norm(point - strokeBegin) < moveThreshold) {
        return strokeBegin;
    }

    if (!snapToAny && isLastUsedPointCorrectNow) {
        possibleHandles = PkList<int>({m_lastUsedPoint});
    } else {
        if (m_useVertical) {
            possibleHandles = PkList<int>({0, 1, 2});
        } else {
            possibleHandles = PkList<int>({0, 1});
        }
    }

    for (int vpIndex : possibleHandles) {
        PkPointF vp = *handles()[vpIndex];
        double dist = 0;
        PkPointF pt = PkPointF();
        PkLineF snapLine = PkLineF();

        // TODO: Would be a good idea to generalize this whole routine
        // in KisAlgebra2d, as it's all lifted from the vanishing
        // point assistant and parallel ruler assistant, and by
        // extension the perspective assistant...
        qreal dx = point.x() - strokeBegin.x();
        qreal dy = point.y() - strokeBegin.y();

        if (vp != *handles()[2]) {
            snapLine = PkLineF(vp, strokeBegin);
        } else {
            PkLineF vertical = PkLineF(*handles()[0],*handles()[1]).normalVector();
            snapLine = PkLineF(vertical.p1(), vertical.p2());
            PkPointF translation = (vertical.p1()-strokeBegin)*-1.0;
            snapLine = snapLine.translated(translation);
        }

        dx = snapLine.dx();
        dy = snapLine.dy();

        const qreal dx2 = dx * dx;
        const qreal dy2 = dy * dy;
        const qreal invsqrlen = 1.0 / (dx2 + dy2);

        pt = PkPointF(dx2 * point.x() + dy2 * snapLine.x1() + dx * dy * (point.y() - snapLine.y1()),
                     dx2 * snapLine.y1() + dy2 * point.y() + dx * dy * (point.x() - snapLine.x1()));

        pt *= invsqrlen;
        dist = std::abs(pt.x() - point.x()) + std::abs(pt.y() - point.y());

        if (dist < best_dist) {
            best_pt = pt;
            best_dist = dist;
            m_lastUsedPoint = vpIndex;
        }
    }

    return best_pt;
}

void TwoPointAssistant::endStroke()
{
    m_snapLine = PkLineF();
    m_lastUsedPoint = -1;
    KisPaintingAssistant::endStroke();
}

PkPointF TwoPointAssistant::adjustPosition(const PkPointF& pt, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt)
{
    return project(pt, strokeBegin, snapToAny, moveThresholdPt);
}

void TwoPointAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    PkPointF p = project(point, strokeBegin, true, 0.0);
    point = p;
}



KisPaintingAssistantHandleSP TwoPointAssistant::firstLocalHandle() const
{
    if (handles().size() > LocalFirstHandle) {
        return handles().at(LocalFirstHandle);
    } else {
        return nullptr;
    }
}

KisPaintingAssistantHandleSP TwoPointAssistant::secondLocalHandle() const
{
    if (handles().size() > LocalSecondHandle) {
        return handles().at(LocalSecondHandle);
    } else {
        return nullptr;
    }
}

PkPointF TwoPointAssistant::getDefaultEditorPosition() const
{
    int centerOfVisionHandle = 2;
    if (handles().size() > centerOfVisionHandle) {
        return *handles().at(centerOfVisionHandle);
    } else if (handles().size() > 0) {
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(false, *handles().at(0));
        return *handles().at(0);
    } else {
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(false, PkPointF(0, 0));
        return PkPointF(0, 0);
    }
}

void TwoPointAssistant::setGridDensity(double density)
{
    m_gridDensity = density;
}

bool TwoPointAssistant::useVertical()
{
    return m_useVertical;
}

void TwoPointAssistant::setUseVertical(bool value)
{
    m_useVertical = value;
}

double TwoPointAssistant::gridDensity()
{
    return m_gridDensity;
}

PkTransform TwoPointAssistant::localTransform(PkPointF vp_a, PkPointF vp_b, PkPointF pt_c, qreal* size)
{
    PkTransform t = PkTransform();
    t.rotate(PkLineF(vp_a, vp_b).angle());
    t.translate(-pt_c.x(),-pt_c.y());
    const PkLineF horizon = PkLineF(t.map(vp_a), PkPointF(t.map(vp_b).x(),t.map(vp_a).y()));
    *size = sqrt(pow(horizon.length()/2.0,2) - pow(abs(horizon.center().x()),2));

    return t;
}

bool TwoPointAssistant::isAssistantComplete() const
{
    return handles().size() >= numHandles();
}

bool TwoPointAssistant::canBeLocal() const
{
    return true;
}

void TwoPointAssistant::saveCustomXml(PkXmlStreamWriter* xml)
{
    xml->writeStartElement("gridDensity");
    xml->writeAttribute("value", KisDomUtils::toString( this->gridDensity()));
    xml->writeEndElement();
    xml->writeStartElement("useVertical");
    xml->writeAttribute("value", KisDomUtils::toString( (int)this->useVertical()));
    xml->writeEndElement();
    xml->writeStartElement("isLocal");
    xml->writeAttribute("value", KisDomUtils::toString( (int)this->isLocal()));
    xml->writeEndElement();

}

bool TwoPointAssistant::loadCustomXml(PkXmlStreamReader* xml)
{
    if (xml && xml->name() == "gridDensity") {
        this->setGridDensity((float)KisDomUtils::toDouble(xml->attributes().value("value")));
    }
    if (xml && xml->name() == "useVertical") {
        this->setUseVertical((bool)KisDomUtils::toInt(xml->attributes().value("value")));
    }
    if (xml && xml->name() == "isLocal") {
        this->setLocal((bool)KisDomUtils::toInt(xml->attributes().value("value")));
    }
    return true;
}

TwoPointAssistantFactory::TwoPointAssistantFactory()
{
}

TwoPointAssistantFactory::~TwoPointAssistantFactory()
{
}

PkString TwoPointAssistantFactory::id() const
{
    return "two point";
}

PkString TwoPointAssistantFactory::name() const
{
    return PkString("2 Point Perspective");
}

KisPaintingAssistant* TwoPointAssistantFactory::createPaintingAssistant() const
{
    return new TwoPointAssistant;
}
