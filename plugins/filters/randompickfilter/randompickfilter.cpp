/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2005 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "randompickfilter.h"
#include <stdlib.h>
#include <vector>
#include <math.h>

#include <PkPoint.h>

#include <kis_debug.h>


#include <KoUpdater.h>

#include <KoMixColorsOp.h>
#include <filter/kis_filter_registry.h>
#include <kis_global.h>
#include <kis_image.h>
#include <kis_layer.h>
#include <kis_paint_device.h>
#include <kis_random_accessor_ng.h>
#include <KisRandomGenerator2D.h>
#include <kis_selection.h>
#include <kis_types.h>
#include <filter/kis_filter_category_ids.h>
#include <filter/kis_filter_configuration.h>
#include <kis_processing_information.h>

#include <kis_iterator_ng.h>
#include <KisSequentialIteratorProgress.h>

namespace {
struct KritaRandomPickFilterFilterRegistration
{
    KritaRandomPickFilterFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisFilterRandomPick());
    }
};
} // namespace
static KritaRandomPickFilterFilterRegistration s_kritaRandomPickFilterFilterRegistration;


KisFilterRandomPick::KisFilterRandomPick() : KisFilter(id(), FiltersCategoryOtherId, PkString("&Random Pick..."))
{
    setColorSpaceIndependence(FULLY_INDEPENDENT);
    setSupportsPainting(true);
}


void KisFilterRandomPick::processImpl(KisPaintDeviceSP device,
                                      const PkRect& applyRect,
                                      const KisFilterConfigurationSP config,
                                      KoUpdater* progressUpdater
                                      ) const
{
    Q_UNUSED(config);
    Q_ASSERT(!device.isNull());

    const KoColorSpace * cs = device->colorSpace();

    PkVariant value;
    int level = (config && config->getProperty("level", value)) ? value.toInt() : 50;
    int opacity = (config && config->getProperty("opacity", value)) ? value.toInt() : 100;
    double windowsize = (config && config->getProperty("windowsize", value)) ? value.toDouble() : 2.5;

    int seedThreshold = rand();
    int seedH = rand();
    int seedV = rand();

    if (config) {
        seedThreshold = config->getInt("seedThreshold", seedThreshold);
        seedH = config->getInt("seedH", seedH);
        seedV = config->getInt("seedV", seedV);
    }
    KisRandomGenerator2D randT(seedThreshold);
    KisRandomGenerator2D randH(seedH);
    KisRandomGenerator2D randV(seedV);

    KisSequentialIteratorProgress dstIt(device, applyRect, progressUpdater);
    KisRandomConstAccessorSP srcRA = device->createRandomConstAccessorNG();

    double threshold = (100 - level) / 100.0;

    qint16 weights[2];
    weights[0] = (255 * opacity) / 100; weights[1] = 255 - weights[0];
    const quint8* pixels[2];
    KoMixColorsOp * mixOp = cs->mixColorsOp();
    while (dstIt.nextPixel()) {
        if (randT.doubleRandomAt(dstIt.x(), dstIt.y()) > threshold) {
            int x = static_cast<int>(dstIt.x() + windowsize * (randH.doubleRandomAt(dstIt.x(), dstIt.y()) - 0.5));
            int y = static_cast<int>(dstIt.y() +  windowsize * (randV.doubleRandomAt(dstIt.x(), dstIt.y()) -0.5));
            srcRA->moveTo(x, y);
            pixels[0] = srcRA->oldRawData();
            pixels[1] = dstIt.oldRawData();
            mixOp->mixColors(pixels, weights, 2, dstIt.rawData());
        }
    }

}

KisFilterConfigurationSP KisFilterRandomPick::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("level", 50);
    config->setProperty("windowsize", 2.5);
    config->setProperty("opacity", 100);
    config->setProperty("seedThreshold", rand());
    config->setProperty("seedH", rand());
    config->setProperty("seedV", rand());

    return config;
}

PkRect KisFilterRandomPick::neededRect(const PkRect& rect, const KisFilterConfigurationSP config, int lod) const
{
    Q_UNUSED(lod);

    PkVariant value;
    int windowsize = ceil((config && config->getProperty("windowsize", value)) ? value.toDouble() : 2.5);
    return rect.adjusted(-windowsize, -windowsize, windowsize, windowsize);
}

PkRect KisFilterRandomPick::changedRect(const PkRect &rect, const KisFilterConfigurationSP config, int lod) const
{
    return neededRect(rect, config, lod);
}
