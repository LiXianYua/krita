/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KRITA_KISCOLORSMUDGESTRATEGYWITHOVERLAY_H
#define KRITA_KISCOLORSMUDGESTRATEGYWITHOVERLAY_H

#include "KisColorSmudgeStrategyBase.h"
#include "kis_painter.h"


class KisColorSmudgeStrategyWithOverlay : public KisColorSmudgeStrategyBase
{
public:
    KisColorSmudgeStrategyWithOverlay(KisPainter *painter,
                                      KisImageSP image,
                                      bool smearAlpha,
                                      bool useDullingMode,
                                      bool useOverlayMode);

    virtual ~KisColorSmudgeStrategyWithOverlay();

    void initializePainting() override;

    PkVector<KisPainter*> finalPainters();

    PkVector<PkRect> paintDab(const PkRect &srcRect, const PkRect &dstRect, const KoColor &currentPaintColor, qreal opacity,
                            qreal colorRateValue, qreal smudgeRateValue, qreal maxPossibleSmudgeRateValue,
                            qreal lightnessStrengthValue, qreal smudgeRadiusValue) override;

protected:
    KisFixedPaintDeviceSP m_maskDab;
    bool m_shouldPreserveMaskDab = true;
    PkScopedPointer<KisOverlayPaintDeviceWrapper> m_layerOverlayDevice;

private:
    PkScopedPointer<KisOverlayPaintDeviceWrapper> m_imageOverlayDevice;
    KisColorSmudgeSourceSP m_sourceWrapperDevice;
    KisPainter m_finalPainter;
    PkScopedPointer<KisPainter> m_overlayPainter;
    bool m_smearAlpha = true;
    KisPainter *m_initializationPainter = 0;
};


#endif //KRITA_KISCOLORSMUDGESTRATEGYWITHOVERLAY_H
