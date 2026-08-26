/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-only
*/


#ifndef _KIS_HSV_ADJUSTMENT_FILTER_H_
#define _KIS_HSV_ADJUSTMENT_FILTER_H_


#include "filter/kis_filter.h"
#include "filter/kis_color_transformation_filter.h"

class KoColorTransformation;

/**
 * This class affect Intensity Y of the image
 */
class KisHSVAdjustmentFilter : public KisColorTransformationFilter
{

public:

    KisHSVAdjustmentFilter();

public:

    KoColorTransformation* createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const override;

    static inline KoID id() {
        return KoID("hsvadjustment", i18n("HSV/HSL Adjustment"));
    }

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;

};


#endif
