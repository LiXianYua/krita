/*
 *  SPDX-FileCopyrightText: 2004 Adrian Page <adrian@pagenet.plus.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_GRADIENT_PAINTER_H_
#define KIS_GRADIENT_PAINTER_H_

#include <PkScopedPointer.h>

#include <KoColor.h>

#include "kis_types.h"
#include "kis_painter.h"

#include <kritaimage_export.h>


/**
 *  XXX: Docs!
 */
class KRITAIMAGE_EXPORT KisGradientPainter : public KisPainter
{

public:

    KisGradientPainter();
    KisGradientPainter(KisPaintDeviceSP device);
    KisGradientPainter(KisPaintDeviceSP device, KisSelectionSP selection);

    ~KisGradientPainter() override;

    enum enumGradientShape {
        GradientShapeLinear,
        GradientShapeBiLinear,
        GradientShapeRadial,
        GradientShapeSquare,
        GradientShapeConical,
        GradientShapeConicalSymetric,
        GradientShapeSpiral,
        GradientShapeReverseSpiral,
        GradientShapePolygonal
    };

    enum enumGradientRepeat {
        GradientRepeatNone,
        GradientRepeatForwards,
        GradientRepeatAlternate
    };

    void setGradientShape(enumGradientShape shape);

    void precalculateShape();

    /**
     * Paint a gradient in the rect between startx, starty, width and height.
     */
    bool paintGradient(const PkPointF& gradientVectorStart,
                       const PkPointF& gradientVectorEnd,
                       enumGradientRepeat repeat,
                       double antiAliasThreshold,
                       bool reverseGradient,
                       qint32 startx,
                       qint32 starty,
                       qint32 width,
                       qint32 height,
                       bool useDithering = false);

    // convenience overload
    bool paintGradient(const PkPointF& gradientVectorStart,
                       const PkPointF& gradientVectorEnd,
                       enumGradientRepeat repeat,
                       double antiAliasThreshold,
                       bool reverseGradient,
                       const PkRect &applyRect,
                       bool useDithering = false);

    template <class T> 
    bool paintGradient(const PkPointF& gradientVectorStart,
                       const PkPointF& gradientVectorEnd,
                       enumGradientRepeat repeat,
                       double antiAliasThreshold,
                       bool reverseGradient,
                       bool useDithering,
                       const PkRect &applyRect,
                       T & paintPolicy);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};
#endif //KIS_GRADIENT_PAINTER_H_
