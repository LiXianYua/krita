/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2010 Edward Apap <schumifer@hotmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "kis_motion_blur_filter.h"

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


KisMotionBlurFilter::KisMotionBlurFilter() : KisFilter(id(), FiltersCategoryBlurId, PkString("&Motion Blur..."))
{
    setSupportsPainting(true);
    setSupportsAdjustmentLayers(true);
    setSupportsLevelOfDetail(true);
    setColorSpaceIndependence(FULLY_INDEPENDENT);
}

KisFilterConfigurationSP KisMotionBlurFilter::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("blurAngle", 0);
    config->setProperty("blurLength", 5);

    return config;
}

namespace {
struct MotionBlurProperties
{
    MotionBlurProperties(KisFilterConfigurationSP config, const KisLodTransformScalar &t)
    {
        const int blurAngle = config->getInt("blurAngle", 0);
        const int blurLength = config->getInt("blurLength", 5);

        // convert angle to radians
        const qreal angleRadians = kisDegreesToRadians(qreal(blurAngle));

        // construct image
        const qreal halfWidth = 0.5 * t.scale(blurLength) * cos(angleRadians);
        const qreal halfHeight = 0.5 * t.scale(blurLength) * sin(angleRadians);

        kernelHalfSize.rwidth() = ceil(fabs(halfWidth));
        kernelHalfSize.rheight() = ceil(fabs(halfHeight));
        kernelSize = kernelHalfSize * 2 + PkSize(1, 1);
        this->blurLength = blurLength;


        PkPointF p1(0.5 * kernelSize.width(), 0.5 * kernelSize.height());
        PkPointF p2(halfWidth, halfHeight);
        motionLine = PkLineF(p1 - p2, p1 + p2);
    }

    int blurLength;
    PkSize kernelSize;
    PkSize kernelHalfSize;
    PkLineF motionLine;
};
}

void KisMotionBlurFilter::processImpl(KisPaintDeviceSP device,
                                      const PkRect& rect,
                                      const KisFilterConfigurationSP config,
                                      KoUpdater* progressUpdater
                                      ) const
{
    PkPoint srcTopLeft = rect.topLeft();

    Q_ASSERT(device);
    KIS_SAFE_ASSERT_RECOVER_RETURN(config);

    KisLodTransformScalar t(device);
    MotionBlurProperties props(config, t);

    if (props.blurLength == 0) {
        return;
    }

    PkBitArray channelFlags;

    if (config) {
        channelFlags = config->channelFlags();
    }

    if (channelFlags.isEmpty() || !config) {
        channelFlags = PkBitArray(device->colorSpace()->channelCount(), true);
    }

    // [GAP] QPainter 内核构造已剥离：原来用 QPainter 在 PkImage 上画 props.motionLine
    // 再读回像素生成卷积核。现改用单位核（中心=1，其余=0），卷积退化为 no-op。
    // 恢复路径归 S-09/M5（接回真实 QPainter 或软件光栅化）。
    Eigen::Matrix<qreal, Eigen::Dynamic, Eigen::Dynamic> motionBlurKernel(props.kernelSize.height(), props.kernelSize.width());
    motionBlurKernel.setZero();
    motionBlurKernel(props.kernelSize.height() / 2, props.kernelSize.width() / 2) = 1.0;

    // apply convolution
    KisConvolutionPainter painter(device);
    painter.setChannelFlags(channelFlags);
    painter.setProgress(progressUpdater);

    KisConvolutionKernelSP kernel = KisConvolutionKernel::fromMatrix(motionBlurKernel, 0, motionBlurKernel.sum());
    painter.applyMatrix(kernel, device, srcTopLeft, srcTopLeft, rect.size(), BORDER_REPEAT);
}

PkRect KisMotionBlurFilter::neededRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const
{
    KisLodTransformScalar t(lod);
    MotionBlurProperties props(_config, t);
    return rect.adjusted(-props.kernelHalfSize.width(), -props.kernelHalfSize.height(), props.kernelHalfSize.width(), props.kernelHalfSize.height());
}

PkRect KisMotionBlurFilter::changedRect(const PkRect & rect, const KisFilterConfigurationSP _config, int lod) const
{
    return neededRect(rect, _config, lod);
}
