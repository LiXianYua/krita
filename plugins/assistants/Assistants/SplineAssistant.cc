/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "SplineAssistant.h"


#include <PkTransform.h>

#include "KisBezierUtils.h"

#include <cmath>
#include <limits>
#include <algorithm>

struct GoldenSearchParams
{
    GoldenSearchParams(qreal lbound
                ,qreal ubound)
        :lbound(lbound)
        ,ubound(ubound)
    {
        samples = PkVector<GoldenSearchPoint>(4);
    }

    struct GoldenSearchPoint {
        GoldenSearchPoint(qreal xval)
            : x(xval)
        {
        }

        GoldenSearchPoint(){}

        void inv_norm(qreal l, qreal u)
        {
            this->xnorm = this->x * (u - l) + l;
        }

        qreal fval;
        qreal xnorm;
        qreal x;
    };


    qreal lbound;
    qreal ubound;
    PkVector<GoldenSearchPoint> samples;
};


struct SplineAssistant::Private {
    Private();

    PkPointF prevStrokebegin;
    qreal prev_t {0};
};

SplineAssistant::Private::Private()
    : prevStrokebegin(0,0)
{
}

SplineAssistant::SplineAssistant()
    : KisPaintingAssistant("spline", PkString("Spline assistant"))
    , m_d(new Private)
{
}

SplineAssistant::~SplineAssistant() = default;

SplineAssistant::SplineAssistant(const SplineAssistant &rhs, PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap)
    : KisPaintingAssistant(rhs, handleMap)
    , m_d(new Private)
{
}

KisPaintingAssistantSP SplineAssistant::clone(PkMap<KisPaintingAssistantHandleSP, KisPaintingAssistantHandleSP> &handleMap) const
{
    return KisPaintingAssistantSP(new SplineAssistant(*this, handleMap));
}

// parametric form of a cubic spline (B(t) = (1-t)^3 P0 + 3 (1-t)^2 t P1 + 3 (1-t) t^2 P2 + t^3 P3)
inline PkPointF B(qreal t, const PkPointF& P0, const PkPointF& P1, const PkPointF& P2, const PkPointF& P3)
{
    const qreal tp = 1 - t;
    const qreal tp2 = tp * tp;
    const qreal t2 = t * t;

    return  (    tp2 * tp) * P0 +
            (3 * tp2 * t ) * P1 +
            (3 * tp  * t2) * P2 +
            (    t   * t2) * P3;
}
// squared distance from a point on the spline to given point: we want to minimize this
inline qreal D(qreal t, const PkPointF& P0, const PkPointF& P1, const PkPointF& P2, const PkPointF& P3, const PkPointF& p)
{
    const qreal
            tp =  1 - t,
            tp2 = tp * tp,
            t2 =  t * t,
            a =   tp2 * tp,
            b =   3 * tp2 * t,
            c =   3 * tp  * t2,
            d =   t   * t2,
            x_dist = a*P0.x() + b*P1.x() + c*P2.x() + d*P3.x() - p.x(),
            y_dist = a*P0.y() + b*P1.y() + c*P2.y() + d*P3.y() - p.y();

    return x_dist * x_dist + y_dist * y_dist;
}


inline qreal goldenSearch(const PkPointF& pt
                          , const PkList<KisPaintingAssistantHandleSP> handles
                          , qreal low
                          , qreal high
                          , qreal tolerance
                          , uint max_iter)
{
    GoldenSearchParams ovalues = GoldenSearchParams(low,high);
    PkVector<GoldenSearchParams::GoldenSearchPoint> p = ovalues.samples;
    qreal u = ovalues.ubound;
    qreal l = ovalues.lbound;

    const qreal ratio = 1 - 2/(1 + sqrt(5));
    p[0].x = 0;
    p[1].x = ratio;
    p[2].x = 1 - p[1].x;
    p[3].x = 1;

    p[1].inv_norm(l,u);
    p[2].inv_norm(l,u);

    p[1].fval = D(p[1].xnorm, *handles[0], *handles[2], *handles[3], *handles[1], pt);
    p[2].fval = D(p[2].xnorm, *handles[0], *handles[2], *handles[3], *handles[1], pt);

    GoldenSearchParams::GoldenSearchPoint xtemp = GoldenSearchParams::GoldenSearchPoint(0);

    uint i = 0; // used to force early exit
    while (std::abs(p[2].xnorm - p[1].xnorm) > tolerance && i < max_iter) {

        if (p[1].fval < p[2].fval) {
            xtemp = p[1];
            p[3] = p[2];
            p[1].x = p[0].x + (p[2].x - p[1].x);
            p[2] = xtemp;

            p[1].inv_norm(l,u);
            p[1].fval = D(p[1].xnorm, *handles[0], *handles[2], *handles[3], *handles[1], pt);

        } else {
            xtemp = p[2];
            p[0] = p[1];
            p[2].x = p[1].x + (p[3].x - p[2].x);
            p[1] = xtemp;

            p[2].inv_norm(l,u);
            p[2].fval = D(p[2].xnorm, *handles[0], *handles[2], *handles[3], *handles[1], pt);

        }
        i++;
    }
    return (p[2].xnorm + p[1].xnorm) / 2;
}


PkPointF SplineAssistant::project(const PkPointF& pt, const PkPointF& strokeBegin) const
{
    assert(isAssistantComplete());

    // minimize d(t), but keep t in the same neighbourhood as before (unless starting a new stroke)
    bool stayClose = (m_d->prevStrokebegin == strokeBegin)? true : false;
    qreal min_t;

    PkList<PkPointF> refs;
    PkVector<int> hindex = {0,2,3,1}; // order handles as expected by KisBezierUtils
    for (int i : hindex) {
        refs.append(*handles()[i]);
    }

    if (stayClose){
        // Search in the vicinity of previous t value.
        // This ensure unimodality for proper goldenSearch algorithm
        qreal delta = 1/10.0;
        qreal lbound = qBound(0.0,1.0, m_d->prev_t - delta);
        qreal ubound = qBound(0.0,1.0, m_d->prev_t + delta);
        min_t = goldenSearch(pt,handles(), lbound , ubound, 1e-6,1e+2);

    } else {
        min_t = KisBezierUtils::nearestPoint(refs,pt);
    }

    PkPointF draw_pos = B(min_t, *handles()[0], *handles()[2], *handles()[3], *handles()[1]);

    m_d->prev_t = min_t;
    m_d->prevStrokebegin = strokeBegin;

    return draw_pos;
}

PkPointF SplineAssistant::adjustPosition(const PkPointF& pt, const PkPointF& strokeBegin, const bool /*snapToAny*/, qreal /*moveThresholdPt*/)
{
    return project(pt, strokeBegin);
}

void SplineAssistant::adjustLine(PkPointF &point, PkPointF &strokeBegin)
{
    point = PkPointF();
    strokeBegin = PkPointF();
}



PkPointF SplineAssistant::getDefaultEditorPosition() const
{
    return B(0.5, *handles()[0], *handles()[2], *handles()[3], *handles()[1]);
}

bool SplineAssistant::isAssistantComplete() const
{
    return handles().size() >= 4; // specify 4 corners to make assistant complete
}

SplineAssistantFactory::SplineAssistantFactory()
{
}

SplineAssistantFactory::~SplineAssistantFactory()
{
}

PkString SplineAssistantFactory::id() const
{
    return "spline";
}

PkString SplineAssistantFactory::name() const
{
    return PkString("Spline");
}

KisPaintingAssistant* SplineAssistantFactory::createPaintingAssistant() const
{
    return new SplineAssistant;
}
