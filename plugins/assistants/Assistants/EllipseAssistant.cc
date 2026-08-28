/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "EllipseAssistant.h"

#include <PkTransform.h>

#include "kis_algebra_2d.h"

#include <math.h>

EllipseAssistant::EllipseAssistant()
        : KisPaintingAssistant("ellipse", PkString("Ellipse assistant"))
{
}

EllipseAssistant::EllipseAssistant(const EllipseAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
    , e(rhs.e)
{
}

KisPaintingAssistantSP EllipseAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new EllipseAssistant(*this, handleMap));
}

PkPointF EllipseAssistant::project(const PkPointF& pt) const
{
    assert(isAssistantComplete());
    e.set(*handles()[0], *handles()[1], *handles()[2]);
    return e.project(pt);
}

PkPointF EllipseAssistant::adjustPosition(const PkPointF& pt, const PkPointF& /*strokeBegin*/, const bool /*snapToAny*/, qreal /*moveThresholdPt*/)
{
    return project(pt);

}

void EllipseAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    const PkPointF p1 = point;
    const PkPointF p2 = strokeBegin;

    assert(isAssistantComplete());
    e.set(*handles()[0], *handles()[1], *handles()[2]);

    PkPointF p3 = e.project(p1);
    PkPointF p4 = e.project(p2);
    point = p3;
    strokeBegin = p4;
}




PkRect EllipseAssistant::boundingRect() const
{
    if (!isAssistantComplete()) {
        return KisPaintingAssistant::boundingRect();
    }

    if (e.set(*handles()[0], *handles()[1], *handles()[2])) {
        return e.boundingRect().adjusted(-2, -2, 2, 2).toAlignedRect();
    } else {
        return PkRect();
    }
}

PkPointF EllipseAssistant::getDefaultEditorPosition() const
{
    return (*handles()[0] + *handles()[1]) * 0.5;
}

bool EllipseAssistant::isAssistantComplete() const
{
    return handles().size() >= 3;
}

void EllipseAssistant::transform(const PkTransform &transform)
{
    e.set(*handles()[0], *handles()[1], *handles()[2]);

    PkPointF newAxes;
    PkTransform newTransform;

    std::tie(newAxes, newTransform) = KisAlgebra2D::transformEllipse(PkPointF(e.semiMajor(), e.semiMinor()), e.getInverse() * transform);

    const PkPointF p1 = newTransform.map(PkPointF(newAxes.x(), 0));
    const PkPointF p2 = newTransform.map(PkPointF(-newAxes.x(), 0));
    const PkPointF p3 = newTransform.map(PkPointF(0, newAxes.y()));

    *handles()[0] = p1;
    *handles()[1] = p2;
    *handles()[2] = p3;

    uncache();
}

EllipseAssistantFactory::EllipseAssistantFactory()
{
}

EllipseAssistantFactory::~EllipseAssistantFactory()
{
}

PkString EllipseAssistantFactory::id() const
{
    return "ellipse";
}

PkString EllipseAssistantFactory::name() const
{
    return PkString("Ellipse");
}

KisPaintingAssistant* EllipseAssistantFactory::createPaintingAssistant() const
{
    return new EllipseAssistant;
}
