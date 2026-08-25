/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_MASK_PROJECTION_PLANE_H
#define __KIS_MASK_PROJECTION_PLANE_H

#include "kis_abstract_projection_plane.h"

#include <PkScopedPointer.h>
/**
 * An implementation of the KisAbstractProjectionPlane interface for a
 * layer object.
 *
 * Please note that recalculate() and apply() methods are not defined
 * for masks, because the KisLayer code still uses traditional
 * methods of KisMask directly.
 */
class KisMaskProjectionPlane : public KisAbstractProjectionPlane
{
public:
    KisMaskProjectionPlane(KisMask *mask);
    ~KisMaskProjectionPlane() override;

    PkRect recalculate(const PkRect& rect, KisNodeSP filthyNode, KisRenderPassFlags flags) override;
    void apply(KisPainter *painter, const PkRect &rect) override;

    PkRect needRect(const PkRect &rect, KisNode::PositionToFilthy pos) const override;
    PkRect changeRect(const PkRect &rect, KisNode::PositionToFilthy pos) const override;
    PkRect accessRect(const PkRect &rect, KisNode::PositionToFilthy pos) const override;
    PkRect needRectForOriginal(const PkRect &rect) const override;
    PkRect tightUserVisibleBounds() const override;
    PkRect looseUserVisibleBounds() const override;

    KisPaintDeviceList getLodCapableDevices() const override;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_MASK_PROJECTION_PLANE_H */
