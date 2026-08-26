/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_QIMAGE_PYRAMID_H
#define __KIS_QIMAGE_PYRAMID_H

#include <PkImage.h>
#include <PkVector.h>
#include <kis_dab_shape.h>
#include <kritabrush_export.h>


class BRUSH_EXPORT KisQImagePyramid
{
public:
    KisQImagePyramid() = default;
    KisQImagePyramid(const PkImage &baseImage, bool useSmoothingForEnlarging = true);
    ~KisQImagePyramid();

    static PkSize imageSize(const PkSize &originalSize,
                           KisDabShape const&,
                           qreal subPixelX, qreal subPixelY);

    static PkSizeF characteristicSize(const PkSize &originalSize, KisDabShape const&);

    PkImage createImage(KisDabShape const&,
                       qreal subPixelX, qreal subPixelY) const;

    PkImage getClosest(PkTransform transform, qreal *scale) const;

    PkImage getClosestWithoutWorkaroundBorder(PkTransform transform, qreal *scale) const;

private:
    friend class KisGbrBrushTest;
    int findNearestLevel(qreal scale, qreal *baseScale) const;
    void appendPyramidLevel(const PkImage &image);

    static void calculateParams(KisDabShape const& shape,
                                qreal subPixelX, qreal subPixelY,
                                const PkSize &originalSize,
                                PkTransform *outputTransform, PkSize *outputSize);

    static void calculateParams(KisDabShape shape,
                                qreal subPixelX, qreal subPixelY,
                                const PkSize &originalSize,
                                qreal baseScale, const PkSize &baseSize,
                                PkTransform *outputTransform, PkSize *outputSize);

private:
    PkSize m_originalSize;
    qreal m_baseScale {0.0};

    struct PyramidLevel {
        PyramidLevel() {}
        PyramidLevel(PkImage _image, PkSize _size) : image(_image), size(_size) {}

        PkImage image;
        PkSize size;
    };

    PkVector<PyramidLevel> m_levels;
};

#endif /* __KIS_QIMAGE_PYRAMID_H */
