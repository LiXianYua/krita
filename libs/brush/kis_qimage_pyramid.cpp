/*
 *  SPDX-FileCopyrightText: 2013 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_qimage_pyramid.h"

#include <algorithm>
#include <cstring>
#include <limits>
#include <PkRgb.h>
#include <kis_debug.h>

#define MIPMAP_SIZE_THRESHOLD 512
#define MAX_MIPMAP_SCALE 8.0

#define QPAINTER_WORKAROUND_BORDER 1

namespace {

PkImage copyArgb32Rect(const PkImage &source, int x, int y, int width, int height)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(source.format() == PkImage::Format_ARGB32, PkImage());

    PkImage result(width, height, PkImage::Format_ARGB32);
    result.fill(0);

    const int sourceLeft = std::max(0, x);
    const int sourceTop = std::max(0, y);
    const int sourceRight = std::min(source.width(), x + width);
    const int sourceBottom = std::min(source.height(), y + height);

    if (sourceLeft >= sourceRight || sourceTop >= sourceBottom) {
        return result;
    }

    const std::size_t rowBytes = static_cast<std::size_t>(sourceRight - sourceLeft) * sizeof(PkRgb);
    const int destinationX = sourceLeft - x;
    for (int sourceY = sourceTop; sourceY < sourceBottom; ++sourceY) {
        const auto *sourceRow = source.constScanLine(sourceY) + sourceLeft * sizeof(PkRgb);
        auto *destinationRow = result.scanLine(sourceY - y) + destinationX * sizeof(PkRgb);
        std::memcpy(destinationRow, sourceRow, rowBytes);
    }

    return result;
}

}


KisQImagePyramid::KisQImagePyramid(const PkImage &baseImage, bool useSmoothingForEnlarging)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!baseImage.isNull());

    m_originalSize = baseImage.size();


    qreal scale = MAX_MIPMAP_SCALE;

    while (scale > 1.0) {
        PkSize scaledSize = m_originalSize * scale;

        if (scaledSize.width() <= MIPMAP_SIZE_THRESHOLD ||
                scaledSize.height() <= MIPMAP_SIZE_THRESHOLD) {

            if (m_levels.isEmpty()) {
                m_baseScale = scale;
            }

            if (useSmoothingForEnlarging) {
                appendPyramidLevel(baseImage.scaled(scaledSize,  Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            } else {
                appendPyramidLevel(baseImage.scaled(scaledSize,  Qt::IgnoreAspectRatio, Qt::FastTransformation));
            }
        }

        scale *= 0.5;
    }

    if (m_levels.isEmpty()) {
        m_baseScale = 1.0;
    }
    appendPyramidLevel(baseImage);

    scale = 0.5;
    while (true) {
        PkSize scaledSize = m_originalSize * scale;

        if (scaledSize.width() == 0 ||
                scaledSize.height() == 0) break;

        appendPyramidLevel(baseImage.scaled(scaledSize,  Qt::IgnoreAspectRatio, Qt::SmoothTransformation));

        scale *= 0.5;
    }
}

KisQImagePyramid::~KisQImagePyramid()
{
}

int KisQImagePyramid::findNearestLevel(qreal scale, qreal *baseScale) const
{
    const qreal scale_epsilon = 1e-6;

    qreal levelScale = m_baseScale;
    int level = 0;
    int lastLevel = m_levels.size() - 1;


    while ((0.5 * levelScale > scale ||
            qAbs(0.5 * levelScale - scale) < scale_epsilon) &&
            level < lastLevel) {

        levelScale *= 0.5;
        level++;
    }

    *baseScale = levelScale;
    return level;
}

inline PkRect roundRect(const PkRectF &rc)
{
    /**
     * This is an analog of toAlignedRect() with the only difference
     * that it ensures the rect position will never be below zero.
     *
     * Warning: be *very* careful with using bottom()/right() values
     *          of a pure PkRect (we don't use it here for the dangers
     *          it can lead to).
     */

    PkRectF rect(rc);

    KIS_SAFE_ASSERT_RECOVER_NOOP(rect.x() > -0.000001);
    KIS_SAFE_ASSERT_RECOVER_NOOP(rect.y() > -0.000001);

    if (rect.x() < 0.000001) {
        rect.setLeft(0.0);
    }

    if (rect.y() < 0.000001) {
        rect.setTop(0.0);
    }

    qreal w_rounded = qRound(rect.width());
    qreal h_rounded = qRound(rect.height());

    //Take care of the float precision errors
    if (qAbs(rect.width() - w_rounded) < 0.000001) {
        rect.setWidth(w_rounded);
    }

    if (qAbs(rect.height() - h_rounded) < 0.000001) {
        rect.setHeight(h_rounded);
    }


    return rect.toAlignedRect();
}

PkTransform baseBrushTransform(KisDabShape const& shape,
                              qreal subPixelX, qreal subPixelY,
                              const PkRectF &baseBounds)
{
    PkTransform transform;
    transform.scale(shape.scaleX(), shape.scaleY());

    if (!qFuzzyCompare(shape.rotation(), 0) && !qIsNaN(shape.rotation())) {
        transform = transform * PkTransform().rotateRadians(shape.rotation());
        PkRectF rotatedBounds = transform.mapRect(baseBounds);
        transform = transform * PkTransform::fromTranslate(-rotatedBounds.x(), -rotatedBounds.y());
    }

    return transform * PkTransform::fromTranslate(subPixelX, subPixelY);
}

void KisQImagePyramid::calculateParams(KisDabShape const& shape,
                                       qreal subPixelX, qreal subPixelY,
                                       const PkSize &originalSize,
                                       PkTransform *outputTransform, PkSize *outputSize)
{
    calculateParams(shape,
                    subPixelX, subPixelY,
                    originalSize, 1.0, originalSize,
                    outputTransform, outputSize);
}

void KisQImagePyramid::calculateParams(KisDabShape shape,
                                       qreal subPixelX, qreal subPixelY,
                                       const PkSize &originalSize,
                                       qreal baseScale, const PkSize &baseSize,
                                       PkTransform *outputTransform, PkSize *outputSize)
{
    Q_UNUSED(baseScale);

    PkRectF originalBounds = PkRectF(PkPointF(), originalSize);
    PkTransform originalTransform = baseBrushTransform(shape, subPixelX, subPixelY, originalBounds);

    qreal realBaseScaleX = qreal(baseSize.width()) / originalSize.width();
    qreal realBaseScaleY = qreal(baseSize.height()) / originalSize.height();
    qreal scaleX = shape.scaleX() / realBaseScaleX;
    qreal scaleY = shape.scaleY() / realBaseScaleY;
    shape = KisDabShape(scaleX, scaleY/scaleX, shape.rotation());

    PkRectF baseBounds = PkRectF(PkPointF(), baseSize);
    PkTransform transform = baseBrushTransform(shape, subPixelX, subPixelY, baseBounds);
    PkRectF mappedRect = originalTransform.mapRect(originalBounds);

    // Set up a 0,0,1,1 size and identity transform in case the transform fails to
    // produce a usable result.
    int width = 1;
    int height = 1;
    *outputTransform = PkTransform();

    if (mappedRect.isValid()) {
        PkRect expectedDstRect = roundRect(mappedRect);

#if 0 // Only enable when debugging; users shouldn't see this warning
        {
            PkRect testingRect = roundRect(transform.mapRect(baseBounds));
            if (testingRect != expectedDstRect) {
                warnKrita << "WARNING: expected and real dab rects do not coincide!";
                warnKrita << "         expected rect:" << expectedDstRect;
                warnKrita << "         real rect:    " << testingRect;
            }
        }
#endif
        KIS_SAFE_ASSERT_RECOVER_NOOP(expectedDstRect.x() >= 0);
        KIS_SAFE_ASSERT_RECOVER_NOOP(expectedDstRect.y() >= 0);

        width = expectedDstRect.x() + expectedDstRect.width();
        height = expectedDstRect.y() + expectedDstRect.height();

        // we should not return invalid image, so adjust the image to be
        // at least 1 px in size.
        width = qMax(1, width);
        height = qMax(1, height);
    }
    else {
#if 0 // Only enable when debugging; users shouldn't see this warning
        qWarning() << "Brush transform generated an invalid rectangle!"
            << ppVar(shape.scaleX()) << ppVar(shape.scaleY()) << ppVar(shape.rotation())
            << ppVar(subPixelX) << ppVar(subPixelY)
            << ppVar(originalSize)
            << ppVar(baseScale)
            << ppVar(baseSize)
            << ppVar(baseBounds)
            << ppVar(mappedRect);
#endif
    }

    *outputTransform = transform;
    *outputSize = PkSize(width, height);
}

PkSize KisQImagePyramid::imageSize(const PkSize &originalSize,
                                  KisDabShape const& shape,
                                  qreal subPixelX, qreal subPixelY)
{
    PkTransform transform;
    PkSize dstSize;

    calculateParams(shape, subPixelX, subPixelY,
                    originalSize,
                    &transform, &dstSize);

    return dstSize;
}

PkSizeF KisQImagePyramid::characteristicSize(const PkSize &originalSize,
                                            KisDabShape const& shape)
{
    PkRectF originalRect(PkPointF(), originalSize);
    PkTransform transform = baseBrushTransform(shape,
                                              0.0, 0.0,
                                              originalRect);

    return transform.mapRect(originalRect).size();
}

void KisQImagePyramid::appendPyramidLevel(const PkImage &image)
{
    /**
     * QPainter has a bug: when doing a transformation it decides that
     * all the pixels outside of the image (source rect) are equal to
     * the border pixels (CLAMP in terms of openGL). This means that
     * there will be no smooth scaling on the border of the image when
     * it is rotated.  To workaround this bug we need to add one pixel
     * wide border to the image, so that it transforms smoothly.
     *
     * See a unittest in: KisGbrBrushTest::testQPainterTransformationBorder
     */
    
PkSize levelSize = image.size();
    PkImage tmp = image.convertToFormat(PkImage::Format_ARGB32);
    tmp = copyArgb32Rect(tmp,
                        -QPAINTER_WORKAROUND_BORDER,
                        -QPAINTER_WORKAROUND_BORDER,
                        image.width() + 2 * QPAINTER_WORKAROUND_BORDER,
                        image.height() + 2 * QPAINTER_WORKAROUND_BORDER);
    m_levels.append(PyramidLevel(tmp, levelSize));
}

PkImage KisQImagePyramid::createImage(KisDabShape const& shape,
                                     qreal subPixelX, qreal subPixelY) const
{
    if (m_levels.isEmpty()) return PkImage();

    qreal baseScale = -1.0;
    int level = findNearestLevel(shape.scale(), &baseScale);

    const PkImage &srcImage = m_levels[level].image;

    PkTransform transform;
    PkSize dstSize;

    calculateParams(shape, subPixelX, subPixelY,
                    m_originalSize, baseScale, m_levels[level].size,
                    &transform, &dstSize);

    if (transform.isIdentity() &&
            srcImage.format() == PkImage::Format_ARGB32) {

        return copyArgb32Rect(srcImage,
                             QPAINTER_WORKAROUND_BORDER,
                             QPAINTER_WORKAROUND_BORDER,
                             srcImage.width() - 2 * QPAINTER_WORKAROUND_BORDER,
                             srcImage.height() - 2 * QPAINTER_WORKAROUND_BORDER);
    }

    /**
     * QPainter has one more bug: when a PkTransform is TxTranslate, it
     * does wrong sampling (probably, Nearest Neighbour) even though
     * we tell it directly that we need SmoothPixmapTransform.
     *
     * So here is a workaround: we set a negligible scale to convince
     * Qt we use a non-only-translating transform.
     */
    while (transform.type() == PkTransform::TxTranslate) {
        const qreal scale = transform.m11();
        const qreal fakeScale = scale - 10 * std::numeric_limits<qreal>::epsilon();
        transform *= PkTransform::fromScale(fakeScale, fakeScale);
    }

    const PkTransform effectiveTransform =
        PkTransform::fromTranslate(-QPAINTER_WORKAROUND_BORDER,
                                   -QPAINTER_WORKAROUND_BORDER) * transform;
    const PkRect transformedBounds = effectiveTransform.mapRect(srcImage.rect());
    const PkImage transformedImage =
        srcImage.transformed(effectiveTransform, Qt::SmoothTransformation);

    return copyArgb32Rect(transformedImage,
                         -transformedBounds.x(),
                         -transformedBounds.y(),
                         dstSize.width(),
                         dstSize.height());
}

PkImage KisQImagePyramid::getClosest(PkTransform transform, qreal *scale) const
{
    if (m_levels.isEmpty()) return PkImage();

    // Estimate scale
    PkSizeF transformedUnitSquare = transform.mapRect(PkRectF(0, 0, 1, 1)).size();
    qreal x = qAbs(transformedUnitSquare.width());
    qreal y = qAbs(transformedUnitSquare.height());
    qreal estimatedScale = (x > y) ? transformedUnitSquare.width() : transformedUnitSquare.height();

    int level = findNearestLevel(estimatedScale, scale);
    return m_levels[level].image;
}

PkImage KisQImagePyramid::getClosestWithoutWorkaroundBorder(PkTransform transform, qreal *scale) const
{
    PkImage image = getClosest(transform, scale);
    return copyArgb32Rect(image,
                         QPAINTER_WORKAROUND_BORDER,
                         QPAINTER_WORKAROUND_BORDER,
                         image.width() - 2 * QPAINTER_WORKAROUND_BORDER,
                         image.height() - 2 * QPAINTER_WORKAROUND_BORDER);
}
