/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef WAVEFILTER_H
#define WAVEFILTER_H

#include <QObject>
#include <PkVariant.h>
#include "filter/kis_filter.h"


class KritaWaveFilter : public QObject
{
    Q_OBJECT
public:
    KritaWaveFilter(QObject *parent, const PkVariantList &);
    ~KritaWaveFilter() override;
};

class KisFilterWave : public KisFilter
{
public:

    KisFilterWave();

public:

    void processImpl(KisPaintDeviceSP device,
                     const PkRect& applyRect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater) const override;
    static inline KoID id() {
        return KoID("wave", i18n("Wave"));
    }

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
public:
    PkRect changedRect(const PkRect& rect, const KisFilterConfigurationSP config = 0, int lod = 0) const override;
    PkRect neededRect(const PkRect& rect, const KisFilterConfigurationSP config = 0, int lod = 0) const override;
};

#endif
