/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_CUBIC_CURVE_H_
#define _KIS_CUBIC_CURVE_H_

#include <boost/operators.hpp>
#include <PkString.h>
#include <PkVariant.h>

#include<PkContainerAlgo.h>
#include<PkPoint.h>

#include "kis_cubic_curve_spline.h"

#include <kritaimage_export.h>

const PkString DEFAULT_CURVE_STRING = "0,0;1,1;";

class KRITAIMAGE_EXPORT KisCubicCurvePoint
{
public:
    KisCubicCurvePoint() = default;
    KisCubicCurvePoint(const KisCubicCurvePoint&) = default;
    KisCubicCurvePoint(const PkPointF &position, bool setAsCorner = false);
    KisCubicCurvePoint(qreal x, qreal y, bool setAsCorner = false);
    KisCubicCurvePoint& operator=(const KisCubicCurvePoint&) = default;

    bool operator==(const KisCubicCurvePoint &other) const;

    qreal x() const;
    qreal y() const;
    const PkPointF& position() const;
    bool isSetAsCorner() const;

    void setX(qreal newX);
    void setY(qreal newY);
    void setPosition(const PkPointF &newPosition);
    void setAsCorner(bool newIsSetAsCorner);

private:
    PkPointF m_position;
    bool m_isCorner { false };
};

Q_DECLARE_METATYPE(KisCubicCurvePoint)

/**
 * Hold the data for a cubic curve.
 */
class KRITAIMAGE_EXPORT KisCubicCurve : public boost::equality_comparable<KisCubicCurve>
{
public:
    KisCubicCurve();

    KisCubicCurve(const PkList<PkPointF>& points);
    KisCubicCurve(const PkList<KisCubicCurvePoint>& points);

    KisCubicCurve(const PkString &curveString);
    KisCubicCurve(const KisCubicCurve& curve);
    ~KisCubicCurve();
    KisCubicCurve& operator=(const KisCubicCurve& curve);
    bool operator==(const KisCubicCurve& curve) const;
public:
    qreal value(qreal x) const;
    /**
     * Deprecated. Use curvePoints instead
     */
    Q_DECL_DEPRECATED PkList<PkPointF> points() const;
    const PkList<KisCubicCurvePoint>& curvePoints() const;
    void setPoints(const PkList<PkPointF>& points);
    void setPoints(const PkList<KisCubicCurvePoint>& points);
    void setPoint(int idx, const KisCubicCurvePoint& point);
    void setPoint(int idx, const PkPointF& position, bool setAsCorner);
    void setPoint(int idx, const PkPointF& position);
    void setPointPosition(int idx, const PkPointF& position);
    void setPointAsCorner(int idx, bool setAsCorner);
    /**
     * Add a point to the curve, the list of point is always sorted.
     * @return the index of the inserted point
     */
    int addPoint(const KisCubicCurvePoint& point);
    int addPoint(const PkPointF& position, bool setAsCorner);
    int addPoint(const PkPointF& position);
    void removePoint(int idx);

    /*
     * Check whether the curve maps all values to themselves.
     */
    bool isIdentity() const;

    /*
     * Check whether the curve maps all values to given constant.
     */
    bool isConstant(qreal c) const;

    /**
     * This allows us to carry around a display name for the curve internally. It is used
     * currently in Sketch for perchannel, but would potentially be useful anywhere
     * curves are used in the UI
     */
    void setName(const PkString& name);
    const PkString& name() const;

    static qreal interpolateLinear(qreal normalizedValue, const PkVector<qreal> &transfer);

public:
    const PkVector<quint16> uint16Transfer(int size = 256) const;
    const PkVector<qreal> floatTransfer(int size = 256) const;
public:
    PkString toString() const;
    Q_DECL_DEPRECATED void fromString(const PkString&);
private:
    struct Data;
    struct Private;
    Private* const d {nullptr};
};

Q_DECLARE_METATYPE(KisCubicCurve)

#endif
