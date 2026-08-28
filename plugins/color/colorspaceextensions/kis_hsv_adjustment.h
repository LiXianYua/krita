/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-only
*/

#ifndef _KIS_HSV_ADJUSTMENT_H_
#define _KIS_HSV_ADJUSTMENT_H_

#include "KoColorTransformationFactory.h"

class KisHSVAdjustmentFactory : public KoColorTransformationFactory
{
public:

    KisHSVAdjustmentFactory();

    PkList< std::pair< KoID, KoID > > supportedModels() const override;

    KoColorTransformation* createTransformation(const KoColorSpace* colorSpace, PkHash<PkString, PkVariant> parameters) const override;

};

class KisHSVCurveAdjustmentFactory : public KoColorTransformationFactory
{
public:

    KisHSVCurveAdjustmentFactory();

    PkList< std::pair< KoID, KoID > > supportedModels() const override;

    KoColorTransformation* createTransformation(const KoColorSpace* colorSpace, PkHash<PkString, PkVariant> parameters) const override;

};

namespace KisHSVCurve {
    enum ColorChannel {
        RGBChannel0 = 0,
        RGBChannel1 = 1,
        RGBChannel2 = 2,
        Alpha = 3,
        AllColors = 4,
        Hue = 5,
        Saturation = 6,
        Value = 7,
        ChannelCount
    };
}

#endif
