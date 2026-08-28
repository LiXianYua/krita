/*
 * SPDX-FileCopyrightText: 2022 Agata Cacko <cacko.azh@gmail.com>
 */

#include "PerspectiveBasedAssistantHelper.h"

#include <PkTransform.h>

#include "kis_algebra_2d.h"
#include <Eigen/Eigenvalues>

#include <math.h>

#include <functional>



bool PerspectiveBasedAssistantHelper::getTetragon(const PkList<KisPaintingAssistantHandleSP>& handles, bool isAssistantComplete, PkPolygonF& outPolygon)
{
    outPolygon.clear();
    for (int i = 0; i < handles.size(); ++i) {
        outPolygon.push_back(*handles[i]);
    }

    if (!isAssistantComplete) {
        return false;
    }

    int sum = 0;
    int signs[4];

    for (int i = 0; i < 4; ++i) {
        int j = (i == 3) ? 0 : (i + 1);
        int k = (j == 3) ? 0 : (j + 1);
        signs[i] = KisAlgebra2D::signZZ(pdot(outPolygon[j] - outPolygon[i], outPolygon[k] - outPolygon[j]));
        sum += signs[i];
    }

    if (sum == 0) {
        // complex (crossed)
        for (int i = 0; i < 4; ++i) {
            int j = (i == 3) ? 0 : (i + 1);
            if (signs[i] * signs[j] == -1) {
                // opposite signs: uncross
                std::swap(outPolygon[i], outPolygon[j]);
                return true;
            }
        }
        // okay, maybe it's just a line
        return false;
    } else if (sum != 4 && sum != -4) {
        // concave, or a triangle
        if (sum == 2 || sum == -2) {
            // concave, let's return a triangle instead
            for (int i = 0; i < 4; ++i) {
                int j = (i == 3) ? 0 : (i + 1);
                if (signs[i] != KisAlgebra2D::signZZ(sum)) {
                    // wrong sign: drop the inside node
                    outPolygon.remove(j);
                    return false;
                }
            }
        }
        return false;
    }
    // convex
    return true;
}

PkPolygonF PerspectiveBasedAssistantHelper::getAllConnectedTetragon(const PkList<KisPaintingAssistantHandleSP>& handles)
{
    PkPolygonF polyAllConnected;
    if (handles.size() < 4) {
        return polyAllConnected;
    }
    polyAllConnected << *handles[0] << *handles[1] << *handles[2] << *handles[3] << *handles[0] << *handles[2] << *handles[1] << *handles[3];
    return polyAllConnected;
}

qreal PerspectiveBasedAssistantHelper::localScale(const PkTransform &transform, PkPointF pt)
{
    //    const qreal epsilon = 1e-5, epsilonSquared = epsilon * epsilon;
    //    qreal xSizeSquared = lengthSquared(transform.map(pt + PkPointF(epsilon, 0.0)) - orig) / epsilonSquared;
    //    qreal ySizeSquared = lengthSquared(transform.map(pt + PkPointF(0.0, epsilon)) - orig) / epsilonSquared;
    //    xSizeSquared /= lengthSquared(transform.map(PkPointF(0.0, pt.y())) - transform.map(PkPointF(1.0, pt.y())));
    //    ySizeSquared /= lengthSquared(transform.map(PkPointF(pt.x(), 0.0)) - transform.map(PkPointF(pt.x(), 1.0)));
    //  when taking the limit epsilon->0:
    //  xSizeSquared=((m23*y+m33)^2*(m23*y+m33+m13)^2)/(m23*y+m13*x+m33)^4
    //  ySizeSquared=((m23*y+m33)^2*(m23*y+m33+m13)^2)/(m23*y+m13*x+m33)^4
    //  xSize*ySize=(abs(m13*x+m33)*abs(m13*x+m33+m23)*abs(m23*y+m33)*abs(m23*y+m33+m13))/(m23*y+m13*x+m33)^4
    const qreal x = transform.m13() * pt.x(),
            y = transform.m23() * pt.y(),
            a = x + transform.m33(),
            b = y + transform.m33(),
            c = x + y + transform.m33(),
            d = c * c;
    return fabs(a*(a + transform.m23())*b*(b + transform.m13()))/(d * d);
}

qreal PerspectiveBasedAssistantHelper::inverseMaxLocalScale(const PkTransform &transform)
{
    const qreal a = fabs((transform.m33() + transform.m13()) * (transform.m33() + transform.m23())),
            b = fabs((transform.m33()) * (transform.m13() + transform.m33() + transform.m23())),
            d00 = transform.m33() * transform.m33(),
            d11 = (transform.m33() + transform.m23() + transform.m13())*(transform.m33() + transform.m23() + transform.m13()),
            s0011 = qMin(d00, d11) / a,
            d10 = (transform.m33() + transform.m13()) * (transform.m33() + transform.m13()),
            d01 = (transform.m33() + transform.m23()) * (transform.m33() + transform.m23()),
            s1001 = qMin(d10, d01) / b;
    return qMin(s0011, s1001);
}

qreal PerspectiveBasedAssistantHelper::distanceInGrid(const PkList<KisPaintingAssistantHandleSP>& handles, bool isAssistantComplete, const PkPointF &point)
{
    // TODO: make it not calculate the poly max distance over and over
    qreal defaultValue = 1;
    int vertexCount = 4;

    PkPolygonF poly;
    if (!PerspectiveBasedAssistantHelper::getTetragon(handles, isAssistantComplete, poly)) {
        return defaultValue;
    }

    boost::optional<PkPointF> vp1;
    boost::optional<PkPointF> vp2;

    PerspectiveBasedAssistantHelper::getVanishingPointsOptional(poly, vp1, vp2);
    if (!vp1 && !vp2) {
        return defaultValue; // possibly wrong shape
    } else if (!vp1 || !vp2) {
        // result should be:
        // dist from horizon / max dist from horizon
        // horizon is parallel to the parallel sides of the tetragon (two must be parallel if there is only one vp)
        PkLineF horizon;
        if (vp1) {
            // that means the 0-1 line is the horizon parallel line
            horizon = PkLineF(vp1.get(), vp1.get() + poly[1] - poly[0]);
        } else {
            horizon = PkLineF(vp2.get(), vp2.get() + poly[2] - poly[1]);
        }

        qreal dist = kisDistanceToLine(point, horizon);
        qreal distMax = 0;
        for (int i = 0; i < vertexCount; i++) {
            qreal vertexDist = kisDistanceToLine(poly[i], horizon);
            if (vertexDist > distMax) {
                distMax = vertexDist;
            }
        }
        if (distMax == 0) {
            return defaultValue;
        }
        return dist/distMax;
    } else if (vp1 && vp2) {
        // should be:
        // dist from vp-line / max dist from vp-line
        PkLineF horizon = PkLineF(vp1.get(), vp2.get());

        qreal dist = kisDistanceToLine(point, horizon);
        qreal distMax = 0;
        for (int i = 0; i < vertexCount; i++) {
            qreal vertexDist = kisDistanceToLine(poly[i], horizon);
            if (vertexDist > distMax) {
                distMax = vertexDist;
            }
        }
        if (distMax == 0) {
            return defaultValue;
        }
        return dist/distMax;
    }

    return defaultValue;

}

qreal PerspectiveBasedAssistantHelper::distanceInGrid(const PerspectiveBasedAssistantHelper::CacheData &cache, const PkPointF& point)
{
    qreal defaultValue = 1;
    if (cache.maxDistanceFromPoint == 0.0) {
        return defaultValue;
    }

    if (!cache.vanishingPoint1 && !cache.vanishingPoint2) {
        return defaultValue; // possibly wrong shape
    } else if (!cache.vanishingPoint1 || !cache.vanishingPoint2) {
        // result should be:
        // dist from horizon / max dist from horizon
        // horizon is parallel to the parallel sides of the tetragon (two must be parallel if there is only one vp)
        qreal dist = kisDistanceToLine(point, cache.horizon);
        return dist/cache.maxDistanceFromPoint;
    } else if (cache.vanishingPoint1 && cache.vanishingPoint2) {
        // should be:
        // dist from vp-line / max dist from vp-line
        qreal dist = kisDistanceToLine(point, cache.horizon);
        return dist/cache.maxDistanceFromPoint;
    }

    return defaultValue;
}

void PerspectiveBasedAssistantHelper::updateCacheData(PerspectiveBasedAssistantHelper::CacheData &cache, const PkPolygonF &poly)
{
    cache.polygon = poly;
    bool r = PerspectiveBasedAssistantHelper::getVanishingPointsOptional(poly, cache.vanishingPoint1, cache.vanishingPoint2);
    (void)r;

    if (cache.vanishingPoint1 && cache.vanishingPoint2) {
        cache.type = CacheData::TwoVps;
    } else if (cache.vanishingPoint1 || cache.vanishingPoint2) {
        cache.type = CacheData::OneVp;
    } else {
        cache.type = CacheData::None;
        cache.horizon = PkLineF();
        cache.distancesFromPoints = PkVector<qreal>();
        cache.maxDistanceFromPoint = 0.0;
        return;
    }

    if (cache.type == CacheData::TwoVps) {
        cache.horizon = PkLineF(cache.vanishingPoint1.get(), cache.vanishingPoint2.get());
    } else if (cache.type == CacheData::OneVp) {
        if (cache.vanishingPoint1) {
            // that means the 0-1 line is the horizon parallel line
            cache.horizon = PkLineF(cache.vanishingPoint1.get(), cache.vanishingPoint1.get() + cache.polygon[1] - cache.polygon[0]);
        } else { // the other vp
            cache.horizon = PkLineF(cache.vanishingPoint2.get(), cache.vanishingPoint2.get() + cache.polygon[2] - cache.polygon[1]);
        }
    }

    int vertexCount = 4;
    cache.distancesFromPoints.fill(0.0, vertexCount);
    for (int i = 0; i < vertexCount; i++) {
        qreal vertexDist = kisDistanceToLine(cache.polygon[i], cache.horizon);
        if (vertexDist > cache.maxDistanceFromPoint) {
            cache.maxDistanceFromPoint = vertexDist;
        }
        cache.distancesFromPoints[i] = vertexDist;
    }

}

bool PerspectiveBasedAssistantHelper::getVanishingPointsOptional(const PkPolygonF &poly, boost::optional<PkPointF> &vp1, boost::optional<PkPointF> &vp2)
{
    bool either = false;
    vp1 = boost::none;
    vp2 = boost::none;

    if (poly.size() < 4) { // four points are required for a tetragon
        return false;
    }

    PkPointF intersection(0, 0);
    // note: in code it seems like vp1 and vp2 are swapped, but it's all correct if you read carefully

    if (fmod(PkLineF(poly[0], poly[1]).angle(), 180.0)>=fmod(PkLineF(poly[2], poly[3]).angle(), 180.0)+2.0
            || fmod(PkLineF(poly[0], poly[1]).angle(), 180.0)<=fmod(PkLineF(poly[2], poly[3]).angle(), 180.0)-2.0) {
        if (PkLineF(poly[0], poly[1]).intersects(PkLineF(poly[2], poly[3]), &intersection) != PkLineF::NoIntersection) {
            vp2 = intersection;
            either = true;
        }
    }
    if (fmod(PkLineF(poly[1], poly[2]).angle(), 180.0)>=fmod(PkLineF(poly[3], poly[0]).angle(), 180.0)+2.0
            || fmod(PkLineF(poly[1], poly[2]).angle(), 180.0)<=fmod(PkLineF(poly[3], poly[0]).angle(), 180.0)-2.0){
        if (PkLineF(poly[1], poly[2]).intersects(PkLineF(poly[3], poly[0]), &intersection) != PkLineF::NoIntersection) {
            vp1 = intersection;
            either = true;
        }
    }
    return either;
}

qreal PerspectiveBasedAssistantHelper::pdot(const PkPointF &a, const PkPointF &b)
{
    return a.x() * b.y() - a.y() * b.x();
}
