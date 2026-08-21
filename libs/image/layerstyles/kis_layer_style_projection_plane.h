/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LAYER_STYLE_PROJECTION_PLANE_H
#define __KIS_LAYER_STYLE_PROJECTION_PLANE_H

#include "kis_abstract_projection_plane.h"

#include <PkScopedPointer.h>

#include "kis_types.h"

#include <kritaimage_export.h>


class KRITAIMAGE_EXPORT KisLayerStyleProjectionPlane : public KisAbstractProjectionPlane
{
public:
    KisLayerStyleProjectionPlane(KisLayer *sourceLayer);
    KisLayerStyleProjectionPlane(const KisLayerStyleProjectionPlane &rhs, KisLayer *sourceLayer, KisPSDLayerStyleSP clonedStyle);

    ~KisLayerStyleProjectionPlane() override;

    PkRect recalculate(const PkRect& rect, KisNodeSP filthyNode, KisRenderPassFlags flags) override;
    void apply(KisPainter *painter, const PkRect &rect) override;

    PkRect needRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const override;
    PkRect changeRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const override;
    PkRect accessRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const override;
    PkRect needRectForOriginal(const PkRect &rect) const override;
    PkRect tightUserVisibleBounds() const override;
    PkRect looseUserVisibleBounds() const override;

    KisPaintDeviceList getLodCapableDevices() const override;


    // a method for registering on KisLayerStyleProjectionPlaneFactory
    static KisAbstractProjectionPlaneSP factoryObject(KisLayer *sourceLayer);

private:
    friend class KisLayerStyleProjectionPlaneTest;
    KisLayerStyleProjectionPlane(KisLayer *sourceLayer, KisPSDLayerStyleSP style);

    void init(KisLayer *sourceLayer, KisPSDLayerStyleSP layerStyle);

    PkRect stylesNeedRect(const PkRect &rect) const;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

typedef PkSharedPointer<KisLayerStyleProjectionPlane> KisLayerStyleProjectionPlaneSP;
typedef PkWeakPointer<KisLayerStyleProjectionPlane> KisLayerStyleProjectionPlaneWSP;

#endif /* __KIS_LAYER_STYLE_PROJECTION_PLANE_H */
