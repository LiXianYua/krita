/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_safe_transform.h"

#include <PkTransform.h>
#include <PkLine.h>
#include <PkPolygon.h>


#include "kis_debug.h"
#include "kis_algebra_2d.h"

namespace {

// PkPolygonF 明确不实现 intersected（登记在案的缺口，见 pk/geometry/PkPolygon.h），
// S 线壳本地补最小实现：Sutherland–Hodgman 凸多边形裁剪。只服务本文件两个调用点，
// 其 clip 均为凸形（矩形 / 半平面裁剪结果），subject 可为任意多边形。
PkPointF pkLineSegIntersect(const PkPointF &p1, const PkPointF &p2,
                            const PkPointF &a, const PkPointF &b)
{
    const PkPointF d1 = p2 - p1;
    const PkPointF d2 = b - a;
    const qreal denom = d1.x() * d2.y() - d1.y() * d2.x();
    if (qAbs(denom) < 1e-12) return p1; // 平行/退化：调用场景下不会发生
    const PkPointF diff = a - p1;
    const qreal t = (diff.x() * d2.y() - diff.y() * d2.x()) / denom;
    return PkPointF(p1.x() + t * d1.x(), p1.y() + t * d1.y());
}

PkPolygonF pkPolygonIntersect(const PkPolygonF &subject, const PkPolygonF &clip)
{
    if (subject.isEmpty() || clip.size() < 3) {
        return PkPolygonF();
    }

    // clip 质心（凸多边形内部点）：无绕向依赖的内侧判定。
    PkPointF centroid;
    for (int i = 0; i < clip.size(); ++i) {
        centroid += clip[i];
    }
    centroid *= (1.0 / clip.size());

    PkPolygonF input = subject;
    const int clipN = clip.size();
    for (int i = 0; i < clipN; ++i) {
        if (input.isEmpty()) break;
        const PkPointF &edgeStart = clip[i];
        const PkPointF &edgeEnd = clip[(i + 1) % clipN];
        const PkPointF edgeVec = edgeEnd - edgeStart;

        const qreal centroidSide = edgeVec.x() * (centroid.y() - edgeStart.y())
            - edgeVec.y() * (centroid.x() - edgeStart.x());
        const bool insideIsNonNegative = centroidSide >= 0.0;

        PkPolygonF output;
        const int inN = input.size();
        if (inN == 0) break;
        PkPointF prev = input[inN - 1];
        qreal prevSide = edgeVec.x() * (prev.y() - edgeStart.y())
            - edgeVec.y() * (prev.x() - edgeStart.x());
        bool prevInside = insideIsNonNegative ? (prevSide >= 0.0) : (prevSide <= 0.0);

        for (int j = 0; j < inN; ++j) {
            const PkPointF &cur = input[j];
            const qreal curSide = edgeVec.x() * (cur.y() - edgeStart.y())
                - edgeVec.y() * (cur.x() - edgeStart.x());
            const bool curInside = insideIsNonNegative ? (curSide >= 0.0) : (curSide <= 0.0);

            if (curInside) {
                if (!prevInside) {
                    output << pkLineSegIntersect(prev, cur, edgeStart, edgeEnd);
                }
                output << cur;
            } else if (prevInside) {
                output << pkLineSegIntersect(prev, cur, edgeStart, edgeEnd);
            }
            prev = cur;
            prevSide = curSide;
            prevInside = curInside;
        }
        input = output;
    }
    return input;
}

} // namespace



struct KisSafeTransform::Private
{
    bool needsClipping = true;

    PkRect bounds;
    PkTransform forwardTransform;
    PkTransform backwardTransform;

    PkPolygonF srcClipPolygon;
    PkPolygonF dstClipPolygon;

    bool getHorizon(const PkTransform &t, PkLineF *horizon) {
        static const qreal eps = 1e-10;

        PkPointF vanishingX(t.m11() / t.m13(), t.m12() / t.m13());
        PkPointF vanishingY(t.m21() / t.m23(), t.m22() / t.m23());

        if (qAbs(t.m13()) < eps && qAbs(t.m23()) < eps) {
            *horizon = PkLineF();
            return false;
        } else if (qAbs(t.m23()) < eps) {
            PkPointF diff = t.map(PkPointF(0.0, 10.0)) - t.map(PkPointF());
            vanishingY = vanishingX + diff;
        } else if (qAbs(t.m13()) < eps) {
            PkPointF diff = t.map(PkPointF(10.0, 0.0)) - t.map(PkPointF());
            vanishingX = vanishingY + diff;
        }

        *horizon = PkLineF(vanishingX, vanishingY);
        return true;
    }

    qreal getCrossSign(const PkLineF &horizon, const PkRectF &rc) {
        if (rc.isEmpty()) return 1.0;

        PkPointF diff = horizon.p2() - horizon.p1();
        return KisAlgebra2D::signPZ(KisAlgebra2D::crossProduct(diff, rc.center() - horizon.p1()));
    }

    PkPolygonF getCroppedPolygon(const PkLineF &baseHorizon, const PkRect &rc, const qreal crossCoeff) {
        if (rc.isEmpty()) return PkPolygonF();

        PkRectF boundsRect(rc);
        PkPolygonF polygon(boundsRect);
        PkPolygonF result;

        // calculate new (offset) horizon to avoid infinity
        const qreal offsetLength = 10.0;
        const PkPointF horizonOffset = offsetLength * crossCoeff *
            KisAlgebra2D::rightUnitNormal(baseHorizon.p2() - baseHorizon.p1());

        const PkLineF horizon = baseHorizon.translated(horizonOffset);

        // base vectors to calculate the side of the horizon
        const PkPointF &basePoint = horizon.p1();
        const PkPointF horizonVec = horizon.p2() - basePoint;


        // iteration
        PkPointF prevPoint = polygon[polygon.size() - 1];
        qreal prevCross = crossCoeff * KisAlgebra2D::crossProduct(horizonVec, prevPoint - basePoint);

        for (int i = 0; i < polygon.size(); i++) {
            const PkPointF &pt = polygon[i];

            qreal cross = crossCoeff * KisAlgebra2D::crossProduct(horizonVec, pt - basePoint);

            if ((cross >= 0 && prevCross >= 0) || (cross == 0 && prevCross < 0)) {
                result << pt;
            } else if (cross * prevCross < 0) {
                PkPointF intersection;
                PkLineF edge(prevPoint, pt);
                PkLineF::IntersectType intersectionType =
                    horizon.intersects(edge, &intersection);

                KIS_ASSERT_RECOVER_NOOP(intersectionType != PkLineF::NoIntersection);

                result << intersection;

                if (cross > 0) {
                    result << pt;
                }
            }

            prevPoint = pt;
            prevCross = cross;
        }

        if (result.size() > 0 && !result.isClosed()) {
            result << result.first();
        }

        return result;
    }

};

KisSafeTransform::KisSafeTransform(const PkTransform &transform,
                                   const PkRect &bounds,
                                   const PkRect &srcInterestRect)
    : m_d(new Private)
{
    m_d->bounds = bounds;

    m_d->forwardTransform = transform;
    m_d->backwardTransform = transform.inverted();

    m_d->needsClipping = transform.type() > PkTransform::TxShear;

    if (m_d->needsClipping) {
        m_d->srcClipPolygon = PkPolygonF(PkRectF(m_d->bounds));
        m_d->dstClipPolygon = PkPolygonF(PkRectF(m_d->bounds));

        qreal crossCoeff = 1.0;

        PkLineF srcHorizon;
        if (m_d->getHorizon(m_d->backwardTransform, &srcHorizon)) {
            crossCoeff = m_d->getCrossSign(srcHorizon, srcInterestRect);
            m_d->srcClipPolygon = m_d->getCroppedPolygon(srcHorizon, m_d->bounds, crossCoeff);
        }

        PkLineF dstHorizon;
        if (m_d->getHorizon(m_d->forwardTransform, &dstHorizon)) {
            crossCoeff = m_d->getCrossSign(dstHorizon, mapRectForward(srcInterestRect));
            m_d->dstClipPolygon = m_d->getCroppedPolygon(dstHorizon, m_d->bounds, crossCoeff);
        }
    }
}

KisSafeTransform::~KisSafeTransform()
{
}

PkPolygonF KisSafeTransform::srcClipPolygon() const
{
    return m_d->srcClipPolygon;
}

PkPolygonF KisSafeTransform::dstClipPolygon() const
{
    return m_d->dstClipPolygon;
}

PkPolygonF KisSafeTransform::mapForward(const PkPolygonF &p)
{
    PkPolygonF poly;

    if (!m_d->needsClipping) {
        poly = m_d->forwardTransform.map(p);
    } else {
        poly = pkPolygonIntersect(m_d->srcClipPolygon, p);
        poly = pkPolygonIntersect(m_d->forwardTransform.map(poly), PkPolygonF(PkRectF(m_d->bounds)));
    }

    return poly;
}

PkPolygonF KisSafeTransform::mapBackward(const PkPolygonF &p)
{
    PkPolygonF poly;

    if (!m_d->needsClipping) {
        poly = m_d->backwardTransform.map(p);
    } else {
        poly = pkPolygonIntersect(m_d->dstClipPolygon, p);
        poly = pkPolygonIntersect(m_d->backwardTransform.map(poly), PkPolygonF(PkRectF(m_d->bounds)));
    }

    return poly;
}

PkRectF KisSafeTransform::mapRectForward(const PkRectF &rc)
{
    return mapForward(rc).boundingRect();
}

PkRectF KisSafeTransform::mapRectBackward(const PkRectF &rc)
{
    return mapBackward(rc).boundingRect();
}

PkRect KisSafeTransform::mapRectForward(const PkRect &rc)
{
    return mapRectForward(PkRectF(rc)).toAlignedRect();
}

PkRect KisSafeTransform::mapRectBackward(const PkRect &rc)
{
    return mapRectBackward(PkRectF(rc)).toAlignedRect();
}
