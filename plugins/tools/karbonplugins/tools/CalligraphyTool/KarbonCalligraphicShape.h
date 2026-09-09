/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2008 Fela Winkelmolen <fela.kde@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KARBONCALLIGRAPHICSHAPE_H
#define KARBONCALLIGRAPHICSHAPE_H

#include <KoParameterShape.h>
#include <PkSharedPointer.h>

#define KarbonCalligraphicShapeId "KarbonCalligraphicShape"

class KarbonCalligraphicPoint
{
public:
    KarbonCalligraphicPoint(const PkPointF &point, double angle, double width)
        : m_point(point), m_angle(angle), m_width(width) {}

    KarbonCalligraphicPoint(const KarbonCalligraphicPoint &rhs) = default;
    KarbonCalligraphicPoint() = default;

    bool operator==(const KarbonCalligraphicPoint &rhs) const {
        return m_point == rhs.m_point &&
            pkQtFuzzyCompare(m_angle, rhs.m_angle) &&
            pkQtFuzzyCompare(m_width, rhs.m_width);
    }

    PkPointF point() const
    {
        return m_point;
    }
    double angle() const
    {
        return m_angle;
    }
    double width() const
    {
        return m_width;
    }

    void setPoint(const PkPointF &point)
    {
        m_point = point;
    }
    void setAngle(double angle)
    {
        m_angle = angle;
    }

private:
    PkPointF m_point; // in shape coordinates
    double m_angle = 0.0;
    double m_width = 0.0;
};

// the indexes of the path will be similar to:
//        7--6--5--4   <- pointCount() / 2
// start  |        |   end    ==> (direction of the stroke)
//        0--1--2--3
class KarbonCalligraphicShape : public KoParameterShape
{
public:
    explicit KarbonCalligraphicShape(double caps = 0.0);
    ~KarbonCalligraphicShape() override;

    KoShape* cloneShape() const override;

    void appendPoint(const PkPointF &p1, double angle, double width);
    void appendPointToPath(const KarbonCalligraphicPoint &p);

    // returns the bounding rect of what needs to be repainted
    // after new points are added
    const PkRectF lastPieceBoundingRect();

    void setSize(const PkSizeF &newSize) override;
    //virtual PkPointF normalize();

    PkPointF normalize() override;

    void simplifyPath();

    void simplifyGuidePath();

    // reimplemented
    PkString pathShapeId() const override;

protected:
    // reimplemented
    void moveHandleAction(int handleId,
                          const PkPointF &point,
                          Pk::KeyboardModifiers modifiers = Pk::NoModifier) override;

    // reimplemented
    void updatePath(const PkSizeF &size) override;

private:
    KarbonCalligraphicShape(const KarbonCalligraphicShape &rhs);

    // auxiliary function that actually inserts the points
    // without doing any additional checks
    // the points should be given in canvas coordinates
    void appendPointsToPathAux(const PkPointF &p1, const PkPointF &p2);

    // function to detect a flip, given the points being inserted
    bool flipDetected(const PkPointF &p1, const PkPointF &p2);

    void smoothLastPoints();
    void smoothPoint(const int index);

    // determine whether the points given are in counterclockwise order or not
    // returns +1 if they are, -1 if they are given in clockwise order
    // and 0 if they form a degenerate triangle
    static int ccw(const PkPointF &p1, const PkPointF &p2, const PkPointF &p3);

    //
    void addCap(int index1, int index2, int pointIndex, bool inverted = false);

    struct Private;
    PkSharedPointer<Private> s;
};

#endif // KARBONCALLIGRAPHICSHAPE_H
