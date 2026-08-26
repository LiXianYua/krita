/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef _KIS_OILPAINT_FILTER_H_
#define _KIS_OILPAINT_FILTER_H_

#include "filter/kis_filter.h"

class KisOilPaintFilter : public KisFilter
{
public:
    KisOilPaintFilter();
public:

    void processImpl(KisPaintDeviceSP device,
                     const PkRect& applyRect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater ) const override;
    static inline KoID id() {
        return KoID("oilpaint", PkString("Oilpaint"));
    }


    PkRect neededRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;
    PkRect changedRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;

private:
    void OilPaint(const KisPaintDeviceSP src, KisPaintDeviceSP dst, const PkRect &applyRect,
                  int BrushSize, int Smoothness, KoUpdater* progressUpdater) const;
    void MostFrequentColor(KisPaintDeviceSP src, quint8* dst, const PkRect& bounds, int X, int Y, int Radius, int Intensity) const;
};

#endif
