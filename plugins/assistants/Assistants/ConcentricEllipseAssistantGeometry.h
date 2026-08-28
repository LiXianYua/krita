/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef CONCENTRIC_ELLIPSE_ASSISTANT_GEOMETRY_H
#define CONCENTRIC_ELLIPSE_ASSISTANT_GEOMETRY_H

#include "kritaassistanttool_export.h"

#include <PkList.h>
#include <PkPoint.h>

class KRITAASSISTANTTOOL_EXPORT ConcentricEllipseAssistantGeometry
{
public:
    static PkPointF project(const PkList<PkPointF> &handles,
                           const PkPointF &point,
                           const PkPointF &strokeBegin);

    static void adjustLine(const PkList<PkPointF> &handles,
                           PkPointF &point,
                           const PkPointF &strokeBegin);
};

#endif
