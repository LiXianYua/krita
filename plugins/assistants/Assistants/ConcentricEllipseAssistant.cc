/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ConcentricEllipseAssistant.h"
#include "ConcentricEllipseAssistantGeometry.h"

#include <PkTransform.h>
#include <kis_algebra_2d.h>

#include <math.h>

ConcentricEllipseAssistant::ConcentricEllipseAssistant()
    : KisPaintingAssistant("concentric ellipse", PkString("Concentric Ellipse assistant"))
{
}

KisPaintingAssistantSP ConcentricEllipseAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new ConcentricEllipseAssistant(*this, handleMap));
}

ConcentricEllipseAssistant::ConcentricEllipseAssistant(const ConcentricEllipseAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
    , m_ellipse(rhs.m_ellipse)
{
}

PkPointF ConcentricEllipseAssistant::project(const PkPointF& pt, const PkPointF& strokeBegin) const
{
    assert(isAssistantComplete());
    return ConcentricEllipseAssistantGeometry::project(
        {*handles()[0], *handles()[1], *handles()[2]}, pt, strokeBegin);
}

PkPointF ConcentricEllipseAssistant::adjustPosition(const PkPointF& pt, const PkPointF& strokeBegin, const bool /*snapToAny*/, qreal /*moveThresholdPt*/)
{
    return project(pt, strokeBegin);
}

void ConcentricEllipseAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    point = project(point, strokeBegin);
}




PkRect ConcentricEllipseAssistant::boundingRect() const
{
    if (!isAssistantComplete()) {
        return KisPaintingAssistant::boundingRect();
    }

    if (m_ellipse.set(*handles()[0], *handles()[1], *handles()[2])) {
        return m_ellipse.boundingRect().adjusted(-2, -2, 2, 2).toAlignedRect();
    } else {
        return PkRect();
    }
}

PkPointF ConcentricEllipseAssistant::getDefaultEditorPosition() const
{
    return (*handles()[0] + *handles()[1]) * 0.5;
}

bool ConcentricEllipseAssistant::isAssistantComplete() const
{
    return handles().size() >= 3;
}

void ConcentricEllipseAssistant::transform(const PkTransform &transform)
{
    m_ellipse.set(*handles()[0], *handles()[1], *handles()[2]);

    PkPointF newAxes;
    PkTransform newTransform;

    std::tie(newAxes, newTransform) = KisAlgebra2D::transformEllipse(PkPointF(m_ellipse.semiMajor(), m_ellipse.semiMinor()), m_ellipse.getInverse() * transform);

    const PkPointF p1 = newTransform.map(PkPointF(newAxes.x(), 0));
    const PkPointF p2 = newTransform.map(PkPointF(-newAxes.x(), 0));
    const PkPointF p3 = newTransform.map(PkPointF(0, newAxes.y()));

    *handles()[0] = p1;
    *handles()[1] = p2;
    *handles()[2] = p3;

    uncache();
}

ConcentricEllipseAssistantFactory::ConcentricEllipseAssistantFactory()
{
}

ConcentricEllipseAssistantFactory::~ConcentricEllipseAssistantFactory()
{
}

PkString ConcentricEllipseAssistantFactory::id() const
{
    return "concentric ellipse";
}

PkString ConcentricEllipseAssistantFactory::name() const
{
    return PkString("Concentric Ellipse");
}

KisPaintingAssistant* ConcentricEllipseAssistantFactory::createPaintingAssistant() const
{
    return new ConcentricEllipseAssistant;
}
