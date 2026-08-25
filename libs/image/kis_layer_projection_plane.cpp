/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_layer_projection_plane.h"

#include <PkBitArray.h>
#include <KoColorSpace.h>
#include <KoChannelInfo.h>
#include <KoCompositeOpRegistry.h>
#include "kis_painter.h"
#include "kis_projection_leaf.h"
#include "kis_cached_paint_device.h"
#include "kis_sequential_iterator.h"


struct KisLayerProjectionPlane::Private
{
    KisLayer *layer;
    KisCachedPaintDevice cachedDevice;
};


KisLayerProjectionPlane::KisLayerProjectionPlane(KisLayer *layer)
    : m_d(new Private)
{
    m_d->layer = layer;
}

KisLayerProjectionPlane::~KisLayerProjectionPlane()
{
}

PkRect KisLayerProjectionPlane::recalculate(const PkRect& rect, KisNodeSP filthyNode, KisRenderPassFlags flags)
{
    return m_d->layer->updateProjection(rect, filthyNode, flags);
}

void KisLayerProjectionPlane::applyImpl(KisPainter *painter, const PkRect &rect, KritaUtils::ThresholdMode thresholdMode)
{
    KisPaintDeviceSP device = m_d->layer->projection();
    if (!device) return;

    PkRect needRect = rect;

    if (m_d->layer->compositeOpId() != COMPOSITE_COPY &&
        m_d->layer->compositeOpId() != COMPOSITE_DESTINATION_IN  &&
        m_d->layer->compositeOpId() != COMPOSITE_DESTINATION_ATOP) {

        needRect &= device->extent();
    }

    if(needRect.isEmpty()) return;

    const PkBitArray channelFlags = m_d->layer->projectionLeaf()->channelFlags();

    PkScopedPointer<KisCachedPaintDevice::Guard> d1;

    if (thresholdMode != KritaUtils::ThresholdNone) {
        d1.reset(new KisCachedPaintDevice::Guard(device, m_d->cachedDevice));
        KisPaintDeviceSP tmp = d1->device();
        tmp->makeCloneFromRough(device, needRect);

        KritaUtils::thresholdOpacity(tmp, needRect, thresholdMode);

        device = tmp;
    }

    painter->setChannelFlags(channelFlags);
    painter->setCompositeOpId(m_d->layer->compositeOpId());
    painter->setOpacityU8(m_d->layer->projectionLeaf()->opacity());
    painter->bitBlt(needRect.topLeft(), device, needRect);
}

void KisLayerProjectionPlane::apply(KisPainter *painter, const PkRect &rect)
{
    applyImpl(painter, rect, KritaUtils::ThresholdNone);
}

void KisLayerProjectionPlane::applyMaxOutAlpha(KisPainter *painter, const PkRect &rect, KritaUtils::ThresholdMode thresholdMode)
{
    applyImpl(painter, rect, thresholdMode);
}

KisPaintDeviceList KisLayerProjectionPlane::getLodCapableDevices() const
{
    return KisPaintDeviceList() << m_d->layer->projection();
}

PkRect KisLayerProjectionPlane::needRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const
{
    return m_d->layer->needRect(rect, pos);
}

PkRect KisLayerProjectionPlane::changeRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const
{
    return m_d->layer->changeRect(rect, pos);
}

PkRect KisLayerProjectionPlane::accessRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const
{
    return m_d->layer->accessRect(rect, pos);
}

PkRect KisLayerProjectionPlane::needRectForOriginal(const PkRect &rect) const
{
    return m_d->layer->needRectForOriginal(rect);
}

PkRect KisLayerProjectionPlane::tightUserVisibleBounds() const
{
    return m_d->layer->tightUserVisibleBounds();
}

PkRect KisLayerProjectionPlane::looseUserVisibleBounds() const
{
    return m_d->layer->looseUserVisibleBounds();
}

