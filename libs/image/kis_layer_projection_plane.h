/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LAYER_PROJECTION_PLANE_H
#define __KIS_LAYER_PROJECTION_PLANE_H

#include "kis_abstract_projection_plane.h"

#include <PkScopedPointer.h>
#include "krita_utils.h"

/**
 * An implementation of the KisAbstractProjectionPlane interface for a
 * layer object
 */
class KisLayerProjectionPlane : public KisAbstractProjectionPlane
{
public:
    KisLayerProjectionPlane(KisLayer *layer);
    ~KisLayerProjectionPlane() override;

    PkRect recalculate(const PkRect& rect, KisNodeSP filthyNode, KisRenderPassFlags flags) override;
    void apply(KisPainter *painter, const PkRect &rect) override;
    void applyMaxOutAlpha(KisPainter *painter, const PkRect &rect, KritaUtils::ThresholdMode thresholdMode);

    PkRect needRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const override;
    PkRect changeRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const override;
    PkRect accessRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const override;
    PkRect needRectForOriginal(const PkRect &rect) const override;
    PkRect tightUserVisibleBounds() const override;
    PkRect looseUserVisibleBounds() const override;

    KisPaintDeviceList getLodCapableDevices() const override;

private:
    void applyImpl(KisPainter *painter, const PkRect &rect, KritaUtils::ThresholdMode thresholdMode);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

typedef PkSharedPointer<KisLayerProjectionPlane> KisLayerProjectionPlaneSP;
typedef PkWeakPointer<KisLayerProjectionPlane> KisLayerProjectionPlaneWSP;


#endif /* __KIS_LAYER_PROJECTION_PLANE_H */
