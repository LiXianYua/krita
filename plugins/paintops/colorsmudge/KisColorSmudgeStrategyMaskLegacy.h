/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KRITA_KISCOLORSMUDGESTRATEGYMASKLEGACY_H
#define KRITA_KISCOLORSMUDGESTRATEGYMASKLEGACY_H

#include "KisColorSmudgeStrategyMask.h"


class KisColorSmudgeStrategyMaskLegacy : public KisColorSmudgeStrategyMask
{
public:
    KisColorSmudgeStrategyMaskLegacy(KisPainter *painter,
                                     KisImageSP image,
                                     bool smearAlpha,
                                     bool useDullingMode,
                                     bool useOverlayMode);

    void sampleDullingColor(const PkRect &srcRect, qreal sampleRadiusValue, KisColorSmudgeSourceSP sourceDevice,
                            KisFixedPaintDeviceSP tempFixedDevice, KisFixedPaintDeviceSP maskDab,
                            KoColor *resultColor) override;
    PkString smearCompositeOp(bool smearAlpha) const override;
    PkString finalCompositeOp(bool smearAlpha) const override;
    qreal finalPainterOpacity(qreal opacity, qreal smudgeRateValue) override;
    qreal colorRateOpacity(qreal opacity, qreal smudgeRateValue, qreal colorRateValue, qreal maxPossibleSmudgeRateValue) override;
    qreal dullingRateOpacity(qreal opacity, qreal smudgeRateValue) override;
    qreal smearRateOpacity(qreal opacity, qreal smudgeRateValue) override;
};


#endif //KRITA_KISCOLORSMUDGESTRATEGYMASKLEGACY_H
