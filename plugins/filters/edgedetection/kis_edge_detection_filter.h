/*
 * SPDX-FileCopyrightText: 2017 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_EDGE_DETECTION_FILTER_H
#define KIS_EDGE_DETECTION_FILTER_H

#include "filter/kis_filter.h"

#include <Eigen/Core>


class KisEdgeDetectionFilter : public KisFilter
{
public:
    KisEdgeDetectionFilter();
    void processImpl(KisPaintDeviceSP device,
                     const PkRect& rect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater
                     ) const override;
    static inline KoID id() {
        return KoID("edge detection", PkString("Edge Detection"));
    }

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
public:
    PkRect neededRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;
    PkRect changedRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;
};

#endif // KIS_EDGE_DETECTION_FILTER_H
