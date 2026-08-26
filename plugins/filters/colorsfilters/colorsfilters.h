/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef COLORSFILTERS_H
#define COLORSFILTERS_H

#include "kis_perchannel_filter.h"
#include "filter/kis_color_transformation_filter.h"


class KisAutoContrast : public KisFilter
{
public:
    KisAutoContrast();
public:

    void processImpl(KisPaintDeviceSP device,
                     const PkRect& applyRect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater
                     ) const override;
    static inline KoID id() {
        return KoID("autocontrast", PkString("Auto Contrast"));
    }

};


#endif
