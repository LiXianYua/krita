/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_desaturate_filter.h"


#include <math.h>

#include <stdlib.h>
#include <string.h>

#include <QSlider>
#include <QPoint>
#include <QColor>

#include <klocalizedstring.h>

#include <kis_debug.h>
#include <kpluginfactory.h>

#include "KoBasicHistogramProducers.h"
#include <KoColorSpace.h>
#include <KoColorTransformation.h>
#include <filter/kis_filter_category_ids.h>
#include <filter/kis_color_transformation_configuration.h>
#include <kis_paint_device.h>
#include <kis_processing_information.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_global.h>
#include <kis_types.h>
#include <kis_selection.h>
#include "filter/kis_filter_registry.h"
#include <kis_painter.h>
#include <KoColorSpaceConstants.h>
#include <KoCompositeOp.h>
#include <kis_iterator_ng.h>
#include <KisGlobalResourcesInterface.h>

KisDesaturateFilter::KisDesaturateFilter()
   : KisColorTransformationFilter(id(), FiltersCategoryAdjustId, i18n("&Desaturate..."))
{
    setShortcut(QKeySequence(Qt::CTRL, Qt::SHIFT, Qt::Key_U));
    setSupportsPainting(true);
}

KisDesaturateFilter::~KisDesaturateFilter()
{
}

KoColorTransformation* KisDesaturateFilter::createTransformation(const KoColorSpace* cs, const KisFilterConfigurationSP config) const
{
    QHash<QString, QVariant> params;
    if (config) {
        params["type"] = config->getInt("type", 0);
    }
    return  cs->createColorTransformation("desaturate_adjustment", params);
}

KisFilterConfigurationSP KisDesaturateFilter::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("type", 0);
    return config;
}
