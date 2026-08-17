/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef KIS_DESATURATE_FILTER_H
#define KIS_DESATURATE_FILTER_H

#include <QObject>
#include <QVariant>

#include <filter/kis_color_transformation_filter.h>

class KoColorSpace;
class KoColorTransformation;

class KisDesaturateFilter : public KisColorTransformationFilter
{
public:
    KisDesaturateFilter();
    ~KisDesaturateFilter() override;
public:

    KoColorTransformation* createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const override;

    static inline KoID id() {
        return KoID("desaturate", i18n("Desaturate"));
    }

    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;

};


#endif
