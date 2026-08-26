/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_PIXELIZE_FILTER_H_
#define _KIS_PIXELIZE_FILTER_H_

#include "filter/kis_filter.h"

class KisPixelizeFilter : public KisFilter
{
public:
    KisPixelizeFilter();
public:

    void processImpl(KisPaintDeviceSP device,
                     const PkRect& applyRect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater) const override;

    static inline KoID id() {
        return KoID("pixelize", PkString("Pixelize"));
    }

    PkRect neededRect(const PkRect & rect, const KisFilterConfigurationSP config, int lod) const override;
    PkRect changedRect(const PkRect & rect, const KisFilterConfigurationSP config, int lod) const override;

public:
    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
};

#endif
