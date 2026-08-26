/* This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2009 Edward Apap <schumifer@hotmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef KIS_GAUSSIAN_BLUR_FILTER_H
#define KIS_GAUSSIAN_BLUR_FILTER_H

#include "filter/kis_filter.h"

#include <Eigen/Core>

class KisGaussianBlurFilter : public KisFilter
{
public:
    KisGaussianBlurFilter();
public:
    void processImpl(KisPaintDeviceSP device,
                     const PkRect& rect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater
                     ) const override;
    static inline KoID id() {
        return KoID("gaussian blur", PkString("Gaussian Blur"));
    }

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
public:
    PkRect neededRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;
    PkRect changedRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;

    bool configurationAllowedForMask(KisFilterConfigurationSP config) const override;
    void fixLoadedFilterConfigurationForMasks(KisFilterConfigurationSP config) const override;
};

#endif
