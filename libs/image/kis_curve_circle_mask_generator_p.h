/*
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_CURVE_CIRCLE_MASK_GENERATOR_P_H
#define KIS_CURVE_CIRCLE_MASK_GENERATOR_P_H

#include "kis_antialiasing_fade_maker.h"
#include <PkContainerAlgo.h>
#include <PkScopedPointer.h>
#include "kis_brush_mask_applicator_base.h"
#include "kis_cubic_curve.h"

struct KisCurveCircleMaskGenerator::Private
{
    Private(bool enableAntialiasing)
        : fadeMaker(*this, enableAntialiasing)
    {
    }

    Private(const Private &rhs)
        : xcoef(rhs.xcoef),
        ycoef(rhs.ycoef),
        curveResolution(rhs.curveResolution),
        curveData(rhs.curveData),
        curvePoints(rhs.curvePoints),
        dirty(true),
        fadeMaker(rhs.fadeMaker,*this)
    {
    }

    qreal xcoef {0.0};
    qreal ycoef {0.0};
    qreal curveResolution {0.0};
    PkVector<qreal> curveData;
    PkList<KisCubicCurvePoint> curvePoints;
    bool dirty {false};

    KisAntialiasingFadeMaker1D<Private> fadeMaker;
    PkScopedPointer<KisBrushMaskApplicatorBase> applicator;

    inline quint8 value(qreal dist) const;
};

#endif // KIS_CURVE_CIRCLE_MASK_GENERATOR_P_H
