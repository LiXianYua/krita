/*
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_SKETCH_PAINTOP_H_
#define KIS_SKETCH_PAINTOP_H_

#include <brushengine/kis_paintop.h>
#include <kis_types.h>

#include "KisSketchStandardOptions.h"
#include "KisSketchOpOptionData.h"
#include "kis_sketch_paintop_settings.h"

#include "kis_painter.h"
#include <kis_brush_option.h>
#include <KisStandardOptions.h>
#include "KisRotationOption.h"
#include "KisOpacityOption.h"
#include "KisAirbrushOptionData.h"

class KisDabCache;


class KisSketchPaintOp : public KisPaintOp
{

public:

    KisSketchPaintOp(const KisPaintOpSettingsSP settings, KisPainter *painter, KisNodeSP node, KisImageSP image);
    ~KisSketchPaintOp() override;

    void paintLine(const KisPaintInformation &pi1, const KisPaintInformation &pi2, KisDistanceInformation *currentDistance) override;

    static PkList<KoResourceLoadResult> prepareLinkedResources(const KisPaintOpSettingsSP settings, KisResourcesInterfaceSP resourcesInterface);

protected:
    KisSpacingInformation paintAt(const KisPaintInformation& info) override;

    KisSpacingInformation updateSpacingImpl(const KisPaintInformation &info) const override;

    KisTimingInformation updateTimingImpl(const KisPaintInformation &info) const override;

private:
    // pixel buffer
    KisPaintDeviceSP m_dab;

    // mask detection area
    KisFixedPaintDeviceSP m_maskDab;
    PkRectF m_brushBoundingBox;
    PkPointF m_hotSpot;

    // simple mode
    qreal m_radius {1.0};

    KisOpacityOption m_opacityOption;
    KisSizeOption m_sizeOption;
    KisRotationOption m_rotationOption;
    KisRateOption m_rateOption;
    KisDensityOption m_densityOption;
    KisLineWidthOption m_lineWidthOption;
    KisOffsetScaleOption m_offsetScaleOption;
    KisAirbrushOptionData m_airbrushOption;

    KisBrushOptionProperties m_brushOption;
    KisSketchOpOptionData m_sketchProperties;

    PkVector<PkPointF> m_points;
    int m_count {0};
    KisPainter * m_painter {nullptr};
    KisBrushSP m_brush;
    KisDabCache *m_dabCache {nullptr};

private:
    void drawConnection(const PkPointF &start, const PkPointF &end, double lineWidth);
    void updateBrushMask(const KisPaintInformation& info, qreal scale, qreal rotation);
    void doPaintLine(const KisPaintInformation &pi1, const KisPaintInformation &pi2);
};

#endif // KIS_SKETCH_PAINTOP_H_
