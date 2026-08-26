/*
  a *  SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
  *
  *  SPDX-License-Identifier: GPL-2.0-or-later
  */
#ifndef KISSIMPLENOISEREDUCER_H
#define KISSIMPLENOISEREDUCER_H

#include <filter/kis_filter.h>
/**
   @author Cyrille Berger
*/

class KisSimpleNoiseReducer : public KisFilter
{
public:
    KisSimpleNoiseReducer();
    ~KisSimpleNoiseReducer() override;
public:

    void processImpl(KisPaintDeviceSP device,
                     const PkRect& applyRect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater
                     ) const override;

    static inline KoID id() {
        return KoID("gaussiannoisereducer", PkString("Gaussian Noise Reducer"));
    }

    PkRect changedRect(const PkRect &rect, const KisFilterConfigurationSP _config, int lod) const override;
    PkRect neededRect(const PkRect &rect, const KisFilterConfigurationSP _config, int lod) const override;

protected:
    KisFilterConfigurationSP  defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
};

#endif
