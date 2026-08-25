/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_CACHED_GRADIENT_SHAPE_STRATEGY_H
#define __KIS_CACHED_GRADIENT_SHAPE_STRATEGY_H

#include "kis_gradient_shape_strategy.h"
#include <PkRect.h>
#include "kritaimage_export.h"

#include <PkScopedPointer.h>

class PkRect;


class KRITAIMAGE_EXPORT KisCachedGradientShapeStrategy : public KisGradientShapeStrategy
{
public:
    KisCachedGradientShapeStrategy(const PkRect &rc, qreal xStep, qreal yStep, KisGradientShapeStrategy *baseStrategy);
    ~KisCachedGradientShapeStrategy() override;

    double valueAt(double x, double y) const override;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_CACHED_GRADIENT_SHAPE_STRATEGY_H */
