/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISBEZIERUTILS_H
#define KISBEZIERUTILS_H

#include "kritaglobal_export.h"

#include <kis_algebra_2d.h>

namespace KisBezierUtils
{
using KisAlgebra2D::lerp;

inline PkPointF bezierCurveDeriv(const PkPointF p0,
                                const PkPointF p1,
                                const PkPointF p2,
                                const PkPointF p3,
                                qreal t)
{
    const qreal t_2 = pow2(t);
    const qreal t_inv = 1.0 - t;
    const qreal t_inv_2 = pow2(t_inv);

    return
        3 * t_inv_2 * (p1 - p0) +
        6 * t_inv * t * (p2 - p1) +
        3 * t_2 * (p3 - p2);
}

inline PkPointF bezierCurveDeriv2(const PkPointF p0,
                                 const PkPointF p1,
                                 const PkPointF p2,
                                 const PkPointF p3,
                                 qreal t)
{
    const qreal t_inv = 1.0 - t;

    return
        6 * t_inv * (p2 - 2 * p1 + p0) +
        6 * t * (p3 - 2 * p2 + p1);
}

inline void deCasteljau(const PkPointF &q0,
                        const PkPointF &q1,
                        const PkPointF &q2,
                        const PkPointF &q3,
                        qreal t,
                        PkPointF *p0,
                        PkPointF *p1,
                        PkPointF *p2,
                        PkPointF *p3,
                        PkPointF *p4)
{
    PkPointF q[4];

    q[0] = q0;
    q[1] = q1;
    q[2] = q2;
    q[3] = q3;

    // points of the new segment after the split point
    PkPointF p[3];

    // the De Casteljau algorithm
    for (unsigned short j = 1; j <= 3; ++j) {
        for (unsigned short i = 0; i <= 3 - j; ++i) {
            q[i] = (1.0 - t) * q[i] + t * q[i + 1];
        }
        p[j - 1] = q[0];
    }

    *p0 = p[0];
    *p1 = p[1];
    *p2 = p[2];
    *p3 = q[1];
    *p4 = q[2];
}

inline PkPointF bezierCurve(const PkPointF p0,
                           const PkPointF p1,
                           const PkPointF p2,
                           const PkPointF p3,
                           qreal t)
{
#if 0
    const qreal t_2 = pow2(t);
    const qreal t_3 = t_2 * t;
    const qreal t_inv = 1.0 - t;
    const qreal t_inv_2 = pow2(t_inv);
    const qreal t_inv_3 = t_inv_2 * t_inv;

    return
        t_inv_3 * p0 +
        3 * t_inv_2 * t * p1 +
        3 * t_inv * t_2 * p2 +
        t_3 * p3;
#else
    PkPointF q0, q1, q2, q3, q4;
    deCasteljau(p0, p1, p2, p3, t, &q0, &q1, &q2, &q3, &q4);
    return q2;
#endif
}

inline PkPointF bezierCurve(const PkList<PkPointF> &points,
                           qreal t)
{
    PkPointF result;

    if (points.size() == 2) {
        result = lerp(points.first(), points.last(), t);
    } else if (points.size() == 3) {
        result = bezierCurve(points[0], points[1], points[1], points[2], t);
    } else if (points.size() == 4) {
        result = bezierCurve(points[0], points[1], points[2], points[3], t);
    } else {
        KIS_SAFE_ASSERT_RECOVER_NOOP(0 && "Unsupported number of bezier control points");
    }

    return result;
}

inline bool isLinearSegmentByDerivatives(const PkPointF &p0, const PkPointF &d0,
                                         const PkPointF &p1, const PkPointF &d1,
                                         const qreal eps = 1e-4)
{
    const PkPointF diff = p1 - p0;
    const qreal dist = KisAlgebra2D::norm(diff);

    const qreal normCoeff = 1.0 / 3.0 / dist;

    const qreal offset1 =
        normCoeff * qAbs(KisAlgebra2D::crossProduct(diff, d0));
    if (offset1 > eps) return false;

    const qreal offset2 =
        normCoeff * qAbs(KisAlgebra2D::crossProduct(diff, d1));
    if (offset2 > eps) return false;

    return true;
}

inline bool isLinearSegmentByControlPoints(const PkPointF &p0, const PkPointF &p1,
                                           const PkPointF &p2, const PkPointF &p3,
                                           const qreal eps = 1e-4)
{
    return isLinearSegmentByDerivatives(p0, (p1 - p0) * 3.0, p3, (p3 - p2) * 3.0, eps);
}

inline int bezierDegree(const PkPointF p0,
                        const PkPointF p1,
                        const PkPointF p2,
                        const PkPointF p3)
{
    const qreal eps = 1e-4;

    int degree = 3;

    if (isLinearSegmentByControlPoints(p0, p1, p2, p3, eps)) {
        degree = 1;
    } else if (KisAlgebra2D::fuzzyPointCompare(p1, p2, eps)) {
        degree = 2;
    }

    return degree;
}

KRITAGLOBAL_EXPORT
PkVector<qreal> linearizeCurve(const PkPointF p0,
                              const PkPointF p1,
                              const PkPointF p2,
                              const PkPointF p3,
                              const qreal eps);
KRITAGLOBAL_EXPORT
PkVector<qreal> mergeLinearizationSteps(const PkVector<qreal> &a, const PkVector<qreal> &b);

KRITAGLOBAL_EXPORT
qreal nearestPoint(const PkList<PkPointF> controlPoints, const PkPointF &point, qreal *resultDistance = 0, PkPointF *resultPoint = 0);

KRITAGLOBAL_EXPORT
int controlPolygonZeros(const PkList<PkPointF> &controlPoints);

/**
 * @brief calculates local (u,v) coordinates of the patch corresponding to \p globalPoint
 *
 * The function uses Krita's own level-based patch interpolation algorithm
 *
 * @param points control points as laid out in KisBezierPatch
 * @param globalPoint point in global coordinates
 * @return point in local coordinates
 */
KRITAGLOBAL_EXPORT
PkPointF calculateLocalPos(const std::array<PkPointF, 12> &points,
                          const PkPointF &globalPoint);

/**
 * @brief calculates global coordinate corresponding to the patch coordinate (u, v)
 *
 * The function uses Krita's own level-based patch interpolation algorithm
 *
 * @param points control points as laid out in KisBezierPatch
 * @param localPoint point in local coordinates
 * @return point in global coordinates
 */
KRITAGLOBAL_EXPORT
PkPointF calculateGlobalPos(const std::array<PkPointF, 12> &points, const PkPointF &localPoint);

/**
 * @brief calculates local (u,v) coordinates of the patch corresponding to \p globalPoint
 *
 * The function uses SVG2 toon patches algorithm
 *
 * @param points control points as laid out in KisBezierPatch
 * @param globalPoint point in global coordinates
 * @return point in local coordinates
 */
KRITAGLOBAL_EXPORT
PkPointF calculateLocalPosSVG2(const std::array<PkPointF, 12> &points,
                              const PkPointF &globalPoint);

/**
 * @brief calculates global coordinate corresponding to the patch coordinate (u, v)
 *
 * The function uses SVG2 toon patches algorithm
 *
 * @param points control points as laid out in KisBezierPatch
 * @param localPoint point in local coordinates
 * @return point in global coordinates
 */
KRITAGLOBAL_EXPORT
PkPointF calculateGlobalPosSVG2(const std::array<PkPointF, 12> &points, const PkPointF &localPoint);

/**
 * @brief Interpolates quadric curve passing through given points
 *
 * Interpolates quadric curve passing through \p p0, \p pt and \p p2
 * with ensuring that \p pt placed at position \p t
 * @return interpolated value for control point p1
 */
KRITAGLOBAL_EXPORT
PkPointF interpolateQuadric(const PkPointF &p0, const PkPointF &p2, const PkPointF &pt, qreal t);

/**
 * @brief moves point \p t of the curve by offset \p offset
 * @return proposed offsets for points p1 and p2 of the curve
 */
KRITAGLOBAL_EXPORT
std::pair<PkPointF, PkPointF> offsetSegment(qreal t, const PkPointF &offset);

KRITAGLOBAL_EXPORT
qreal curveLength(const PkPointF p0,
                  const PkPointF p1,
                  const PkPointF p2,
                  const PkPointF p3,
                  const qreal error);

KRITAGLOBAL_EXPORT
qreal curveLengthAtPoint(const PkPointF p0,
                         const PkPointF p1,
                         const PkPointF p2,
                         const PkPointF p3,
                         qreal t,
                         const qreal error);

KRITAGLOBAL_EXPORT
qreal curveParamByProportion(const PkPointF p0,
                             const PkPointF p1,
                             const PkPointF p2,
                             const PkPointF p3,
                             qreal proportion,
                             const qreal error);

KRITAGLOBAL_EXPORT
qreal curveProportionByParam(const PkPointF p0, const PkPointF p1, const PkPointF p2, const PkPointF p3, qreal t, const qreal error);

/**
 * @brief Adjusts position for the bezier control points
 * after removing a node.
 *
 * First source curve P: \p p0, \p p1, \p p2, \p p3
 * Second source curve Q: \p p3, \p q1, \p q2, \p q3
 *
 * Node to remove: \p p3 and its control points \p p2 and \p q1
 *
 * @return a pair of new positions for \p p1 and \p q2
 */
KRITAGLOBAL_EXPORT
std::pair<PkPointF, PkPointF> removeBezierNode(const PkPointF &p0,
                                             const PkPointF &p1,
                                             const PkPointF &p2,
                                             const PkPointF &p3,
                                             const PkPointF &q1,
                                             const PkPointF &q2,
                                             const PkPointF &q3);

/**
 * Find intersection points (their parameter values) between a cubic
 * Bezier curve and a line.
 *
 * Curve: \p p0, \p p1, \p p2, \p p3
 * Line: \p line
 *
 * For cubic Bezier curves there can be at most three intersection points.
 */
KRITAGLOBAL_EXPORT
PkVector<qreal> intersectWithLine(const PkPointF &p0,
                                 const PkPointF &p1,
                                 const PkPointF &p2,
                                 const PkPointF &p3,
                                 const PkLineF &line,
                                 qreal eps);

/**
 * Find new nearest intersection point between a cubic Bezier curve and a line.
 * The resulting point will be the nearest to \p nearestAnchor.
 */
KRITAGLOBAL_EXPORT
boost::optional<qreal> intersectWithLineNearest(const PkPointF &p0,
                                                const PkPointF &p1,
                                                const PkPointF &p2,
                                                const PkPointF &p3,
                                                const PkLineF &line,
                                                const PkPointF &nearestAnchor,
                                                qreal eps);

} // namespace KisBezierUtils

#endif // KISBEZIERUTILS_H
