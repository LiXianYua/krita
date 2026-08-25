/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KRITA_UTILS_H
#define __KRITA_UTILS_H

class PkRect;
class PkRectF;
class PkSize;
class PkPen;
class PkPoint;
class PkPointF;
class PkPainterPath;
class PkBitArray;
class PkPainter;
class PkImage;
class PkRegion;
class PkTransform;
struct KisRenderedDab;
class KisRegion;

#include <PkVector.h>
#include <PkList.h>
#include "kritaimage_export.h"
#include "kis_types.h"
#include "krita_container_utils.h"
#include <functional>


namespace KritaUtils
{
    PkSize KRITAIMAGE_EXPORT optimalPatchSize();

    PkVector<PkRect> KRITAIMAGE_EXPORT splitRectIntoPatches(const PkRect &rc, const PkSize &patchSize);
    PkVector<PkRect> KRITAIMAGE_EXPORT splitRectIntoPatchesTight(const PkRect &rc, const PkSize &patchSize);
    PkVector<PkRect> KRITAIMAGE_EXPORT splitRegionIntoPatches(const PkRegion &region, const PkSize &patchSize);
    PkVector<PkRect> KRITAIMAGE_EXPORT splitRegionIntoPatches(const KisRegion &region, const PkSize &patchSize);

    KRITAIMAGE_EXPORT KisRegion splitTriangles(const PkPointF &center,
                                             const PkVector<PkPointF> &points);
    KRITAIMAGE_EXPORT KisRegion splitPath(const PkPainterPath &path);

    PkString KRITAIMAGE_EXPORT prettyFormatReal(qreal value);

    qreal KRITAIMAGE_EXPORT maxDimensionPortion(const PkRectF &bounds, qreal portion, qreal minValue);


    /**
     * Split a path \p path into a set of disjoint (non-intersectable)
     * paths if possible.
     *
     * It tries to follow odd-even fill rule, but has a small problem:
     * If you have three selections included into each other twice,
     * then the smallest selection will be included into the final subpath,
     * although it shouldn't according to odd-even-fill rule. It is still
     * to be fixed.
     */
    PkList<PkPainterPath> KRITAIMAGE_EXPORT splitDisjointPaths(const PkPainterPath &path);


    quint8 KRITAIMAGE_EXPORT mergeOpacityU8(quint8 opacity, quint8 parentOpacity);
    qreal KRITAIMAGE_EXPORT mergeOpacityF(qreal opacity, qreal parentOpacity);
    PkBitArray KRITAIMAGE_EXPORT mergeChannelFlags(const PkBitArray &flags, const PkBitArray &parentFlags);

    bool KRITAIMAGE_EXPORT compareChannelFlags(PkBitArray f1, PkBitArray f2);
    PkString KRITAIMAGE_EXPORT toLocalizedOnOff(bool value);

    KisNodeSP KRITAIMAGE_EXPORT nearestNodeAfterRemoval(KisNodeSP node);

    /**
     * When drawing a rect Qt uses quite a weird algorithm. It
     * draws 4 lines:
     *  o at X-es: rect.x() and rect.right() + 1
     *  o at Y-s: rect.y() and rect.bottom() + 1
     *
     *  Which means that bottom and right lines of the rect are painted
     *  outside the virtual rectangle the rect defines. This methods overcome this issue by
     *  painting the adjusted rect.
     */
    void KRITAIMAGE_EXPORT renderExactRect(PkPainter *p, const PkRect &rc);

    /**
     * \see renderExactRect(PkPainter *p, const PkRect &rc)
     */
    void KRITAIMAGE_EXPORT renderExactRect(PkPainter *p, const PkRect &rc, const PkPen &pen);

    PkImage KRITAIMAGE_EXPORT convertQImageToGrayA(const PkImage &image);

    void KRITAIMAGE_EXPORT applyToAlpha8Device(KisPaintDeviceSP dev, const PkRect &rc, std::function<void(quint8)> func);
    void KRITAIMAGE_EXPORT filterAlpha8Device(KisPaintDeviceSP dev, const PkRect &rc, std::function<quint8(quint8)> func);

    qreal KRITAIMAGE_EXPORT estimatePortionOfTransparentPixels(KisPaintDeviceSP dev, const PkRect &rect, qreal samplePortion);

    void KRITAIMAGE_EXPORT mirrorDab(Qt::Orientation dir, const PkPoint &center, KisRenderedDab *dab, bool skipMirrorPixels = false);
    void KRITAIMAGE_EXPORT mirrorDab(Qt::Orientation dir, const PkPointF &center, KisRenderedDab *dab, bool skipMirrorPixels = false);

    void KRITAIMAGE_EXPORT mirrorRect(Qt::Orientation dir, const PkPoint &center, PkRect *rc);
    void KRITAIMAGE_EXPORT mirrorRect(Qt::Orientation dir, const PkPointF &center, PkRect *rc);
    void KRITAIMAGE_EXPORT mirrorPoint(Qt::Orientation dir, const PkPoint &center, PkPointF *pt);
    void KRITAIMAGE_EXPORT mirrorPoint(Qt::Orientation dir, const PkPointF &center, PkPointF *pt);


    /**
     * Returns a special transformation that converts vector shape coordinates
     * ('pt') into a special coordinate space, where all path boolean operations
     * should happen.
     *
     * The problem is that Qt's path boolean operation do not support curves,
     * therefore all the curves are converted into lines
     * (见 Qt 原始 path 分段器 addPath). The curves are split into lines using
     * absolute size of the curve for the threshold. Therefore, when applying
     * boolean operations we should convert them into 'image pixel' coordinate
     * space first.
     *
     * See https://bugs.kde.org/show_bug.cgi?id=411056
     */
    PkTransform KRITAIMAGE_EXPORT pathShapeBooleanSpaceWorkaround(KisImageSP image);

    /**
     * Sometimes, when intersecting two paths, 路径类型
     * does not close some of the subpaths. It causes glitches
     * when rendering them on screen. So we should just close
     * them explicitly.
     *
     * Note: after intersecting the paths all bezier curves are
     *       already converted to polylines, so it should be safe
     *       to go through the polygons.
     *
     * See: https://bugs.kde.org/show_bug.cgi?id=408369
     */
    PkPainterPath KRITAIMAGE_EXPORT tryCloseTornSubpathsAfterIntersection(PkPainterPath path);

    enum ThresholdMode {
        ThresholdNone = 0,
        ThresholdFloor,
        ThresholdCeil,
        ThresholdMaxOut
    };

    void thresholdOpacity(KisPaintDeviceSP device, const PkRect &rect, ThresholdMode mode);
    void thresholdOpacityAlpha8(KisPaintDeviceSP device, const PkRect &rect, ThresholdMode mode);

    template <typename Visitor>
    void rasterizeHLine(const PkPoint &startPoint, const PkPoint &endPoint, Visitor visitor)
    {
        PkVector<PkPoint> points;
        int startX, endX;
        if (startPoint.x() < endPoint.x()) {
            startX = startPoint.x();
            endX = endPoint.x();
        } else {
            startX = endPoint.x();
            endX = startPoint.x();
        }
        for (int x = startX; x <= endX; ++x) {
            visitor(PkPoint(x, startPoint.y()));
        }
    }

    template <typename Visitor>
    void rasterizeVLine(const PkPoint &startPoint, const PkPoint &endPoint, Visitor visitor)
    {
        PkVector<PkPoint> points;
        int startY, endY;
        if (startPoint.y() < endPoint.y()) {
            startY = startPoint.y();
            endY = endPoint.y();
        } else {
            startY = endPoint.y();
            endY = startPoint.y();
        }
        for (int y = startY; y <= endY; ++y) {
            visitor(PkPoint(startPoint.x(), y));
        }
    }

    template <typename Visitor>
    void rasterizeLineDDA(const PkPoint &startPoint, const PkPoint &endPoint, Visitor visitor)
    {
        PkVector<PkPoint> points;

        if (startPoint == endPoint) {
            visitor(startPoint);
            return;
        }
        if (startPoint.y() == endPoint.y()) {
            rasterizeHLine(startPoint, endPoint, visitor);
            return;
        }
        if (startPoint.x() == endPoint.x()) {
            rasterizeVLine(startPoint, endPoint, visitor);
            return;
        }

        const PkPoint delta = endPoint - startPoint;
        PkPoint currentPosition = startPoint;
        PkPointF currentPositionF = startPoint;
        qreal m = static_cast<qreal>(delta.y()) / static_cast<qreal>(delta.x());
        int increment;

        if (std::abs(m) > 1.0) {
            if (delta.y() > 0) {
                m = 1.0 / m;
                increment = 1;
            } else {
                m = -1.0 / m;
                increment = -1;
            }
            while (currentPosition.y() != endPoint.y()) {
                currentPositionF.setX(currentPositionF.x() + m);
                currentPosition = PkPoint(static_cast<int>(qRound(currentPositionF.x())),
                                        currentPosition.y() + increment);
                visitor(currentPosition);
            }
        } else {
            if (delta.x() > 0) {
                increment = 1;
            } else {
                increment = -1;
                m = -m;
            }
            while (currentPosition.x() != endPoint.x()) {
                currentPositionF.setY(currentPositionF.y() + m);
                currentPosition = PkPoint(currentPosition.x() + increment,
                                        static_cast<int>(qRound(currentPositionF.y())));
                visitor(currentPosition);
            }
        }
    }

    template <typename Visitor>
    void rasterizePolylineDDA(const PkVector<PkPoint> &polylinePoints, Visitor visitor)
    {
        if (polylinePoints.size() == 0) {
            return;
        }
        if (polylinePoints.size() == 1) {
            visitor(polylinePoints.first());
            return;
        }

        // copy all points from the first segment
        rasterizeLineDDA(polylinePoints[0], polylinePoints[1], visitor);
        // for the rest of the segments, copy all points except the first one
        // (it is the same as the last point in the previous segment)
        for (int i = 1; i < polylinePoints.size() - 1; ++i) {
            int pointIndex = 0;
            rasterizeLineDDA(
                polylinePoints[i], polylinePoints[i + 1],
                [&pointIndex, &visitor](const PkPoint &point) -> void
                {
                    if (pointIndex > 0) {
                        visitor(point);
                    }
                    ++pointIndex;
                }
            );
        }
    }

    template <typename Visitor>
    void rasterizePolygonDDA(const PkVector<PkPoint> &polygonPoints, Visitor visitor)
    {
        // this is a line
        if (polygonPoints.size() < 3) {
            rasterizeLineDDA(polygonPoints.first(), polygonPoints.last(), visitor);
            return;
        }
        // rasterize all segments except the last one
        PkPoint lastSegmentStart;
        if (polygonPoints.first() == polygonPoints.last()) {
            // mid(0, size-1) == 去掉末尾重复闭合点后的子序列；PkVector 无 mid()，
            // 用「拷贝 + resize 截断」等价实现（真 Qt 容器与壳 PkVector 均成立）。
            PkVector<PkPoint> subPoints(polygonPoints);
            subPoints.resize(polygonPoints.size() - 1);
            rasterizePolylineDDA(subPoints, visitor);
            lastSegmentStart = polygonPoints[polygonPoints.size() - 2];
        } else {
            rasterizePolylineDDA(polygonPoints, visitor);
            lastSegmentStart = polygonPoints[polygonPoints.size() - 1];
        }
        // close the polygon
        {
            PkVector<PkPoint> points;
            auto addPoint = [&points](const PkPoint &point) -> void { points.append(point); };
            rasterizeLineDDA(lastSegmentStart, polygonPoints.first(), addPoint);
            for (int i = 1; i < points.size() - 1; ++i) {
                visitor(points[i]);
            }
        }
    }

    // Convenience functions
    PkVector<PkPoint> KRITAIMAGE_EXPORT rasterizeHLine(const PkPoint &startPoint, const PkPoint &endPoint);
    PkVector<PkPoint> KRITAIMAGE_EXPORT rasterizeVLine(const PkPoint &startPoint, const PkPoint &endPoint);
    PkVector<PkPoint> KRITAIMAGE_EXPORT rasterizeLineDDA(const PkPoint &startPoint, const PkPoint &endPoint);
    PkVector<PkPoint> KRITAIMAGE_EXPORT rasterizePolylineDDA(const PkVector<PkPoint> &polylinePoints);
    PkVector<PkPoint> KRITAIMAGE_EXPORT rasterizePolygonDDA(const PkVector<PkPoint> &polygonPoints);
}

#endif /* __KRITA_UTILS_H */
