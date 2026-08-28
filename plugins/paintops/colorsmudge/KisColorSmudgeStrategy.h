/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KRITA_KISCOLORSMUDGESTRATEGY_H
#define KRITA_KISCOLORSMUDGESTRATEGY_H

#include <KisOptimizedByteArray.h>
#include <kis_dab_cache.h>

class KisColorSmudgeStrategy
{
public:
    KisColorSmudgeStrategy();

    virtual ~KisColorSmudgeStrategy() = default;

    virtual void initializePainting() = 0;

    virtual void updateMask(KisDabCache *dabCache,
                            const KisPaintInformation& info,
                            const KisDabShape &shape,
                            const PkPointF &cursorPoint,
                            PkRect *dstDabRect,
                            qreal lightnessStrength) = 0;

    virtual PkVector<PkRect> paintDab(const PkRect &srcRect, const PkRect &dstRect,
                                    const KoColor &currentPaintColor,
                                    qreal opacity,
                                    qreal colorRateValue,
                                    qreal smudgeRateValue,
                                    qreal maxPossibleSmudgeRateValue,
                                    qreal lightnessStrengthValue,
                                    qreal smudgeRadiusValue) = 0;

    virtual const KoColorSpace* preciseColorSpace() const = 0;

protected:
    KisOptimizedByteArray::MemoryAllocatorSP m_memoryAllocator;
};


#endif //KRITA_KISCOLORSMUDGESTRATEGY_H
