/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MYPAINTCURVERANGEMODEL_H
#define MYPAINTCURVERANGEMODEL_H

#include <tuple>

#include <PkGlobal.h>
#include <PkString.h>
#include <PkRect.h>

class MyPaintCurveRangeModel
{
public:
    struct NormalizedCurve {
        PkString curve;
        qreal xMin = 0.0;
        qreal xMax = 1.0;
        qreal yLimit = 1.0;
    };

public:
    static std::tuple<PkString, PkRectF> reshapeCurve(std::tuple<PkString, PkRectF> curve);
};

#endif // MYPAINTCURVERANGEMODEL_H
