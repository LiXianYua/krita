/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "PerspectiveAssistant.h"


#include <PkTransform.h>

#include <kis_algebra_2d.h>
#include <kis_dom_utils.h>

#include "PerspectiveBasedAssistantHelper.h"

#include <math.h>
#include <limits>

PerspectiveAssistant::PerspectiveAssistant()
    : KisPaintingAssistant("perspective", PkString("Perspective assistant"))
{
}

PerspectiveAssistant::PerspectiveAssistant(const PerspectiveAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
    , m_subdivisions(rhs.m_subdivisions)
    , m_snapLine(rhs.m_snapLine)
    , m_cachedTransform(rhs.m_cachedTransform)
    , m_cachedPolygon(rhs.m_cachedPolygon)
    , m_cacheValid(rhs.m_cacheValid)
    , m_cache(rhs.m_cache)
{
    for (int i = 0; i < 4; ++i) {
        m_cachedPoints[i] = rhs.m_cachedPoints[i];
    }
}

KisPaintingAssistantSP PerspectiveAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new PerspectiveAssistant(*this, handleMap));
}

PkPointF PerspectiveAssistant::project(const PkPointF& pt, const PkPointF& strokeBegin, const bool snapToAnyDirection, qreal moveThresholdPt)
{
    const static PkPointF nullPoint(std::numeric_limits<qreal>::quiet_NaN(), std::numeric_limits<qreal>::quiet_NaN());

    assert(isAssistantComplete());

    if (snapToAnyDirection || m_snapLine.isNull()) {
        PkPolygonF poly;
        PkTransform transform;

        if (!getTransform(poly, transform)) {
            return nullPoint;
        }

        if (!poly.containsPoint(strokeBegin, Qt::OddEvenFill)) {
            return nullPoint; // avoid problems with multiple assistants: only snap if starting in the grid
        }

        if (KisAlgebra2D::norm(pt - strokeBegin) < moveThresholdPt) {
            return strokeBegin; // allow some movement before snapping
        }

        // construct transformation
        bool invertible;
        const PkTransform inverse = transform.inverted(&invertible);
        if (!invertible) {
            return nullPoint; // shouldn't happen
        }


        // figure out which direction to go
        const PkPointF start = inverse.map(strokeBegin);
        const PkLineF verticalLine = PkLineF(strokeBegin, transform.map(start + PkPointF(0, 1)));
        const PkLineF horizontalLine = PkLineF(strokeBegin, transform.map(start + PkPointF(1, 0)));

        // determine whether the horizontal or vertical line is closer to the point
        m_snapLine = KisAlgebra2D::pointToLineDistSquared(pt, verticalLine) < KisAlgebra2D::pointToLineDistSquared(pt, horizontalLine) ? verticalLine : horizontalLine;
    }

    // snap to line
    const qreal
            dx = m_snapLine.dx(),
            dy = m_snapLine.dy(),
            dx2 = dx * dx,
            dy2 = dy * dy,
            invsqrlen = 1.0 / (dx2 + dy2);
    PkPointF r(dx2 * pt.x() + dy2 * m_snapLine.x1() + dx * dy * (pt.y() - m_snapLine.y1()),
              dx2 * m_snapLine.y1() + dy2 * pt.y() + dx * dy * (pt.x() - m_snapLine.x1()));

    r *= invsqrlen;
    return r;
}

PkPointF PerspectiveAssistant::adjustPosition(const PkPointF& pt, const PkPointF& strokeBegin, const bool snapToAny, qreal moveThresholdPt)
{
    return project(pt, strokeBegin, snapToAny, moveThresholdPt);
}

void PerspectiveAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    point = project(point, strokeBegin, true, 0.0);
}

void PerspectiveAssistant::endStroke()
{
    m_snapLine = PkLineF();
    KisPaintingAssistant::endStroke();
}

bool PerspectiveAssistant::contains(const PkPointF& pt) const
{
    PkPolygonF poly;
    if (!PerspectiveBasedAssistantHelper::getTetragon(handles(), isAssistantComplete(), poly)) return false;
    return poly.containsPoint(pt, Qt::OddEvenFill);
}

qreal PerspectiveAssistant::distance(const PkPointF& pt) const
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(m_cacheValid);
    return PerspectiveBasedAssistantHelper::distanceInGrid(m_cache, pt);
}

bool PerspectiveAssistant::isActive() const
{
    return isSnappingActive();
}



PkPointF PerspectiveAssistant::getDefaultEditorPosition() const
{
    PkPointF centroid(0, 0);
    for (int i = 0; i < 4; ++i) {
        centroid += *handles()[i];
    }

    return centroid * 0.25;
}

bool PerspectiveAssistant::getTransform(PkPolygonF& poly, PkTransform& transform) const
{
    if (m_cachedPolygon.size() != 0 && isAssistantComplete()) {
        for (int i = 0; i <= 4; ++i) {
            if (i == 4) {
                poly = m_cachedPolygon;
                transform = m_cachedTransform;
                return m_cacheValid;
            }
            if (m_cachedPoints[i] != *handles()[i]) break;
        }
    }

    m_cachedPolygon.clear();
    m_cacheValid = false;

    if (!PerspectiveBasedAssistantHelper::getTetragon(handles(), isAssistantComplete(), poly)) {
        m_cachedPolygon = poly;
        return false;
    }

    if (!PkTransform::squareToQuad(poly, transform)) {
        qWarning("Failed to create perspective mapping");
        return false;
    }

    for (int i = 0; i < 4; ++i) {
        m_cachedPoints[i] = *handles()[i];
    }

    m_cachedPolygon = poly;
    m_cachedTransform = transform;
    PerspectiveBasedAssistantHelper::updateCacheData(m_cache, poly);
    m_cacheValid = true;
    return true;
}

bool PerspectiveAssistant::isAssistantComplete() const
{
    return handles().size() >= 4; // specify 4 corners to make assistant complete
}

int PerspectiveAssistant::subdivisions() const {
    return m_subdivisions;
}

void PerspectiveAssistant::setSubdivisions(int subdivisions) {
    if (subdivisions < 1) m_subdivisions = 1;
    else m_subdivisions = subdivisions;
}

void PerspectiveAssistant::saveCustomXml(PkXmlStreamWriter *xml) {
    if (xml) {
        xml->writeStartElement("subdivisions");
        xml->writeAttribute("value", KisDomUtils::toString(subdivisions()));
        xml->writeEndElement();
    }
}

bool PerspectiveAssistant::loadCustomXml(PkXmlStreamReader *xml) {
    if (xml && xml->name() == "subdivisions") {
        setSubdivisions(KisDomUtils::toInt(xml->attributes().value("value")));
    }
    return true;
}



PerspectiveAssistantFactory::PerspectiveAssistantFactory()
{
}

PerspectiveAssistantFactory::~PerspectiveAssistantFactory()
{
}

PkString PerspectiveAssistantFactory::id() const
{
    return "perspective";
}

PkString PerspectiveAssistantFactory::name() const
{
    return PkString("Perspective");
}

KisPaintingAssistant* PerspectiveAssistantFactory::createPaintingAssistant() const
{
    return new PerspectiveAssistant;
}
