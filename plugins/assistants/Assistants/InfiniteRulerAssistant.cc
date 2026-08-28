/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2014 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 * SPDX-FileCopyrightText: 2022 Julian Schmidt <julisch1107@web.de>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "InfiniteRulerAssistant.h"


#include <PkTransform.h>

#include <kis_algebra_2d.h>

#include <math.h>

InfiniteRulerAssistant::InfiniteRulerAssistant()
    : RulerAssistant("infinite ruler", PkString("Infinite Ruler assistant"))
{
}

InfiniteRulerAssistant::InfiniteRulerAssistant(const InfiniteRulerAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : RulerAssistant(rhs, handleMap)
{
}

KisPaintingAssistantSP InfiniteRulerAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new InfiniteRulerAssistant(*this, handleMap));
}

PkPointF InfiniteRulerAssistant::project(const PkPointF& pt, const PkPointF& strokeBegin, const bool checkForInitialMovement, qreal moveThresholdPt)
{
    assert(isAssistantComplete());
    //code nicked from the perspective ruler.
    qreal dx = pt.x() - strokeBegin.x();
    qreal dy = pt.y() - strokeBegin.y();
    if (checkForInitialMovement && KisAlgebra2D::norm(PkPointF(dx, dy)) < moveThresholdPt) {
        // allow some movement before snapping
        return strokeBegin;
    }

    PkLineF snapLine = PkLineF(*handles()[0], *handles()[1]);
    
        dx = snapLine.dx();
        dy = snapLine.dy();
    const qreal
        dx2 = dx * dx,
        dy2 = dy * dy,
        invsqrlen = 1.0 / (dx2 + dy2);
    PkPointF r(dx2 * pt.x() + dy2 * snapLine.x1() + dx * dy * (pt.y() - snapLine.y1()),
              dx2 * snapLine.y1() + dy2 * pt.y() + dx * dy * (pt.x() - snapLine.x1()));
    r *= invsqrlen;
    return r;
    //return pt;
}

PkPointF InfiniteRulerAssistant::adjustPosition(const PkPointF& pt, const PkPointF& strokeBegin, const bool /*snapToAny*/, qreal moveThresholdPt)
{
    return project(pt, strokeBegin, true, moveThresholdPt);
}

void InfiniteRulerAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{

    point = project(point, strokeBegin, false, 0.0);
    strokeBegin = project(strokeBegin, strokeBegin, false, 0.0);
}



InfiniteRulerAssistant::ClippingResult InfiniteRulerAssistant::clipLineParametric(PkLineF line, PkRectF rect, bool extendFirst, bool extendSecond) {
    double dx = line.x2() - line.x1();
    double dy = line.y2() - line.y1();
    
    double q1 = line.x1() - rect.x();
    double q2 = rect.x() + rect.width() - line.x1();
    double q3 = line.y1() - rect.y();
    double q4 = rect.y() + rect.height() - line.y1();
    
    PkVector<double> p = PkVector<double>({-dx, dx, -dy, dy});
    PkVector<double> q = PkVector<double>({q1, q2, q3, q4});
    
    double tmin = extendFirst ? -std::numeric_limits<double>::infinity() : 0.0;
    double tmax = extendSecond ? +std::numeric_limits<double>::infinity() : 1.0;
    
    for (int i = 0; i < p.length(); i++) {
        
        if (p[i] == 0 && q[i] < 0) {
            // Line is parallel to this boundary and outside of it
            return ClippingResult{false, 0, 0};
            
        } else if (p[i] < 0) {
            // Line moves into this boundary with increasing t
            // Set minimum t where it just comes in
            double t = q[i] / p[i];
            if (t > tmin) {
                tmin = t;
            }
          
        } else if (p[i] > 0) {
            // Line moves out of this boundary with increasing t
            // Set maximum t where it is still inside
            double t = q[i] / p[i];
            if (t < tmax) {
                tmax = t;
            }
        }
    }
    
    // The line intersects the rectangle if tmin < tmax.
    return ClippingResult{tmin < tmax, tmin, tmax};
}

PkPointF InfiniteRulerAssistant::getDefaultEditorPosition() const
{
    return (*handles()[0]);
}

bool InfiniteRulerAssistant::isAssistantComplete() const
{
    return handles().size() >= 2;
}

InfiniteRulerAssistantFactory::InfiniteRulerAssistantFactory() = default;

InfiniteRulerAssistantFactory::~InfiniteRulerAssistantFactory() = default;

PkString InfiniteRulerAssistantFactory::id() const
{
    return "infinite ruler";
}

PkString InfiniteRulerAssistantFactory::name() const
{
    return PkString("Infinite Ruler");
}

KisPaintingAssistant* InfiniteRulerAssistantFactory::createPaintingAssistant() const
{
    return new InfiniteRulerAssistant;
}
