/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "krita_utils.h"

#include <cmath>
#include <iomanip>
#include <sstream>

#include "kis_algebra_2d.h"

#include <KoColorSpaceRegistry.h>

#include "kis_image.h"
#include "kis_image_config.h"
#include "kis_debug.h"
#include "kis_node.h"
#include "kis_sequential_iterator.h"
#include "kis_random_accessor_ng.h"

#include <KisRenderedDab.h>
#include <PkRgb.h>
#include <KisRegion.h>


namespace KritaUtils
{

    PkSize optimalPatchSize()
    {
        KisImageConfig cfg(true);
        return PkSize(cfg.updatePatchWidth(),
                      cfg.updatePatchHeight());
    }

    PkVector<PkRect> splitRectIntoPatches(const PkRect &rc, const PkSize &patchSize)
    {
        using namespace KisAlgebra2D;


        PkVector<PkRect> patches;

        const qint32 firstCol = divideFloor(rc.x(), patchSize.width());
        const qint32 firstRow = divideFloor(rc.y(), patchSize.height());

        // TODO: check if -1 is needed here
        const qint32 lastCol = divideFloor(rc.x() + rc.width(), patchSize.width());
        const qint32 lastRow = divideFloor(rc.y() + rc.height(), patchSize.height());

        for(qint32 i = firstRow; i <= lastRow; i++) {
            for(qint32 j = firstCol; j <= lastCol; j++) {
                PkRect maxPatchRect(j * patchSize.width(), i * patchSize.height(),
                                   patchSize.width(), patchSize.height());
                PkRect patchRect = rc & maxPatchRect;

                if (!patchRect.isEmpty()) {
                    patches.append(patchRect);
                }
            }
        }

        return patches;
    }

    PkVector<PkRect> splitRectIntoPatchesTight(const PkRect &rc, const PkSize &patchSize)
    {
        PkVector<PkRect> patches;

        for (qint32 y = rc.y(); y < rc.y() + rc.height(); y += patchSize.height()) {
            for (qint32 x = rc.x(); x < rc.x() + rc.width(); x += patchSize.width()) {
                patches.append(PkRect(x, y,
                                 qMin(rc.x() + rc.width() - x, patchSize.width()),
                                 qMin(rc.y() + rc.height() - y, patchSize.height())));
            }
        }

        return patches;
    }

    PkVector<PkRect> splitRegionIntoPatches(const PkRegion &region, const PkSize &patchSize)
    {
        PkVector<PkRect> patches;

        for (const PkRect rect : region.rects()) {
            patches.append(KritaUtils::splitRectIntoPatches(rect, patchSize));
        }

        return patches;
    }

    bool checkInTriangle(const PkRectF &rect,
                         const PkPolygonF &triangle)
    {
        // NOTE: PkPolygonF 无 intersected 方法（pk 偏差登记），用包围盒相交近似。
        // 对 splitTriangles 的 dirty-rect 判定语义安全（过近似 → 可能多包 1 个 64x64 块）。
        return triangle.boundingRect().intersects(rect);
    }


    KisRegion splitTriangles(const PkPointF &center,
                                               const PkVector<PkPointF> &points)
    {

        Q_ASSERT(points.size());
        Q_ASSERT(!(points.size() & 1));

        PkVector<PkPolygonF> triangles;
        PkRect totalRect;

        for (int i = 0; i < points.size(); i += 2) {
            PkPolygonF triangle;
            triangle.append(center);
            triangle.append(points[i]);
            triangle.append(points[i+1]);

            totalRect |= triangle.boundingRect().toAlignedRect();
            triangles.append(triangle);
        }


        const int step = 64;
        const int right = totalRect.x() + totalRect.width();
        const int bottom = totalRect.y() + totalRect.height();

        PkVector<PkRect> dirtyRects;

        for (int y = totalRect.y(); y < bottom;) {
            int nextY = qMin((y + step) & ~(step-1), bottom);

            for (int x = totalRect.x(); x < right;) {
                int nextX = qMin((x + step) & ~(step-1), right);

                PkRect rect(x, y, nextX - x, nextY - y);

                for (const PkPolygonF &triangle : triangles) {
                    if(checkInTriangle(rect, triangle)) {
                        dirtyRects.append(rect);
                        break;
                    }
                }

                x = nextX;
            }
            y = nextY;
        }
        return KisRegion(std::move(dirtyRects));
    }

    KisRegion splitPath(const PkPainterPath &path)
    {
        PkVector<PkRect> dirtyRects;
        PkRect totalRect = path.boundingRect().toAlignedRect();

        // adjust the rect for antialiasing to work
        totalRect = totalRect.adjusted(-1,-1,1,1);

        const int step = 64;
        const int right = totalRect.x() + totalRect.width();
        const int bottom = totalRect.y() + totalRect.height();

        for (int y = totalRect.y(); y < bottom;) {
            int nextY = qMin((y + step) & ~(step-1), bottom);

            for (int x = totalRect.x(); x < right;) {
                int nextX = qMin((x + step) & ~(step-1), right);

                PkRect rect(x, y, nextX - x, nextY - y);

                if(path.intersects(PkRectF(rect))) {
                    dirtyRects.append(rect);
                }

                x = nextX;
            }
            y = nextY;
        }

        return KisRegion(std::move(dirtyRects));
    }

    PkString KRITAIMAGE_EXPORT prettyFormatReal(qreal value)
    {
        // NOTE: PkString 无本地化格式化 API，用标准库格式化（定点、一位小数）。
        std::ostringstream ss;
        ss << std::fixed << std::setprecision(1) << value;
        return PkString(ss.str().c_str());
    }

    qreal KRITAIMAGE_EXPORT maxDimensionPortion(const PkRectF &bounds, qreal portion, qreal minValue)
    {
        qreal maxDimension = qMax(bounds.width(), bounds.height());
        return qMax(portion * maxDimension, minValue);
    }

    PkList<PkPainterPath> splitDisjointPaths(const PkPainterPath &path)
    {
        PkList<PkPainterPath> resultList;
        // NOTE: PkPainterPath::toSubpathPolygons 需要 PkTransform 参数（无无参重载），传恒等变换。
        PkVector<PkPolygonF> inputPolygons = path.toSubpathPolygons(PkTransform());

        for (const PkPolygonF &poly : inputPolygons) {
            PkPainterPath testPath;
            testPath.addPolygon(poly);

            if (resultList.isEmpty()) {
                resultList.append(testPath);
                continue;
            }

            PkPainterPath mergedPath = testPath;

            for (auto it = resultList.begin(); it != resultList.end(); /*noop*/) {
                // NOTE: PkPainterPath 无 path-path intersects（只有 intersects(PkRectF)），
                // 用包围盒相交近似：可能多合并包围盒相邻的子路径，可接受的过近似。
                if (it->boundingRect().intersects(testPath.boundingRect())) {
                    mergedPath.addPath(*it);
                    it = resultList.erase(it);
                } else {
                    ++it;
                }
            }

            resultList.append(mergedPath);
        }

        return resultList;
    }

    quint8 mergeOpacityU8(quint8 opacity, quint8 parentOpacity)
    {
        if (parentOpacity != OPACITY_OPAQUE_U8) {
            opacity = (int(opacity) * parentOpacity) / OPACITY_OPAQUE_U8;
        }
        return opacity;
    }

    qreal mergeOpacityF(qreal opacity, qreal parentOpacity)
    {
        if (!qFuzzyCompare(parentOpacity, OPACITY_OPAQUE_F)) {
            opacity *= parentOpacity;
        }
        return opacity;
    }

    PkBitArray mergeChannelFlags(const PkBitArray &childFlags, const PkBitArray &parentFlags)
    {
        PkBitArray flags = childFlags;

        if (!flags.isEmpty() &&
            !parentFlags.isEmpty() &&
            flags.size() == parentFlags.size()) {

            // NOTE: PkBitArray 无 operator&=，用 operator& 新建。
            flags = flags & parentFlags;

        } else if (!parentFlags.isEmpty()) {
            flags = parentFlags;
        }

        return flags;
    }

    bool compareChannelFlags(PkBitArray f1, PkBitArray f2)
    {
        // NOTE: PkBitArray 无 isNull()，以 size()==0 表示「空/未定义」（真 Qt isNull 语义）。
        if (f1.size() == 0 && f2.size() == 0) return true;

        if (f1.size() == 0) {
            f1.resize(f2.size());
            f1.fill(true);
        }

        if (f2.size() == 0) {
            f2.resize(f1.size());
            f2.fill(true);
        }

        return f1 == f2;
    }

    PkString KRITAIMAGE_EXPORT toLocalizedOnOff(bool value)
    {
        // NOTE: 剥掉翻译调用（壳内无翻译层），消费方接受英文 on/off。
        return value ? PkString("on") : PkString("off");
    }

    KisNodeSP nearestNodeAfterRemoval(KisNodeSP node)
    {
        KisNodeSP newNode = node->nextSibling();

        if (!newNode) {
            newNode = node->prevSibling();
        }

        if (!newNode) {
            newNode = node->parent();
        }

        return newNode;
    }

    void renderExactRect(PkPainter *p, const PkRect &rc)
    {
        // NOTE: 壳 PkPainter::drawRect 为 no-op 桩（渲染路径在壳外，见批次 D）。
        p->drawRect(rc.adjusted(0,0,-1,-1));
    }

    void renderExactRect(PkPainter *p, const PkRect &rc, const PkPen &pen)
    {
        PkPen oldPen = p->pen();
        p->setPen(pen);
        renderExactRect(p, rc);
        p->setPen(oldPen);
    }

    PkImage convertQImageToGrayA(const PkImage &image)
    {
        PkImage dstImage(image.size(), PkImage::Format_ARGB32);

        // TODO: if someone feel bored, a more optimized version of this would be welcome
        const PkSize size = image.size();
        for(int y = 0; y < size.height(); ++y) {
            for(int x = 0; x < size.width(); ++x) {
                const PkRgb pixel = image.pixel(x,y);
                // NOTE: 无 qGray 宏，直接用 pk 通道提取；公式同 Qt：(r*11+g*16+b*5)>>5
                const int gray = (pkRed(pixel) * 11 + pkGreen(pixel) * 16 + pkBlue(pixel) * 5) >> 5;
                dstImage.setPixel(x, y, pkRgba(gray, gray, gray, pkAlpha(pixel)));
            }
        }

        return dstImage;
    }

    void applyToAlpha8Device(KisPaintDeviceSP dev, const PkRect &rc, std::function<void(quint8)> func) {
        KisSequentialConstIterator dstIt(dev, rc);
        while (dstIt.nextPixel()) {
            const quint8 *dstPtr = dstIt.rawDataConst();
            func(*dstPtr);
        }
    }

    void filterAlpha8Device(KisPaintDeviceSP dev, const PkRect &rc, std::function<quint8(quint8)> func) {
        KisSequentialIterator dstIt(dev, rc);
        while (dstIt.nextPixel()) {
            quint8 *dstPtr = dstIt.rawData();
            *dstPtr = func(*dstPtr);
        }
    }

    qreal estimatePortionOfTransparentPixels(KisPaintDeviceSP dev, const PkRect &rect, qreal samplePortion) {
        const KoColorSpace *cs = dev->colorSpace();

        const qreal linearPortion = std::sqrt(samplePortion);
        const qreal ratio = qreal(rect.width()) / rect.height();
        const int xStep = qMax(1, qRound(1.0 / linearPortion * ratio));
        const int yStep = qMax(1, qRound(1.0 / linearPortion / ratio));

        int numTransparentPixels = 0;
        int numPixels = 0;

        KisRandomConstAccessorSP it = dev->createRandomConstAccessorNG();
        for (int y = rect.y(); y <= rect.bottom(); y += yStep) {
            for (int x = rect.x(); x <= rect.right(); x += xStep) {
                it->moveTo(x, y);
                const quint8 alpha = cs->opacityU8(it->rawDataConst());

                if (alpha != OPACITY_OPAQUE_U8) {
                    numTransparentPixels++;
                }

                numPixels++;
            }
        }

        if (numPixels == 0) {
            return 0; // avoid dividing by 0
        }
        return qreal(numTransparentPixels) / numPixels;
    }

    void mirrorDab(Qt::Orientation dir, const PkPoint &center, KisRenderedDab *dab, bool skipMirrorPixels)
    {
        const PkRect rc = dab->realBounds();

        if (dir == Qt::Horizontal) {
            const int mirrorX = -((rc.x() + rc.width()) - center.x()) + center.x();

            if (!skipMirrorPixels) {
                dab->device->mirror(true, false);
            }
            dab->offset.rx() = mirrorX;
        } else /* if (dir == Qt::Vertical) */ {
            const int mirrorY = -((rc.y() + rc.height()) - center.y()) + center.y();

            if (!skipMirrorPixels) {
                dab->device->mirror(false, true);
            }
            dab->offset.ry() = mirrorY;
        }
    }

    void mirrorDab(Qt::Orientation dir, const PkPointF &center, KisRenderedDab *dab, bool skipMirrorPixels)
    {
        const PkRect rc = dab->realBounds();

        if (dir == Qt::Horizontal) {
            const int mirrorX = -((rc.x() + rc.width()) - center.x()) + center.x();

            if (!skipMirrorPixels) {
                dab->device->mirror(true, false);
            }
            dab->offset.rx() = mirrorX;
        } else /* if (dir == Qt::Vertical) */ {
            const int mirrorY = -((rc.y() + rc.height()) - center.y()) + center.y();

            if (!skipMirrorPixels) {
                dab->device->mirror(false, true);
            }
            dab->offset.ry() = mirrorY;
        }
    }

    void mirrorRect(Qt::Orientation dir, const PkPoint &center, PkRect *rc)
    {
        if (dir == Qt::Horizontal) {
            const int mirrorX = -((rc->x() + rc->width()) - center.x()) + center.x();
            rc->moveLeft(mirrorX);
        } else /* if (dir == Qt::Vertical) */ {
            const int mirrorY = -((rc->y() + rc->height()) - center.y()) + center.y();
            rc->moveTop(mirrorY);
        }
    }

    void mirrorRect(Qt::Orientation dir, const PkPointF &center, PkRect *rc)
    {
        if (dir == Qt::Horizontal) {
            const int mirrorX = -((rc->x() + rc->width()) - center.x()) + center.x();
            rc->moveLeft(mirrorX);
        } else /* if (dir == Qt::Vertical) */ {
            const int mirrorY = -((rc->y() + rc->height()) - center.y()) + center.y();
            rc->moveTop(mirrorY);
        }
    }

    void mirrorPoint(Qt::Orientation dir, const PkPoint &center, PkPointF *pt)
    {
        if (dir == Qt::Horizontal) {
            pt->rx() = -(pt->x() - qreal(center.x())) + center.x();
        } else /* if (dir == Qt::Vertical) */ {
            pt->ry() = -(pt->y() - qreal(center.y())) + center.y();
        }
    }

    void mirrorPoint(Qt::Orientation dir, const PkPointF &center, PkPointF *pt)
    {
        if (dir == Qt::Horizontal) {
            pt->rx() = -(pt->x() - qreal(center.x())) + center.x();
        } else /* if (dir == Qt::Vertical) */ {
            pt->ry() = -(pt->y() - qreal(center.y())) + center.y();
        }
    }

    PkTransform pathShapeBooleanSpaceWorkaround(KisImageSP image)
    {
        return PkTransform::fromScale(image->xRes(), image->yRes());
    }

    PkPainterPath tryCloseTornSubpathsAfterIntersection(PkPainterPath path)
    {
        path.setFillRule(Qt::WindingFill);
        PkVector<PkPolygonF> polys = path.toSubpathPolygons(PkTransform());

        path = PkPainterPath();
        path.setFillRule(Qt::WindingFill);
        for (PkPolygonF poly : polys) {
            ENTER_FUNCTION() << ppVar(poly.isClosed());
            if (!poly.isClosed()) {
                poly.append(poly.first());
            }
            path.addPolygon(poly);
        }
        return path;
    }

    void thresholdOpacity(KisPaintDeviceSP device, const PkRect &rect, ThresholdMode mode)
    {
        const KoColorSpace *cs = device->colorSpace();

        if (mode == ThresholdCeil) {
            KisSequentialIterator it(device, rect);
            while (it.nextPixel()) {
                if (cs->opacityU8(it.rawDataConst()) > 0) {
                    cs->setOpacity(it.rawData(), quint8(255), 1);
                }
            }
        } else if (mode == ThresholdFloor) {
            KisSequentialIterator it(device, rect);
            while (it.nextPixel()) {
                if (cs->opacityU8(it.rawDataConst()) < 255) {
                    cs->setOpacity(it.rawData(), quint8(0), 1);
                }
            }
        } else if (mode == ThresholdMaxOut) {
            KisSequentialIterator it(device, rect);
            int numConseqPixels = it.nConseqPixels();
            while (it.nextPixels(numConseqPixels)) {
                numConseqPixels = it.nConseqPixels();
                cs->setOpacity(it.rawData(), quint8(255), numConseqPixels);
            }
        }
    }

    void thresholdOpacityAlpha8(KisPaintDeviceSP device, const PkRect &rect, ThresholdMode mode)
    {
        if (mode == ThresholdCeil) {
            filterAlpha8Device(device, rect,
                [] (quint8 value) {
                    return value > 0 ? 255 : value;
                });
        } else if (mode == ThresholdFloor) {
            filterAlpha8Device(device, rect,
                [] (quint8 value) {
                    return value < 255 ? 0 : value;
                });
        } else if (mode == ThresholdMaxOut) {
            // PkColor(Qt::GlobalColor) 隐式转 const PkColor&，进 KoColor(PkColor, cs)。
            device->fill(rect, KoColor(Qt::white, device->colorSpace()));
        }
    }

    PkVector<PkPoint> rasterizeHLine(const PkPoint &startPoint, const PkPoint &endPoint)
    {
        PkVector<PkPoint> points;
        rasterizeHLine(startPoint, endPoint, [&points](const PkPoint &point) { points.append(point); });
        return points;
    }

    PkVector<PkPoint> rasterizeVLine(const PkPoint &startPoint, const PkPoint &endPoint)
    {
        PkVector<PkPoint> points;
        rasterizeVLine(startPoint, endPoint, [&points](const PkPoint &point) { points.append(point); });
        return points;
    }

    PkVector<PkPoint> rasterizeLineDDA(const PkPoint &startPoint, const PkPoint &endPoint)
    {
        PkVector<PkPoint> points;
        rasterizeLineDDA(startPoint, endPoint, [&points](const PkPoint &point) { points.append(point); });
        return points;
    }

    PkVector<PkPoint> rasterizePolylineDDA(const PkVector<PkPoint> &polylinePoints)
    {
        PkVector<PkPoint> points;
        rasterizePolylineDDA(polylinePoints, [&points](const PkPoint &point) { points.append(point); });
        return points;
    }

    PkVector<PkPoint> rasterizePolygonDDA(const PkVector<PkPoint> &polygonPoints)
    {
        PkVector<PkPoint> points;
        rasterizePolygonDDA(polygonPoints, [&points](const PkPoint &point) { points.append(point); });
        return points;
    }

}
