/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_GRADIENT_SHAPE_STRATEGY_H
#define __KIS_GRADIENT_SHAPE_STRATEGY_H

#include <PkPoint.h>

#include "kritaimage_export.h"

class KRITAIMAGE_EXPORT KisGradientShapeStrategy
{
public:
    KisGradientShapeStrategy();
    KisGradientShapeStrategy(const PkPointF& gradientVectorStart, const PkPointF& gradientVectorEnd);
    virtual ~KisGradientShapeStrategy();

    virtual double valueAt(double x, double y) const = 0;

protected:
    PkPointF m_gradientVectorStart;
    PkPointF m_gradientVectorEnd;
};

#endif /* __KIS_GRADIENT_SHAPE_STRATEGY_H */
