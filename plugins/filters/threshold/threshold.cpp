/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2016 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "threshold.h"
#include <stdlib.h>
#include <vector>

#include <PkPoint.h>


#include <kis_debug.h>

#include <filter/kis_filter_category_ids.h>
#include <filter/kis_filter_registry.h>
#include <kis_global.h>
#include "kis_histogram.h"
#include <kis_layer.h>
#include "kis_paint_device.h"
#include "kis_painter.h"
#include <kis_processing_information.h>
#include <kis_selection.h>
#include <kis_types.h>
#include <KisSequentialIteratorProgress.h>

#include <KoBasicHistogramProducers.h>
#include "KoColorModelStandardIds.h"
#include <KoColorSpace.h>
#include <KoColorTransformation.h>
#include <KoUpdater.h>
#include <KisGlobalResourcesInterface.h>

namespace {
struct KritaThresholdFilterRegistration
{
    KritaThresholdFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisFilterThreshold());
    }
};
} // namespace
static KritaThresholdFilterRegistration s_kritaThresholdFilterRegistration;


KisFilterThreshold::KisFilterThreshold()
    : KisFilter(id(), FiltersCategoryAdjustId, PkString("&Threshold..."))
{
    setColorSpaceIndependence(FULLY_INDEPENDENT);

    setSupportsPainting(false);
    setShowConfigurationWidget(true);
    setSupportsLevelOfDetail(true);
    setSupportsAdjustmentLayers(true);
    setSupportsThreading(true);
}

void KisFilterThreshold::processImpl(KisPaintDeviceSP device,
                 const PkRect& applyRect,
                 const KisFilterConfigurationSP config,
                 KoUpdater *progressUpdater) const
{
    Q_ASSERT(!device.isNull());

    const int threshold = config->getInt("threshold");

    KoColor white(Qt::white, device->colorSpace());
    KoColor black(Qt::black, device->colorSpace());

    KisSequentialIteratorProgress it(device, applyRect, progressUpdater);
    const int pixelSize = device->colorSpace()->pixelSize();

    while (it.nextPixel()) {
        if (device->colorSpace()->intensity8(it.oldRawData()) > threshold) {
            white.setOpacity(device->colorSpace()->opacityU8(it.oldRawData()));
            memcpy(it.rawData(), white.data(), pixelSize);
        }
        else {
            black.setOpacity(device->colorSpace()->opacityU8(it.oldRawData()));
            memcpy(it.rawData(), black.data(), pixelSize);
        }
    }

}


KisFilterConfigurationSP KisFilterThreshold::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("threshold", 128);
    return config;
}
