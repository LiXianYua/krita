/*
 *  SPDX-FileCopyrightText: 2013 Sahil Nagpal <nagpal.sahil01@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "kis_color_balance_filter.h"
#include <filter/kis_filter_category_ids.h>
#include "filter/kis_color_transformation_configuration.h"
#include "kis_selection.h"
#include "kis_paint_device.h"
#include "kis_processing_information.h"
#include <KisGlobalResourcesInterface.h>

KisColorBalanceFilter::KisColorBalanceFilter() 
        : KisColorTransformationFilter(id(), FiltersCategoryAdjustId, i18n("&Color Balance..."))
{
    setShortcut(QKeySequence(Qt::CTRL | Qt::Key_B));
	setSupportsPainting(true);
}

KoColorTransformation * KisColorBalanceFilter::createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const
{
	QHash<QString, QVariant> params;
    if (config) {
        params["cyan_red_midtones"] = config->getInt("cyan_red_midtones", 0) * 0.01;
        params["magenta_green_midtones"] = config->getInt("magenta_green_midtones", 0) * 0.01;
        params["yellow_blue_midtones"] = config->getInt("yellow_blue_midtones", 0) * 0.01;

        params["cyan_red_shadows"] = config->getInt("cyan_red_shadows", 0) * 0.01;
        params["magenta_green_shadows"] = config->getInt("magenta_green_shadows", 0) * 0.01;
        params["yellow_blue_shadows"] = config->getInt("yellow_blue_shadows", 0) * 0.01;

        params["cyan_red_highlights"] = config->getInt("cyan_red_highlights", 0) * 0.01;
        params["magenta_green_highlights"] = config->getInt("magenta_green_highlights", 0) * 0.01;
        params["yellow_blue_highlights"] = config->getInt("yellow_blue_highlights", 0) * 0.01;
        params["preserve_luminosity"] = config->getBool("preserve_luminosity", true);

    }
    return cs->createColorTransformation("ColorBalance" , params);
}

KisFilterConfigurationSP KisColorBalanceFilter::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("cyan_red_midtones", 0);
    config->setProperty("yellow_green_midtones", 0);
    config->setProperty("magenta_blue_midtones", 0);

    config->setProperty("cyan_red_shadows", 0);
    config->setProperty("yellow_green_shadows", 0);
    config->setProperty("magenta_blue_shadows", 0);

    config->setProperty("cyan_red_highlights", 0);
    config->setProperty("yellow_green_highlights", 0);
    config->setProperty("magenta_blue_highlights", 0);
    config->setProperty("preserve_luminosity", true);

    return config;
}
