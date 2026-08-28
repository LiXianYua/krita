/*
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _ELLIPSE_H_
#define _ELLIPSE_H_

#include <PkPoint.h>
#include <PkTransform.h>

class Ellipse
{
public:
    Ellipse();
    Ellipse(const PkPointF& p1, const PkPointF& p2, const PkPointF& p3);
    ~Ellipse();
    
    PkPointF project(const PkPointF&) const; // find a close point on the ellipse
    PkRectF boundingRect() const; // find an axis-aligned box bounding this ellipse (inexact)
    
    bool set(const PkPointF& m1, const PkPointF& m2, const PkPointF& p); // set all points
    
    const PkPointF& major1() const { return p1; }
    bool setMajor1(const PkPointF& p);
    const PkPointF& major2() const { return p2; }
    bool setMajor2(const PkPointF& p);
    const PkPointF& point() const { return p3; }
    bool setPoint(const PkPointF& p);
    const PkTransform& getTransform() const { return matrix; }
    const PkTransform& getInverse() const { return inverse; }
    qreal semiMajor() const { return a; }
    qreal semiMinor() const { return b; }
    
private:
    bool changeMajor(); // determine 'a', 'b', 'matrix' and 'inverse'
    bool changeMinor(); // determine 'b'
    
    PkTransform matrix; // transformation turning p1, p2 and p3 into their corresponding points on the ellipse in canonical position
    PkTransform inverse; // inverse transformation
    qreal a; // semi-major axis: half the distance between p1 and p2 (horizontal axis)
    qreal b; // semi-minor axis (vertical axis)
    // a may not actually be larger than b, but we don't care that much
    
    PkPointF p1;
    PkPointF p2;
    PkPointF p3;
};

#endif
