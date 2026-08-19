/*
 *  SPDX-FileCopyrightText: 2000 Matthias Elter <elter@kde.org>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISGLOBAL_H_
#define KISGLOBAL_H_

#include <limits>
#include <cstdint>
#include <algorithm>

#include <KoConfig.h>
#include "kis_assert.h"

#include <PkPoint.h>
#include <PkLine.h>
#include <PkRect.h>
#include <PkSize.h>

const uint8_t quint8_MAX = std::numeric_limits<uint8_t>::max();
const uint16_t quint16_MAX = std::numeric_limits<uint16_t>::max();

const int16_t qint16_MIN = std::numeric_limits<int16_t>::min();
const int16_t qint16_MAX = std::numeric_limits<int16_t>::max();
const int32_t qint32_MAX = std::numeric_limits<int32_t>::max();
const int32_t qint32_MIN = std::numeric_limits<int32_t>::min();

const uint8_t MAX_SELECTED = std::numeric_limits<uint8_t>::max();
const uint8_t MIN_SELECTED = std::numeric_limits<uint8_t>::min();
const uint8_t SELECTION_THRESHOLD = 1;

template <typename T>
constexpr inline const T &kisBoundFast(const T &min, const T &val, const T &max)
{
    /**
     * This is a fork of an old version of qBound. It has the following properties:
     * 1) Does **not** have asserts (we cannot have them in blendmodes)
     * 2) Does not have automatic type deduction. The user must explicitly type the
     *    common type in case of ambiguity.
     *
     * Rules of thumb:
     *
     * 1) If you are writing time-critical code (e.g. blendmodes), use kisBoundFast()
     * 2) Otherwise use qBound() or std::clamp (the latter may optionally have an assert as well)
     */
    return std::max(min, std::min(max, val));
}

enum OutlineStyle {
    OUTLINE_NONE = 0,
    OUTLINE_CIRCLE,
    OUTLINE_FULL,
    OUTLINE_TILT,

    N_OUTLINE_STYLE_SIZE
};

enum CursorStyle {
    CURSOR_STYLE_NO_CURSOR = 0,
    CURSOR_STYLE_TOOLICON,
    CURSOR_STYLE_POINTER,
    CURSOR_STYLE_SMALL_ROUND,
    CURSOR_STYLE_CROSSHAIR,
    CURSOR_STYLE_TRIANGLE_RIGHTHANDED,
    CURSOR_STYLE_TRIANGLE_LEFTHANDED,
    CURSOR_STYLE_BLACK_PIXEL,
    CURSOR_STYLE_WHITE_PIXEL,
    CURSOR_STYLE_ERASER,

    N_CURSOR_STYLE_SIZE
};

enum OldCursorStyle {
    OLD_CURSOR_STYLE_TOOLICON = 0,
    OLD_CURSOR_STYLE_CROSSHAIR = 1,
    OLD_CURSOR_STYLE_POINTER = 2,

    OLD_CURSOR_STYLE_OUTLINE = 3,

    OLD_CURSOR_STYLE_NO_CURSOR = 4,
    OLD_CURSOR_STYLE_SMALL_ROUND = 5,

    OLD_CURSOR_STYLE_OUTLINE_CENTER_DOT = 6,
    OLD_CURSOR_STYLE_OUTLINE_CENTER_CROSS = 7,

    OLD_CURSOR_STYLE_TRIANGLE_RIGHTHANDED = 8,
    OLD_CURSOR_STYLE_TRIANGLE_LEFTHANDED = 9,

    OLD_CURSOR_STYLE_OUTLINE_TRIANGLE_RIGHTHANDED = 10,
    OLD_CURSOR_STYLE_OUTLINE_TRIANGLE_LEFTHANDED = 11
};

const double PRESSURE_MIN = 0.0;
const double PRESSURE_MAX = 1.0;
const double PRESSURE_DEFAULT = PRESSURE_MAX;
const double PRESSURE_THRESHOLD = 5.0 / 255.0;

// copy of lcms.h
#define INTENT_PERCEPTUAL                 0
#define INTENT_RELATIVE_COLORIMETRIC      1
#define INTENT_SATURATION                 2
#define INTENT_ABSOLUTE_COLORIMETRIC      3

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Name of the property in the KisApplication that contains the name
// of the current style, even if there is a stylesheet applied
constexpr const char *currentUnderlyingStyleNameProperty = "currentUnderlyingStyleName";

// converts \p a to [0, 2 * M_PI) range
template<typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
normalizeAngle(T a) {
    if (a < T(0.0)) {
        a = T(2 * M_PI) + std::fmod(a, T(2 * M_PI));
    }

    return a >= T(2 * M_PI) ? std::fmod(a, T(2 * M_PI)) : a;
}

// converts \p a to [0, 360.0) range
template<typename T>
typename std::enable_if<std::is_floating_point<T>::value, T>::type
normalizeAngleDegrees(T a) {
    if (a < T(0.0)) {
        a = T(360.0) + std::fmod(a, T(360.0));
    }

    return a >= T(360.0) ? std::fmod(a, T(360.0)) : a;
}

inline double shortestAngularDistance(double a, double b) {
    double dist = fmod(std::abs(a - b), 2 * M_PI);
    if (dist > M_PI) dist = 2 * M_PI - dist;

    return dist;
}

inline double incrementInDirection(double a, double inc, double direction) {
    double b1 = a + inc;
    double b2 = a - inc;

    double d1 = shortestAngularDistance(b1, direction);
    double d2 = shortestAngularDistance(b2, direction);

    return d1 < d2 ? b1 : b2;
}

inline double bisectorAngle(double a, double b) {
    const double diff = shortestAngularDistance(a, b);
    return incrementInDirection(a, 0.5 * diff, b);
}

template<typename T>
inline T pow2(const T& x) {
    return x * x;
}

template<typename T>
inline T pow3(const T& x) {
    return x * x * x;
}

template<typename T>
inline T kisDegreesToRadians(T degrees) {
    return degrees * M_PI / 180.0;
}

template<typename T>
inline T kisRadiansToDegrees(T radians) {
    return radians * 180.0 / M_PI;
}

template<class T, typename U>
inline T kisGrowRect(const T &rect, U offset) {
    return rect.adjusted(-offset, -offset, offset, offset);
}

inline double kisDistance(const PkPointF &pt1, const PkPointF &pt2) {
    return std::sqrt(pow2(pt1.x() - pt2.x()) + pow2(pt1.y() - pt2.y()));
}

inline double kisSquareDistance(const PkPointF &pt1, const PkPointF &pt2) {
    return pow2(pt1.x() - pt2.x()) + pow2(pt1.y() - pt2.y());
}

template<typename PointType>
inline PointType snapToClosestAxis(PointType P) {
    if (std::abs(P.x()) < std::abs(P.y())) {
        P.setX(0);
    } else {
        P.setY(0);
    }
    return P;
}

template<typename PointType>
inline PointType snapToClosestNiceAngle(PointType point, PointType startPoint, double angle = (2 * M_PI) / 24) {
    // default angle = 15 degrees

    const PkPointF lineVector = point - startPoint;
    double lineAngle = std::atan2(lineVector.y(), lineVector.x());

    if (lineAngle < 0) {
        lineAngle += 2 * M_PI;
    }

    const uint32_t constrainedLineIndex = static_cast<uint32_t>((lineAngle / angle) + 0.5);
    const double constrainedLineAngle = constrainedLineIndex * angle;

    const double lineLength = kisDistance(lineVector, PkPointF());

    const PkPointF constrainedLineVector(lineLength * std::cos(constrainedLineAngle), lineLength * std::sin(constrainedLineAngle));

    const PkPointF result = startPoint + constrainedLineVector;

    return result;
}

inline double kisDistanceToLine(const PkPointF &m, const PkLineF &line)
{
    const PkPointF &p1 = line.p1();
    const PkPointF &p2 = line.p2();

    double distance = 0;

    if (std::abs(p1.x() - p2.x()) < 1e-12) {
        distance = std::abs(m.x() - p2.x());
    } else if (std::abs(p1.y() - p2.y()) < 1e-12) {
        distance = std::abs(m.y() - p2.y());
    } else {
        double A = 1;
        double B = - (p1.x() - p2.x()) / (p1.y() - p2.y());
        double C = - p1.x() - B * p1.y();

        distance = std::abs(A * m.x() + B * m.y() + C) / std::sqrt(pow2(A) + pow2(B));
    }

    return distance;
}

inline double kisSquareDistanceToLine(const PkPointF &m, const PkLineF &line)
{
    const PkPointF &p1 = line.p1();
    const PkPointF &p2 = line.p2();

    double distance = 0;

    if (std::abs(p1.x() - p2.x()) < 1e-12) {
        distance = pow2(m.x() - p2.x());
    } else if (std::abs(p1.y() - p2.y()) < 1e-12) {
        distance = pow2(m.y() - p2.y());
    } else {
        double A = 1;
        double B = - (p1.x() - p2.x()) / (p1.y() - p2.y());
        double C = - p1.x() - B * p1.y();

        distance = pow2(A * m.x() + B * m.y() + C) / (pow2(A) + pow2(B));
    }

    return distance;

}

inline PkPointF kisProjectOnVector(const PkPointF &base, const PkPointF &v)
{
    const double prod = base.x() * v.x() + base.y() * v.y();
    const double lengthSq = pow2(base.x()) + pow2(base.y());
    double coeff = prod / lengthSq;

    return coeff * base;
}

inline PkRect kisEnsureInRect(PkRect rc, const PkRect &bounds)
{
    if(rc.right() > bounds.right()) {
        rc.translate(bounds.right() - rc.right(), 0);
    }

    if(rc.left() < bounds.left()) {
        rc.translate(bounds.left() - rc.left(), 0);
    }

    if(rc.bottom() > bounds.bottom()) {
        rc.translate(0, bounds.bottom() - rc.bottom());
    }

    if(rc.top() < bounds.top()) {
        rc.translate(0, bounds.top() - rc.top());
    }

    return rc;
}

inline PkRectF kisTrimLeft( int width, PkRectF &toTakeFrom)
{
    PkPointF trimmedOrigin = toTakeFrom.topLeft();
    PkSize trimmedSize = PkSize(width, toTakeFrom.height());
    toTakeFrom.setWidth(toTakeFrom.width() - width);
    toTakeFrom.translate(width, 0);
    return PkRectF(trimmedOrigin, trimmedSize);
}

inline PkRect kisTrimLeft( int width, PkRect &toTakeFrom)
{
    PkRectF converted = PkRectF(toTakeFrom);
    PkRectF toReturn = kisTrimLeft(width, converted);
    toTakeFrom = converted.toAlignedRect();
    return toReturn.toAlignedRect();
}

inline PkRectF kisTrimTop( int height, PkRectF& toTakeFrom)
{
    PkPointF trimmedOrigin = toTakeFrom.topLeft();
    PkSize trimmedSize = PkSize(toTakeFrom.width(), height);
    toTakeFrom.setHeight(toTakeFrom.height() - height);
    toTakeFrom.translate(0, height);
    return PkRectF(trimmedOrigin, trimmedSize);
}

inline PkRect kisTrimTop( int height, PkRect& toTakeFrom)
{
    PkRectF converted = PkRectF(toTakeFrom);
    PkRectF toReturn = kisTrimTop(height, converted);
    toTakeFrom = converted.toAlignedRect();
    return toReturn.toAlignedRect();
}

#include "kis_pointer_utils.h"
#include <type_traits>

// Round up to the next power of two (replacement for Qt's qNextPowerOfTwo,
// which only had a quint32 overload). The bit-twiddling runs on an unsigned,
// fixed-width base type: this normalizes size_t (which Linux and macOS define
// differently) and keeps the shifts from ever hitting signed overflow.
template <typename T>
inline T nextPowerOfTwo(T v)
{
    static_assert(std::is_integral<T>::value, "Value has to be an integral number");
    using base_type = typename std::conditional<sizeof(T) == sizeof(uint64_t), uint64_t, uint32_t>::type;
    base_type cv = static_cast<base_type>(v);
    if (cv == 0) return static_cast<T>(1);
    cv--;
    cv |= cv >> 1;
    cv |= cv >> 2;
    cv |= cv >> 4;
    cv |= cv >> 8;
    cv |= cv >> 16;
    if constexpr (sizeof(base_type) >= 8) {
        cv |= cv >> 32;
    }
    cv++;
    return static_cast<T>(cv);
}

#endif // KISGLOBAL_H_

