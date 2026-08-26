/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2010 Edward Apap <schumifer@hotmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef KIS_LENS_BLUR_FILTER_H
#define KIS_LENS_BLUR_FILTER_H

#include "filter/kis_filter.h"

#include <Eigen/Core>

class KisLensBlurFilter : public KisFilter
{
public:
    KisLensBlurFilter();
public:

    void processImpl(KisPaintDeviceSP src,
                     const PkRect& size,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater
                     ) const override;
    static inline KoID id() {
        return KoID("lens blur", PkString("Lens Blur"));
    }

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;

    static PkSize getKernelHalfSize(const KisFilterConfigurationSP config, int lod);

    PkRect neededRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;
    PkRect changedRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;

private:
    static PkPolygonF getIrisPolygon(const KisFilterConfigurationSP config, int lod);
};

#endif
