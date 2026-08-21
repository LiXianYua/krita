/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LAYER_STYLE_FILTER_PROJECTION_PLANE_H
#define __KIS_LAYER_STYLE_FILTER_PROJECTION_PLANE_H

#include "kis_abstract_projection_plane.h"

#include <PkScopedPointer.h>

#include "kis_types.h"

class KisLayerStyleKnockoutBlower;


class KisLayerStyleFilterProjectionPlane : public KisAbstractProjectionPlane
{
public:
    KisLayerStyleFilterProjectionPlane(KisLayer *sourceLayer);
    KisLayerStyleFilterProjectionPlane(const KisLayerStyleFilterProjectionPlane &rhs, KisLayer *sourceLayer, KisPSDLayerStyleSP clonedStyle);
    ~KisLayerStyleFilterProjectionPlane() override;

    void setStyle(KisLayerStyleFilter *filter, KisPSDLayerStyleSP style);

    PkRect recalculate(const PkRect& rect, KisNodeSP filthyNode, KisRenderPassFlags flags) override;
    void apply(KisPainter *painter, const PkRect &rect) override;

    PkRect needRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const override;
    PkRect changeRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const override;
    PkRect accessRect(const PkRect &rect, KisLayer::PositionToFilthy pos) const override;
    PkRect needRectForOriginal(const PkRect &rect) const override;
    PkRect tightUserVisibleBounds() const override;
    PkRect looseUserVisibleBounds() const override;

    KisPaintDeviceList getLodCapableDevices() const override;

    /**
     * \returns true if a call to apply() will actually paint anything. Basically,
     * it is a cached version of isEnabled(), though the state may change after calling
     * to recalculate().
     */
    bool isEmpty() const;

    KisLayerStyleKnockoutBlower *knockoutBlower() const;

protected:

    KisLayerStyleFilter* filter() const;
    KisPSDLayerStyleSP style() const;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

typedef PkSharedPointer<KisLayerStyleFilterProjectionPlane> KisLayerStyleFilterProjectionPlaneSP;
typedef PkWeakPointer<KisLayerStyleFilterProjectionPlane> KisLayerStyleFilterProjectionPlaneWSP;

#endif /* __KIS_LAYER_STYLE_FILTER_PROJECTION_PLANE_H */
