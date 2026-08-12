/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "DodgeBurn.h"
#include <filter/kis_filter_category_ids.h>
#include <filter/kis_color_transformation_configuration.h>
#include <kis_paint_device.h>
#include <KisGlobalResourcesInterface.h>

KisFilterDodgeBurn::KisFilterDodgeBurn(const QString& id, const QString& prefix, const QString& name ) : KisColorTransformationFilter(KoID(id, name), FiltersCategoryAdjustId, name), m_prefix(prefix)
{
    setColorSpaceIndependence(FULLY_INDEPENDENT);
    setSupportsPainting(true);
}

KisFilterConfigurationSP KisFilterDodgeBurn::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("exposure", 0.5);
    config->setProperty("type", KisFilterDodgeBurn::MIDTONES);
    return config;
}

KoColorTransformation* KisFilterDodgeBurn::createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const
{
    QHash<QString, QVariant> params;
    QString suffix = "Midtones";
    if (config) {
        params["exposure"] = config->getDouble("exposure", 0.5);
        int type = config->getInt("type", KisFilterDodgeBurn::MIDTONES);
        switch(type)
          {
            case KisFilterDodgeBurn::HIGHLIGHTS:
              suffix = "Highlights";
              break;
            case KisFilterDodgeBurn::SHADOWS:
              suffix = "Shadows";
              break;
            default:
              break;
          }
    }
    return cs->createColorTransformation(m_prefix + suffix, params);

}

