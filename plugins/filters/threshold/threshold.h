/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef THRESHOLD_H
#define THRESHOLD_H

#include <QObject>
#include <PkVariant.h>
#include <filter/kis_filter.h>
#include <kis_filter_configuration.h>

class WdgThreshold;
class QWidget;
class KisHistogram;



class KritaThreshold : public QObject
{
    Q_OBJECT
public:
    KritaThreshold(QObject *parent, const PkVariantList &);
    ~KritaThreshold() override;
};

class KisFilterThreshold : public KisFilter
{
public:
    KisFilterThreshold();
public:

    static inline KoID id() {
        return KoID("threshold", i18n("Threshold"));
    }

    void processImpl(KisPaintDeviceSP device,
                     const PkRect& applyRect,
                     const KisFilterConfigurationSP config,
                     KoUpdater *progressUpdater) const override;

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;

};


#endif

