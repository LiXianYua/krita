/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ConcentricEllipseAssistantGeometry.h"

#include "Ellipse.h"

#include <PkLine.h>

PkPointF ConcentricEllipseAssistantGeometry::project(const PkList<PkPointF> &handles,
                                                    const PkPointF &point,
                                                    const PkPointF &strokeBegin)
{
    assert(handles.size() >= 3);

    Ellipse ellipse;
    ellipse.set(handles[0], handles[1], handles[2]);

    const PkPointF initial = ellipse.project(strokeBegin);
    const PkPointF center = ellipse.boundingRect().center();
    const qreal ratio = PkLineF(center, strokeBegin).length()
        / PkLineF(center, initial).length();

    PkLineF extrapolate0(center, handles[0]);
    extrapolate0.setLength(extrapolate0.length() * ratio);
    PkLineF extrapolate1(center, handles[1]);
    extrapolate1.setLength(extrapolate1.length() * ratio);
    PkLineF extrapolate2(center, handles[2]);
    extrapolate2.setLength(extrapolate2.length() * ratio);

    Ellipse extraEllipse;
    extraEllipse.set(extrapolate0.p2(), extrapolate1.p2(), extrapolate2.p2());
    return extraEllipse.project(point);
}

void ConcentricEllipseAssistantGeometry::adjustLine(const PkList<PkPointF> &handles,
                                                    PkPointF &point,
                                                    const PkPointF &strokeBegin)
{
    point = project(handles, point, strokeBegin);
}
