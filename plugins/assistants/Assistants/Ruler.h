/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _RULER_H_
#define _RULER_H_

#include <PkPoint.h>

class Ruler
{
public:
    Ruler();
    ~Ruler();
    PkPointF project(const PkPointF&);
    const PkPointF& point1() const;
    void setPoint1(const PkPointF& p) {
        p1 = p;
    }
    const PkPointF& point2() const;
    void setPoint2(const PkPointF& p) {
        p2 = p;
    }
private:
    PkPointF p1;
    PkPointF p2;
};

#endif
