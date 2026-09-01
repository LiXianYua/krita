/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2008 Fela Winkelmolen <fela.kde@gmail.com>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KarbonCalligraphicShape.h"

#include <KoPathPoint.h>

#include <KoParameterShape_p.h>
#include "KarbonSimplifyPath.h"
#include <KoCurveFit.h>
#include <KoColorBackground.h>

#include <PkColor.h>
#include <PkPainterPath.h>

#include <cmath>
#include <cstdlib>

#undef M_PI
const qreal M_PI = 3.1415927;

struct KarbonCalligraphicShape::Private
{
    Private(qreal _caps)
        : lastWasFlip(false),
          caps(_caps)

    {
    }

    Private(const Private &rhs) = default;

    bool lastWasFlip;
    qreal caps = 0.0;
    // the actual data then determines it's shape (guide path + data for points)
    PkList<KarbonCalligraphicPoint> points;
};

KarbonCalligraphicShape::KarbonCalligraphicShape(qreal caps)
    : s(new Private(caps))
{
    setShapeId(KoPathShapeId);
    setFillRule(Qt::WindingFill);
    setBackground(PkSharedPointer<KoShapeBackground>(new KoColorBackground(PkColor(Qt::black))));
    setStroke(KoShapeStrokeModelSP());
}

KarbonCalligraphicShape::KarbonCalligraphicShape(const KarbonCalligraphicShape &rhs)
    : KoParameterShape(rhs),
      s(PkSharedPointer<Private>::create(*rhs.s))
{
}

KarbonCalligraphicShape::~KarbonCalligraphicShape()
{
}

KoShape *KarbonCalligraphicShape::cloneShape() const
{
    return new KarbonCalligraphicShape(*this);
}

void KarbonCalligraphicShape::appendPoint(const PkPointF &point, qreal angle, qreal width)
{
    // convert the point from canvas to shape coordinates
    PkPointF p = point - position();
    KarbonCalligraphicPoint calligraphicPoint(p, angle, width);

    PkList<PkPointF> handles = this->handles();
    handles.append(p);
    setHandles(handles);
    s->points.append(calligraphicPoint);
    appendPointToPath(calligraphicPoint);

    // make the angle of the first point more in line with the actual
    // direction
    if (s->points.count() == 4) {
        s->points[0].setAngle(angle);
        s->points[1].setAngle(angle);
        s->points[2].setAngle(angle);
    }

    normalize();
}

void KarbonCalligraphicShape::appendPointToPath(const KarbonCalligraphicPoint &p)
{
    qreal dx = std::cos(p.angle()) * p.width();
    qreal dy = std::sin(p.angle()) * p.width();

    // find the outline points
    PkPointF p1 = p.point() - PkPointF(dx / 2, dy / 2);
    PkPointF p2 = p.point() + PkPointF(dx / 2, dy / 2);

    if (pointCount() == 0) {
        moveTo(p1);
        lineTo(p2);
        return;
    }
    // pointCount > 0

    bool flip = (pointCount() >= 2) ? flipDetected(p1, p2) : false;

    // if there was a flip add additional points
    if (flip) {
        appendPointsToPathAux(p2, p1);
        if (pointCount() > 4) {
            smoothLastPoints();
        }
    }

    appendPointsToPathAux(p1, p2);

    if (pointCount() > 4) {
        smoothLastPoints();

        if (flip) {
            int index = pointCount() / 2;
            // find the last two points
            KoPathPoint *last1 = pointByIndex(KoPathPointIndex(0, index - 1));
            KoPathPoint *last2 = pointByIndex(KoPathPointIndex(0, index));

            last1->removeControlPoint1();
            last1->removeControlPoint2();
            last2->removeControlPoint1();
            last2->removeControlPoint2();
            s->lastWasFlip = true;
        }

        if (s->lastWasFlip) {
            int index = pointCount() / 2;
            // find the previous two points
            KoPathPoint *prev1 = pointByIndex(KoPathPointIndex(0, index - 2));
            KoPathPoint *prev2 = pointByIndex(KoPathPointIndex(0, index + 1));

            prev1->removeControlPoint1();
            prev1->removeControlPoint2();
            prev2->removeControlPoint1();
            prev2->removeControlPoint2();

            if (!flip) {
                s->lastWasFlip = false;
            }
        }
    }

    // add initial cap if it's the fourth added point
    // this code is here because this function is called from different places
    // pointCount() == 8 may causes crashes because it doesn't take possible
    // flips into account

    if (s->points.count() >= 4 && p == s->points[3]) {
       addCap(3, 0, 0, true);
        // duplicate the last point to make the points remain "balanced"
        // needed to keep all indexes code (else I would need to change
        // everything in the code...)
        KoPathPoint *last = pointByIndex(KoPathPointIndex(0, pointCount() - 1));
        KoPathPoint *newPoint = new KoPathPoint(this, last->point());
        insertPoint(newPoint, KoPathPointIndex(0, pointCount()));
        close();
    }
}

void KarbonCalligraphicShape::appendPointsToPathAux(const PkPointF &p1, const PkPointF &p2)
{
    KoPathPoint *pathPoint1 = new KoPathPoint(this, p1);
    KoPathPoint *pathPoint2 = new KoPathPoint(this, p2);

    // calculate the index of the insertion position
    int index = pointCount() / 2;

    insertPoint(pathPoint2, KoPathPointIndex(0, index));
    insertPoint(pathPoint1, KoPathPointIndex(0, index));
}

void KarbonCalligraphicShape::smoothLastPoints()
{
    int index = pointCount() / 2;
    smoothPoint(index - 2);
    smoothPoint(index + 1);
}

void KarbonCalligraphicShape::smoothPoint(const int index)
{
    if (pointCount() < index + 2) {
        return;
    } else if (index < 1) {
        return;
    }

    const KoPathPointIndex PREV(0, index - 1);
    const KoPathPointIndex INDEX(0, index);
    const KoPathPointIndex NEXT(0, index + 1);

    PkPointF prev = pointByIndex(PREV)->point();
    PkPointF point = pointByIndex(INDEX)->point();
    PkPointF next = pointByIndex(NEXT)->point();

    PkPointF vector = next - prev;
    qreal dist = (PkLineF(prev, next)).length();
    // normalize the vector (make it's size equal to 1)
    if (!qFuzzyCompare(dist + 1, 1)) {
        vector /= dist;
    }
    qreal mult = 0.35; // found by trial and error, might not be perfect...
    // distance of the control points from the point
    qreal dist1 = (PkLineF(point, prev)).length() * mult;
    qreal dist2 = (PkLineF(point, next)).length() * mult;
    PkPointF vector1 = vector * dist1;
    PkPointF vector2 = vector * dist2;
    PkPointF controlPoint1 = point - vector1;
    PkPointF controlPoint2 = point + vector2;

    pointByIndex(INDEX)->setControlPoint1(controlPoint1);
    pointByIndex(INDEX)->setControlPoint2(controlPoint2);
}

const PkRectF KarbonCalligraphicShape::lastPieceBoundingRect()
{
    if (pointCount() < 6) {
        return PkRectF();
    }

    int index = pointCount() / 2;

    PkPointF p1 = pointByIndex(KoPathPointIndex(0, index - 3))->point();
    PkPointF p2 = pointByIndex(KoPathPointIndex(0, index - 2))->point();
    PkPointF p3 = pointByIndex(KoPathPointIndex(0, index - 1))->point();
    PkPointF p4 = pointByIndex(KoPathPointIndex(0, index))->point();
    PkPointF p5 = pointByIndex(KoPathPointIndex(0, index + 1))->point();
    PkPointF p6 = pointByIndex(KoPathPointIndex(0, index + 2))->point();

    // TODO: also take the control points into account
    PkPainterPath p;
    p.moveTo(p1);
    p.lineTo(p2);
    p.lineTo(p3);
    p.lineTo(p4);
    p.lineTo(p5);
    p.lineTo(p6);

    return p.boundingRect().translated(position());
}

bool KarbonCalligraphicShape::flipDetected(const PkPointF &p1, const PkPointF &p2)
{
    // detect the flip caused by the angle changing 180 degrees
    // thus detect the boundary crossing
    int index = pointCount() / 2;
    PkPointF last1 = pointByIndex(KoPathPointIndex(0, index - 1))->point();
    PkPointF last2 = pointByIndex(KoPathPointIndex(0, index))->point();

    int sum1 = std::abs(ccw(p1, p2, last1) + ccw(p1, last2, last1));
    int sum2 = std::abs(ccw(p2, p1, last2) + ccw(p2, last1, last2));
    // if there was a flip
    return sum1 < 2 && sum2 < 2;
}

int KarbonCalligraphicShape::ccw(const PkPointF &p1, const PkPointF &p2,const PkPointF &p3)
{
    // calculate two times the area of the triangle formed by the points given
    qreal area2 = (p2.x() - p1.x()) * (p3.y() - p1.y()) -
                  (p2.y() - p1.y()) * (p3.x() - p1.x());
    if (area2 > 0) {
        return +1; // the points are given in counterclockwise order
    } else if (area2 < 0) {
        return -1; // the points are given in clockwise order
    } else {
        return 0; // the points form a degenerate triangle
    }
}

void KarbonCalligraphicShape::setSize(const PkSizeF &newSize)
{
    // PkSizeF oldSize = size();
    // TODO: check
    KoParameterShape::setSize(newSize);
}

PkPointF KarbonCalligraphicShape::normalize()
{
    PkPointF offset(KoParameterShape::normalize());
    PkTransform matrix;
    matrix.translate(-offset.x(), -offset.y());

    for (int i = 0; i < s->points.size(); ++i) {
        s->points[i].setPoint(matrix.map(s->points[i].point()));
    }

    return offset;
}

void KarbonCalligraphicShape::moveHandleAction(int handleId,
        const PkPointF &point,
        Qt::KeyboardModifiers modifiers)
{
    (void)modifiers;
    s->points[handleId].setPoint(point);
}

void KarbonCalligraphicShape::updatePath(const PkSizeF &size)
{
    (void)size;

    PkPointF pos = position();

    // remove all points
    clear();
    setPosition(PkPoint(0, 0));

    for (const KarbonCalligraphicPoint &p : s->points) {
        appendPointToPath(p);
    }

    PkList<PkPointF> handles;
    for (const KarbonCalligraphicPoint &p : s->points) {
        handles.append(p.point());
    }
    setHandles(handles);

    setPosition(pos);
    normalize();
}

void KarbonCalligraphicShape::simplifyPath()
{
    if (s->points.count() < 2) {
        return;
    }

    close();

    // add final cap
    addCap(s->points.count() - 2, s->points.count() - 1, pointCount() / 2);

    // TODO: the error should be proportional to the width
    //       and it shouldn't be a magic number
    karbonSimplifyPath(this, 0.3);
}

void KarbonCalligraphicShape::addCap(int index1, int index2, int pointIndex, bool inverted)
{
    PkPointF p1 = s->points[index1].point();
    PkPointF p2 = s->points[index2].point();

    // TODO: review why spikes can appear with a lower limit
    PkPointF delta = p2 - p1;
    if (delta.manhattanLength() < 1.0) {
        return;
    }

    PkPointF direction = PkLineF(PkPointF(0, 0), delta).unitVector().p2();
    qreal width = s->points[index2].width();
    PkPointF p = p2 + direction * s->caps * width;

    KoPathPoint *newPoint = new KoPathPoint(this, p);

    qreal angle = s->points[index2].angle();
    if (inverted) {
        angle += M_PI;
    }

    qreal dx = std::cos(angle) * width;
    qreal dy = std::sin(angle) * width;
    newPoint->setControlPoint1(PkPointF(p.x() - dx / 2, p.y() - dy / 2));
    newPoint->setControlPoint2(PkPointF(p.x() + dx / 2, p.y() + dy / 2));

    insertPoint(newPoint, KoPathPointIndex(0, pointIndex));
}

PkString KarbonCalligraphicShape::pathShapeId() const
{
    return KarbonCalligraphicShapeId;
}

void KarbonCalligraphicShape::simplifyGuidePath()
{
    // do not attempt to simplify if there are too few points
    if (s->points.count() < 3) {
        return;
    }

    PkList<PkPointF> points;
    for (const KarbonCalligraphicPoint &p : s->points) {
        points.append(p.point());
    }

    // cumulative data used to determine if the point can be removed
    qreal widthChange = 0;
    qreal directionChange = 0;
    PkList<KarbonCalligraphicPoint>::iterator i = s->points.begin() + 2;

    while (i != std::prev(s->points.end())) {
        PkPointF point = i->point();

        qreal width = i->width();
        qreal prevWidth = std::prev(i)->width();
        qreal widthDiff = width - prevWidth;
        widthDiff /= qMax(width, prevWidth);

        qreal directionDiff = 0;
        if (std::next(i) != s->points.end()) {
            PkPointF prev = std::prev(i)->point();
            PkPointF next = std::next(i)->point();

            directionDiff = PkLineF(prev, point).angleTo(PkLineF(point, next));
            if (directionDiff > 180) {
                directionDiff -= 360;
            }
        }

        if (directionChange * directionDiff >= 0 &&
                qAbs(directionChange + directionDiff) < 20 &&
                widthChange * widthDiff >= 0 &&
                qAbs(widthChange + widthDiff) < 0.1) {
            // deleted point
            i = s->points.erase(i);
            directionChange += directionDiff;
            widthChange += widthDiff;
        } else {
            // keep point
            directionChange = 0;
            widthChange = 0;
            ++i;
        }
    }

    updatePath(PkSizeF());
}
