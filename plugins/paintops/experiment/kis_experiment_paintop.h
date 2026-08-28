/*
 *  SPDX-FileCopyrightText: 2010-2011 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_EXPERIMENT_PAINTOP_H_
#define KIS_EXPERIMENT_PAINTOP_H_

#include <PkPainterPath.h>

#include <klocalizedstring.h>
#include <brushengine/kis_paintop.h>
#include <kis_types.h>

#include "kis_experiment_paintop_settings.h"
#include "KisExperimentOpOptionData.h"

#include <kis_painter.h>

class PkPointF;
class KisPainter;
class KisRegion;

class KisExperimentPaintOp : public KisPaintOp
{

public:

    KisExperimentPaintOp(const KisPaintOpSettingsSP settings, KisPainter *painter, KisNodeSP node, KisImageSP image);
    ~KisExperimentPaintOp() override;

    void paintLine(const KisPaintInformation& pi1, const KisPaintInformation& pi2, KisDistanceInformation *currentDistance) override;

protected:
    KisSpacingInformation paintAt(const KisPaintInformation& info) override;

    KisSpacingInformation updateSpacingImpl(const KisPaintInformation &info) const override;

private:
    void paintRegion(const KisRegion &changedRegion);
    PkPointF speedCorrectedPosition(const KisPaintInformation& pi1,
                                   const KisPaintInformation& pi2);


    static qreal simplifyThreshold(const PkRectF &bounds);
    static PkPointF getAngle(const PkPointF& p1, const PkPointF& p2, qreal distance);
    static PkPainterPath applyDisplace(const PkPainterPath& path, int speed);


    bool m_displaceEnabled {false};
    int m_displaceCoeff {0};
    PkPainterPath m_lastPaintedPath;

    bool m_windingFill {false};
    bool m_hardEdge {false};

    bool m_speedEnabled {false};
    int m_speedMultiplier {1};
    qreal m_savedSpeedCoeff {1.0};
    PkPointF m_savedSpeedPoint;

    bool m_smoothingEnabled {false};
    int m_smoothingThreshold {1};
    PkPointF m_savedSmoothingPoint;
    int m_savedSmoothingDistance {1};

    int m_savedUpdateDistance {1};
    PkVector<PkPointF> m_savedPoints;
    int m_lastPaintTime {0};

    bool m_firstRun {true};
    PkPointF m_center;

    PkPainterPath m_path;
    KisExperimentOpOptionData m_experimentOption;

    bool m_useMirroring {false};
    KisPainter *m_originalPainter {0};
    KisPaintDeviceSP m_originalDevice;

    KisPainter::FillStyle m_fillStyle {KisPainter::FillStyleNone};
};

#endif // KIS_EXPERIMENT_PAINTOP_H_
