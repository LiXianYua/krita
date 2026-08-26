/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2019 Miguel Lopez <reptillia39@live.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "gaussianhighpass_filter.h"
#include <PkBitArray.h>

#include <KoColorSpace.h>
#include <KoChannelInfo.h>
#include <KoColor.h>
#include <kis_painter.h>
#include <kis_paint_device.h>
#include <kis_paint_layer.h>
#include <kis_group_layer.h>

#include <kis_mask_generator.h>
#include <kis_gaussian_kernel.h>
#include <filter/kis_filter_category_ids.h>
#include <filter/kis_filter_configuration.h>
#include <kis_processing_information.h>
#include <KoProgressUpdater.h>
#include <KoUpdater.h>
#include <KoMixColorsOp.h>
#include "kis_lod_transform.h"
#include <KoCompositeOpRegistry.h>

#include "KoColorSpaceTraits.h"
#include <KisSequentialIteratorProgress.h>


KisGaussianHighPassFilter::KisGaussianHighPassFilter() : KisFilter(id(), FiltersCategoryEdgeDetectionId, PkString("&Gaussian High Pass..."))
{
    setSupportsPainting(true);
    setSupportsAdjustmentLayers(true);
    setSupportsThreading(true);
    setSupportsLevelOfDetail(true);
    setColorSpaceIndependence(FULLY_INDEPENDENT);
}

KisFilterConfigurationSP KisGaussianHighPassFilter::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("blurAmount", 1);
    return config;
}

void KisGaussianHighPassFilter::processImpl(KisPaintDeviceSP device,
                                   const PkRect& applyRect,
                                   const KisFilterConfigurationSP config,
                                   KoUpdater *
                                   ) const
{
    PkPointer<KoUpdater> convolutionUpdater = 0;
    KIS_SAFE_ASSERT_RECOVER_RETURN(config);

    PkVariant value;
    KisLodTransformScalar t(device);
    const qreal blurAmount = t.scale(config->getProperty("blurAmount", value) ? value.toDouble() : 1.0);
    PkBitArray channelFlags = config->channelFlags();

    const PkRect gaussNeedRect = this->neededRect(applyRect, config, device->defaultBounds()->currentLevelOfDetail());

    KisCachedPaintDevice::Guard d1(device, m_cachedPaintDevice);
    KisPaintDeviceSP blur = d1.device();
    KisPainter::copyAreaOptimizedOldData(gaussNeedRect.topLeft(), device, blur, gaussNeedRect);
    KisGaussianKernel::applyGaussian(blur, applyRect,
                                     blurAmount, blurAmount,
                                     channelFlags,
                                     convolutionUpdater,
                                     true); // make sure we create an internal transaction on temp device
    
    KisPainter painter(device);
    painter.setCompositeOpId(COMPOSITE_GRAIN_EXTRACT);
    painter.bitBlt(applyRect.topLeft(), blur, applyRect);
    painter.end();
}


PkRect KisGaussianHighPassFilter::neededRect(const PkRect & rect, const KisFilterConfigurationSP config, int lod) const
{
    KisLodTransformScalar t(lod);

    PkVariant value;

    const int halfSize = config->getProperty("blurAmount", value) ? KisGaussianKernel::kernelSizeFromRadius(t.scale(value.toFloat())) / 2 : 5;

    return rect.adjusted( -halfSize * 2, -halfSize * 2, halfSize * 2, halfSize * 2);
}

PkRect KisGaussianHighPassFilter::changedRect(const PkRect & rect, const KisFilterConfigurationSP config, int lod) const
{
    KisLodTransformScalar t(lod);

    PkVariant value;

    const int halfSize = config->getProperty("blurAmount", value) ? KisGaussianKernel::kernelSizeFromRadius(t.scale(value.toFloat())) / 2 : 5;

    return rect.adjusted( -halfSize, -halfSize, halfSize, halfSize);
}
