/*
 *  SPDX-FileCopyrightText: 2013 Sahil Nagpal <nagpal.sahil01@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KIS_BURN_MIDTONES_ADJUSTMENT_H_
#define _KIS_BURN_MIDTONES_ADJUSTMENT_H_

#include "KoColorTransformationFactory.h"

class KisBurnMidtonesAdjustmentFactory : public KoColorTransformationFactory
{
public:

    KisBurnMidtonesAdjustmentFactory();

    PkList< std::pair< KoID, KoID > > supportedModels() const override;

    KoColorTransformation* createTransformation(const KoColorSpace* colorSpace, PkHash<PkString, PkVariant> parameters) const override;

};

#endif
