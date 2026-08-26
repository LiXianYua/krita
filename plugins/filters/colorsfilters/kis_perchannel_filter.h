/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef _KIS_PERCHANNEL_FILTER_H_
#define _KIS_PERCHANNEL_FILTER_H_


#include <filter/kis_color_transformation_filter.h>
#include <filter/kis_color_transformation_configuration.h>
#include <kis_paint_device.h>

#include "virtual_channel_info.h"

#include "kis_multichannel_filter_base.h"


class KisPerChannelFilterConfiguration
        : public KisMultiChannelFilterConfiguration
{
public:
    KisPerChannelFilterConfiguration(int channelCount, KisResourcesInterfaceSP resourcesInterface);
    KisPerChannelFilterConfiguration(const KisPerChannelFilterConfiguration &rhs);
    ~KisPerChannelFilterConfiguration() override;

    KisFilterConfigurationSP clone() const override;

    KisCubicCurve getDefaultCurve() override;
};


/**
 * This class is a filter to adjust channels independently
 */
class KisPerChannelFilter : public KisMultiChannelFilter
{
public:
    KisPerChannelFilter();

    KisFilterConfigurationSP factoryConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;

    KoColorTransformation* createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const override;

    static inline KoID id() {
        return KoID("perchannel", PkString("Color Adjustment"));
    }
};

#endif
