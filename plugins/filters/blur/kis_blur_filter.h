/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_BLUR_FILTER_H
#define KIS_BLUR_FILTER_H

#include "filter/kis_filter.h"

class KisBlurFilter : public KisFilter
{
public:
    KisBlurFilter();
public:

    void processImpl(KisPaintDeviceSP device,
                     const PkRect& size,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater
                     ) const override;
    static inline KoID id() {
        return KoID("blur", i18n("Blur"));
    }

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
public:
    PkRect neededRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;
    PkRect changedRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;
};

#endif
