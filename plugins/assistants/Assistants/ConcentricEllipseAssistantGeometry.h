/*
 * SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 * SPDX-FileCopyrightText: 2010 Geoffry Song <goffrie@gmail.com>
 * SPDX-FileCopyrightText: 2017 Scott Petrovic <scottpetrovic@gmail.com>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef CONCENTRIC_ELLIPSE_ASSISTANT_GEOMETRY_H
#define CONCENTRIC_ELLIPSE_ASSISTANT_GEOMETRY_H

#include "kritaassistanttool_export.h"

#include <QList>
#include <QPointF>

class KRITAASSISTANTTOOL_EXPORT ConcentricEllipseAssistantGeometry
{
public:
    static QPointF project(const QList<QPointF> &handles,
                           const QPointF &point,
                           const QPointF &strokeBegin);

    static void adjustLine(const QList<QPointF> &handles,
                           QPointF &point,
                           const QPointF &strokeBegin);
};

#endif
