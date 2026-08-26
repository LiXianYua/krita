/*
 * SPDX-FileCopyrightText: 2017 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_CONVERT_HEIGHT_TO_NORMAL_MAP_FILTER_H
#define KIS_CONVERT_HEIGHT_TO_NORMAL_MAP_FILTER_H

#include "filter/kis_filter.h"

class KritaConvertHeightToNormalMapFilter : public QObject
{
    Q_OBJECT
public:
    KritaConvertHeightToNormalMapFilter(QObject *parent, const PkVariantList &);
    ~KritaConvertHeightToNormalMapFilter() override;
};

class KisConvertHeightToNormalMapFilter : public KisFilter
{
public:
    KisConvertHeightToNormalMapFilter();
    void processImpl(KisPaintDeviceSP device,
                     const PkRect& rect,
                     const KisFilterConfigurationSP config,
                     KoUpdater* progressUpdater
                     ) const override;
    static inline KoID id() {
        return KoID("height to normal", i18n("Height to Normal Map"));
    }

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
public:
    PkRect neededRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;
    PkRect changedRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const override;
};


#endif // KIS_CONVERT_HEIGHT_TO_NORMAL_MAP_FILTER_H
