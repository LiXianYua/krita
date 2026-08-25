/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_mask_projection_plane.h"

#include <KoColorSpace.h>
#include <KoChannelInfo.h>
#include "kis_painter.h"
#include "kis_mask.h"


struct KisMaskProjectionPlane::Private
{
    KisMask *mask;
};


KisMaskProjectionPlane::KisMaskProjectionPlane(KisMask *mask)
    : m_d(new Private)
{
    m_d->mask = mask;
}

KisMaskProjectionPlane::~KisMaskProjectionPlane()
{
}

PkRect KisMaskProjectionPlane::recalculate(const PkRect& rect, KisNodeSP filthyNode, KisRenderPassFlags flags)
{
    Q_UNUSED(filthyNode);
    Q_UNUSED(flags);

    KIS_ASSERT_RECOVER_NOOP(0 && "KisMaskProjectionPlane::recalculate() is not defined!");

    return rect;
}

void KisMaskProjectionPlane::apply(KisPainter *painter, const PkRect &rect)
{
    Q_UNUSED(painter);
    Q_UNUSED(rect);

    KIS_ASSERT_RECOVER_NOOP(0 && "KisMaskProjectionPlane::apply() is not defined!");
}

KisPaintDeviceList KisMaskProjectionPlane::getLodCapableDevices() const
{
    // masks have no projection
    return KisPaintDeviceList();
}

PkRect KisMaskProjectionPlane::needRect(const PkRect &rect, KisNode::PositionToFilthy pos) const
{
    return m_d->mask->needRect(rect, pos);
}

PkRect KisMaskProjectionPlane::changeRect(const PkRect &rect, KisNode::PositionToFilthy pos) const
{
    return m_d->mask->changeRect(rect, pos);
}

PkRect KisMaskProjectionPlane::accessRect(const PkRect &rect, KisNode::PositionToFilthy pos) const
{
    return m_d->mask->accessRect(rect, pos);
}

PkRect KisMaskProjectionPlane::needRectForOriginal(const PkRect &rect) const
{
    return rect;
}

PkRect KisMaskProjectionPlane::tightUserVisibleBounds() const
{
    // masks don't have any internal rendering subtrees,
    // so just return the exact bounds of the mask
    return m_d->mask->exactBounds();
}

PkRect KisMaskProjectionPlane::looseUserVisibleBounds() const
{
    // masks don't have anything complex inside, so just
    // so just return the extent of the mask
    return m_d->mask->extent();
}

