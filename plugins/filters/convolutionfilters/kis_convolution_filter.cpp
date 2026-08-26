/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_convolution_filter.h"
#include <kis_debug.h>

#include <KoCompositeOp.h>

#include "kis_painter.h"
#include "kis_convolution_painter.h"
#include "kis_convolution_kernel.h"
#include <filter/kis_filter_configuration.h>
#include <kis_selection.h>
#include <kis_paint_device.h>
#include <kis_processing_information.h>
#include "kis_lod_transform.h"

KisConvolutionFilter::KisConvolutionFilter(const KoID& id, const KoID & category, const PkString & entry)
        : KisFilter(id, category, entry)
{
    setColorSpaceIndependence(FULLY_INDEPENDENT);
    setSupportsLevelOfDetail(true);
}


void KisConvolutionFilter::processImpl(KisPaintDeviceSP device,
                                       const PkRect& applyRect,
                                       const KisFilterConfigurationSP config,
                                       KoUpdater* progressUpdater) const
{
    Q_UNUSED(config);

    PkPoint srcTopLeft = applyRect.topLeft();
    Q_ASSERT(device != 0);

    KisConvolutionPainter painter(device);

    PkBitArray channelFlags;
    if (config) {
        channelFlags = config->channelFlags();
    }
    if (channelFlags.isEmpty() || !config) {
        channelFlags = PkBitArray(device->colorSpace()->channelCount(), true);
    }
 
    painter.setChannelFlags(channelFlags);
    painter.setProgress(progressUpdater);
    painter.applyMatrix(m_matrix, device, srcTopLeft, srcTopLeft, applyRect.size(), BORDER_REPEAT);

}

PkRect KisConvolutionFilter::neededRect(const PkRect &rect, const KisFilterConfigurationSP _config, int lod) const
{
    Q_UNUSED(_config);

    KisLodTransformScalar t(lod);

    const int windowsize = qMax(m_matrix->width(), m_matrix->height());
    const int margin  = qCeil(t.scale(0.5 * windowsize)) + 1;
    return kisGrowRect(rect, margin);
}

PkRect KisConvolutionFilter::changedRect(const PkRect &rect, const KisFilterConfigurationSP _config, int lod) const
{
    return neededRect(rect, _config, lod);
}

void KisConvolutionFilter::setIgnoreAlpha(bool v)
{
    m_ignoreAlpha = v;
}
