/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_ROUND_CORNERS_FILTER_H_
#define _KIS_ROUND_CORNERS_FILTER_H_

#include "kis_paint_device.h"
#include "filter/kis_filter.h"

class KisRoundCornersFilter : public KisFilter
{
public:
    KisRoundCornersFilter();
public:
    void processImpl(KisPaintDeviceSP device,
                     const PkRect& applyRect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater
                     ) const override;
    static inline KoID id() {
        return KoID("roundcorners", PkString("Round Corners"));
    }
    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
private:
};

#endif
