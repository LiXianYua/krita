/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KRITA_KISCOLORSMUDGESTRATEGYBASE_H
#define KRITA_KISCOLORSMUDGESTRATEGYBASE_H

#include <kis_types.h>

#include "KisColorSmudgeStrategy.h"
#include "KisColorSmudgeSource.h"

class KisPainter;


class KisColorSmudgeStrategyBase : public KisColorSmudgeStrategy
{
public:
    struct DabColoringStrategy
    {
        virtual ~DabColoringStrategy() = default;
        virtual bool supportsFusedDullingBlending() const = 0;
        virtual void blendInColorRate(const KoColor &paintColor, const KoCompositeOp *colorRateOp, qreal colorRateOpacity,
                                      KisFixedPaintDeviceSP dstDevice, const PkRect &dstRect) const = 0;
        virtual void blendInFusedBackgroundAndColorRateWithDulling(KisFixedPaintDeviceSP dst, KisColorSmudgeSourceSP src,
                                                                   const PkRect &dstRect,
                                                                   const KoColor &preparedDullingColor,
                                                                   const KoCompositeOp *smearOp,
                                                                   const qreal smudgeRateOpacity,
                                                                   const KoColor &paintColor,
                                                                   const KoCompositeOp *colorRateOp,
                                                                   const qreal colorRateOpacity) const = 0;
    };

    struct DabColoringStrategyMask : public DabColoringStrategy
    {
        bool supportsFusedDullingBlending() const override;

        void blendInColorRate(const KoColor &paintColor, const KoCompositeOp *colorRateOp, qreal colorRateOpacity,
                              KisFixedPaintDeviceSP dstDevice, const PkRect &dstRect) const override;

        void blendInFusedBackgroundAndColorRateWithDulling(KisFixedPaintDeviceSP dst,
                                                           KisColorSmudgeSourceSP src,
                                                           const PkRect &dstRect,
                                                           const KoColor &preparedDullingColor,
                                                           const KoCompositeOp *smearOp,
                                                           const qreal smudgeRateOpacity,
                                                           const KoColor &paintColor,
                                                           const KoCompositeOp *colorRateOp,
                                                           const qreal colorRateOpacity) const override;
    };

    struct DabColoringStrategyStamp : public DabColoringStrategy
    {
        void setStampDab(KisFixedPaintDeviceSP device);

        void blendInColorRate(const KoColor &paintColor, const KoCompositeOp *colorRateOp, qreal colorRateOpacity,
                              KisFixedPaintDeviceSP dstDevice, const PkRect &dstRect) const override;

        bool supportsFusedDullingBlending() const override;

        void blendInFusedBackgroundAndColorRateWithDulling(KisFixedPaintDeviceSP dst,
                                                           KisColorSmudgeSourceSP src,
                                                           const PkRect &dstRect,
                                                           const KoColor &preparedDullingColor,
                                                           const KoCompositeOp *smearOp,
                                                           const qreal smudgeRateOpacity,
                                                           const KoColor &paintColor,
                                                           const KoCompositeOp *colorRateOp,
                                                           const qreal colorRateOpacity) const override;

    private:
        KisFixedPaintDeviceSP m_origDab;
    };

public:

    KisColorSmudgeStrategyBase(bool useDullingMode);

    void initializePaintingImpl(const KoColorSpace *dstColorSpace,
                                bool smearAlpha,
                                const PkString &colorRateCompositeOpId);

    virtual DabColoringStrategy& coloringStrategy() = 0;

    const KoColorSpace* preciseColorSpace() const override;

    virtual PkString smearCompositeOp(bool smearAlpha) const;

    virtual PkString finalCompositeOp(bool smearAlpha) const;

    virtual qreal finalPainterOpacity(qreal opacity, qreal smudgeRateValue);

    virtual qreal colorRateOpacity(qreal opacity, qreal smudgeRateValue, qreal colorRateValue, qreal maxPossibleSmudgeRateValue);

    virtual qreal dullingRateOpacity(qreal opacity, qreal smudgeRateValue);

    virtual qreal smearRateOpacity(qreal opacity, qreal smudgeRateValue);

    virtual void sampleDullingColor(const PkRect &srcRect, qreal sampleRadiusValue, KisColorSmudgeSourceSP sourceDevice,
                                    KisFixedPaintDeviceSP tempFixedDevice, KisFixedPaintDeviceSP maskDab,
                                    KoColor *resultColor);

    void blendBrush(const PkVector<KisPainter *> dstPainters, KisColorSmudgeSourceSP srcSampleDevice,
                    KisFixedPaintDeviceSP maskDab, bool preserveMaskDab, const PkRect &srcRect, const PkRect &dstRect,
                    const KoColor &currentPaintColor, qreal opacity, qreal smudgeRateValue,
                    qreal maxPossibleSmudgeRateValue, qreal colorRateValue, qreal smudgeRadiusValue);

    void blendInBackgroundWithSmearing(KisFixedPaintDeviceSP dst, KisColorSmudgeSourceSP src, const PkRect &srcRect,
                                       const PkRect &dstRect, const qreal smudgeRateOpacity);

    void blendInBackgroundWithDulling(KisFixedPaintDeviceSP dst, KisColorSmudgeSourceSP src, const PkRect &dstRect,
                                      const KoColor &preparedDullingColor, const qreal smudgeRateOpacity);

protected:
    const KoCompositeOp * m_colorRateOp {nullptr};
    KoColor m_preparedDullingColor;
    const KoCompositeOp * m_smearOp {nullptr};
private:
    KisFixedPaintDeviceSP m_blendDevice;
    bool m_useDullingMode {true};
};


#endif //KRITA_KISCOLORSMUDGESTRATEGYBASE_H
