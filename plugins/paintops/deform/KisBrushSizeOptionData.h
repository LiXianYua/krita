/*
 *  SPDX-FileCopyrightText: 2009, 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_BRUSHSIZE_OPTION_DATA_H
#define KIS_BRUSHSIZE_OPTION_DATA_H


#include "kis_types.h"
#include <boost/operators.hpp>

class KisPropertiesConfiguration;

struct KisBrushSizeOptionData : boost::equality_comparable<KisBrushSizeOptionData>
{
    inline friend bool operator==(const KisBrushSizeOptionData &lhs, const KisBrushSizeOptionData &rhs) {
        return pkQtFuzzyCompare(lhs.brushDiameter, rhs.brushDiameter)
            && pkQtFuzzyCompare(lhs.brushAspect, rhs.brushAspect)
            && pkQtFuzzyCompare(lhs.brushRotation, rhs.brushRotation)
            && pkQtFuzzyCompare(lhs.brushScale, rhs.brushScale)
            && pkQtFuzzyCompare(lhs.brushSpacing, rhs.brushSpacing)
            && pkQtFuzzyCompare(lhs.brushDensity, rhs.brushDensity)
            && pkQtFuzzyCompare(lhs.brushJitterMovement, rhs.brushJitterMovement)
            && lhs.brushJitterMovementEnabled == rhs.brushJitterMovementEnabled;
    }

    qreal brushDiameter {20.0};
    qreal brushAspect {1.0};
    qreal brushRotation {0.0};
    qreal brushScale {1.0};
    qreal brushSpacing {0.3};
    qreal brushDensity {1.0};
    qreal brushJitterMovement {0.0};
    bool brushJitterMovementEnabled {false};

    bool read(const KisPropertiesConfiguration *setting);
    void write(KisPropertiesConfiguration *setting) const;
};

#endif // KIS_BRUSHSIZE_OPTION_DATA_H
