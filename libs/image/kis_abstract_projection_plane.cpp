/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_abstract_projection_plane.h"


KisAbstractProjectionPlane::KisAbstractProjectionPlane()
{
}

KisAbstractProjectionPlane::~KisAbstractProjectionPlane()
{
}

PkRect KisDumbProjectionPlane::recalculate(const PkRect& rect, KisNodeSP filthyNode, KisRenderPassFlags flags)
{
    Q_UNUSED(filthyNode);
    Q_UNUSED(flags);
    return rect;
}

void KisDumbProjectionPlane::apply(KisPainter *painter, const PkRect &rect)
{
    Q_UNUSED(painter);
    Q_UNUSED(rect);
}


PkRect KisDumbProjectionPlane::needRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const
{
    Q_UNUSED(pos);
    return rect;
}

PkRect KisDumbProjectionPlane::changeRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const
{
    Q_UNUSED(pos);
    return rect;
}

PkRect KisDumbProjectionPlane::accessRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const
{
    Q_UNUSED(pos);
    return rect;
}

PkRect KisDumbProjectionPlane::needRectForOriginal(const PkRect &rect) const
{
    return rect;
}

PkRect KisDumbProjectionPlane::tightUserVisibleBounds() const
{
    return PkRect();
}

PkRect KisDumbProjectionPlane::looseUserVisibleBounds() const
{
    return PkRect();
}

KisPaintDeviceList KisDumbProjectionPlane::getLodCapableDevices() const
{
    // arghm...
    return KisPaintDeviceList();
}
