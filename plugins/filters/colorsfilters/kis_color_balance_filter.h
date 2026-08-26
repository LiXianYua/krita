/*
 *  SPDX-FileCopyrightText: 2013 Sahil Nagpal <nagpal.sahil01@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef _KIS_COLOR_BALANCE_FILTER_H_
#define _KIS_COLOR_BALANCE_FILTER_H_

#include "filter/kis_filter.h"
#include "filter/kis_color_transformation_filter.h"


class KoColorTransformation;

class KisColorBalanceFilter : public  KisColorTransformationFilter
{

public:

	KisColorBalanceFilter();
public:
    enum Type {
      SHADOWS,
      MIDTONES,
      HIGHLIGHTS
    };

public:

    KoColorTransformation* createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const override;

	static inline KoID id() {
        return KoID("colorbalance", i18n("Color Balance"));
	}

    KisFilterConfigurationSP  defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;

};

#endif
