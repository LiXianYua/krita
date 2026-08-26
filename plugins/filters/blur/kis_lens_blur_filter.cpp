/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2010 Edward Apap <schumifer@hotmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "kis_lens_blur_filter.h"

#include <KoCompositeOp.h>

#include <kis_convolution_kernel.h>
#include <kis_convolution_painter.h>

#include <filter/kis_filter_category_ids.h>
#include <filter/kis_filter_configuration.h>
#include <kis_selection.h>
#include <kis_paint_device.h>
#include <kis_processing_information.h>
#include "kis_lod_transform.h"


#include <math.h>


KisLensBlurFilter::KisLensBlurFilter() : KisFilter(id(), FiltersCategoryBlurId, i18n("&Lens Blur..."))
{
    setSupportsPainting(true);
    setSupportsAdjustmentLayers(true);
    setSupportsLevelOfDetail(true);
    setColorSpaceIndependence(FULLY_INDEPENDENT);

}

PkSize KisLensBlurFilter::getKernelHalfSize(const KisFilterConfigurationSP config, int lod)
{
    PkPolygonF iris = getIrisPolygon(config, lod);
    PkRect rect = iris.boundingRect().toAlignedRect();

    int w = std::ceil(qreal(rect.width()) / 2.0);
    int h = std::ceil(qreal(rect.height()) / 2.0);

    return PkSize(w, h);
}

KisFilterConfigurationSP KisLensBlurFilter::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("irisShape", "Pentagon (5)");
    config->setProperty("irisRadius", 5);
    config->setProperty("irisRotation", 0);

    PkSize halfSize = getKernelHalfSize(config, 0);
    config->setProperty("halfWidth", halfSize.width());
    config->setProperty("halfHeight", halfSize.height());

    return config;
}

PkPolygonF KisLensBlurFilter::getIrisPolygon(const KisFilterConfigurationSP config, int lod)
{
    KIS_ASSERT_RECOVER(config) { return PkPolygonF(); }

    KisLodTransformScalar t(lod);

    PkVariant value;
    config->getProperty("irisShape", value);
    PkString irisShape = value.toString();
    config->getProperty("irisRadius", value);
    uint irisRadius = t.scale(value.toUInt());
    config->getProperty("irisRotation", value);
    uint irisRotation = value.toUInt();

    if (irisRadius < 1)
        return PkPolygonF();

    PkPolygonF irisShapePoly;

    int sides = 1;
    qreal angle = 0;

    if (irisShape == "Triangle") sides = 3;
    else if (irisShape == "Quadrilateral (4)") sides = 4;
    else if (irisShape == "Pentagon (5)") sides = 5;
    else if (irisShape == "Hexagon (6)") sides = 6;
    else if (irisShape == "Heptagon (7)") sides = 7;
    else if (irisShape == "Octagon (8)") sides = 8;
    else return PkPolygonF();

    for (int i = 0; i < sides; ++i) {
        irisShapePoly << PkPointF(0.5 * cos(angle), 0.5 * sin(angle));
        angle += 2 * M_PI / sides;
    }

    PkTransform transform;
    transform.rotate(irisRotation);
    transform.scale(irisRadius * 2, irisRadius * 2);

    PkPolygonF transformedIris = transform.map(irisShapePoly);

    return transformedIris;
}

void KisLensBlurFilter::processImpl(KisPaintDeviceSP device,
                                    const PkRect& rect,
                                    const KisFilterConfigurationSP config,
                                    KoUpdater* progressUpdater
                                    ) const
{
    PkPoint srcTopLeft = rect.topLeft();

    Q_ASSERT(device != 0);
    KIS_SAFE_ASSERT_RECOVER_RETURN(config);

    PkBitArray channelFlags = config->channelFlags();
    if (channelFlags.isEmpty()) {
        channelFlags = PkBitArray(device->colorSpace()->channelCount(), true);
    }

    const int lod = device->defaultBounds()->currentLevelOfDetail();
    PkPolygonF transformedIris = getIrisPolygon(config, lod);
    if (transformedIris.isEmpty()) return;

    PkRectF boundingRect = transformedIris.boundingRect();

    int kernelWidth = boundingRect.toAlignedRect().width();
    int kernelHeight = boundingRect.toAlignedRect().height();

    // [GAP] QPainter 内核构造已剥离：原来用 QPainter 填充 transformedIris 多边形再读回像素。
    // 现改用单位核（中心=1，其余=0），卷积退化为 no-op。恢复路径归 S-09/M5。
    Eigen::Matrix<qreal, Eigen::Dynamic, Eigen::Dynamic> irisKernel(kernelHeight, kernelWidth);
    irisKernel.setZero();
    irisKernel(kernelHeight / 2, kernelWidth / 2) = 1.0;

    // apply convolution
    KisConvolutionPainter painter(device);
    painter.setChannelFlags(channelFlags);
    painter.setProgress(progressUpdater);

    KisConvolutionKernelSP kernel = KisConvolutionKernel::fromMatrix(irisKernel, 0, irisKernel.sum());
    painter.applyMatrix(kernel, device, srcTopLeft, srcTopLeft, rect.size(), BORDER_REPEAT);
}

PkRect KisLensBlurFilter::neededRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const
{
    KisLodTransformScalar t(lod);

    PkVariant value;
    const int halfWidth = t.scale(_config->getProperty("halfWidth", value) ? value.toUInt() : 5);
    const int halfHeight = t.scale(_config->getProperty("halfHeight", value) ? value.toUInt() : 5);

    return rect.adjusted(-halfWidth * 2, -halfHeight * 2, halfWidth * 2, halfHeight * 2);
}

PkRect KisLensBlurFilter::changedRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const
{
    KisLodTransformScalar t(lod);

    PkVariant value;
    const int halfWidth = t.scale(_config->getProperty("halfWidth", value) ? value.toUInt() : 5);
    const int halfHeight = t.scale(_config->getProperty("halfHeight", value) ? value.toUInt() : 5);

    return rect.adjusted(-halfWidth, -halfHeight, halfWidth, halfHeight);
}
