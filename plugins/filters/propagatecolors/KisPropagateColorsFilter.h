/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2024 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PROPAGATE_COLORS_FILTER_H
#define KIS_PROPAGATE_COLORS_FILTER_H

#include <filter/kis_filter.h>
#include <kis_filter_configuration.h>


class KisPropagateColorsFilter : public KisFilter
{
public:
    KisPropagateColorsFilter();

    void processImpl(KisPaintDeviceSP device,
                     const PkRect& applyRect,
                     const KisFilterConfigurationSP config,
                     KoUpdater *progressUpdater) const override;

    KisFilterConfigurationSP factoryConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
    bool needsTransparentPixels(const KisFilterConfigurationSP config, const KoColorSpace *cs) const override;
    PkRect neededRect(const PkRect &rect, const KisFilterConfigurationSP config, int lod) const override;
    PkRect changedRect(const PkRect &rect, const KisFilterConfigurationSP config, int lod) const override;
};

#endif
