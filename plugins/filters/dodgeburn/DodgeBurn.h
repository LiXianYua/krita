/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef DODGE_BURN_H
#define DODGE_BURN_H

#include "filter/kis_color_transformation_filter.h"


class KisFilterDodgeBurn : public KisColorTransformationFilter
{
public:
    enum Type {
      SHADOWS,
      MIDTONES,
      HIGHLIGHTS
    };
public:
    KisFilterDodgeBurn(const QString& id, const QString& prefix, const QString& name );
public:

    KoColorTransformation* createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const override;
    KisFilterConfigurationSP defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const override;
private:
    QString m_prefix;
};

#endif
