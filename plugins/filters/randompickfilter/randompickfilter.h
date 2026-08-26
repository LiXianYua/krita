/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RANDOMPICKFILTER_H
#define RANDOMPICKFILTER_H

#include <QObject>
#include <PkVariant.h>
#include "filter/kis_filter.h"


class KritaRandomPickFilter : public QObject
{
    Q_OBJECT
public:
    KritaRandomPickFilter(QObject *parent, const PkVariantList &);
    ~KritaRandomPickFilter() override;
};

class KisFilterRandomPick : public KisFilter
{
public:
    KisFilterRandomPick();
public:
    void processImpl(KisPaintDeviceSP device,
                     const PkRect& applyRect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater
                     ) const override;
    static inline KoID id() {
        return KoID("randompick", i18n("Random Pick"));
    }

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
public:
    PkRect neededRect(const PkRect& rect, const KisFilterConfigurationSP config, int lod = 0) const override;
    PkRect changedRect(const PkRect& rect, const KisFilterConfigurationSP config, int lod = 0) const override;
};

#endif
