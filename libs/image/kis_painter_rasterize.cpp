/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2008-2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ============================================================================
// [GAP] 登记（2026-08-24 修复轮 C1，S-06 全分支评审 Critical）
//
// 本文件自 libs/image/kis_painter.cc 拆出的**路径填充/绘制族**，共 13 个函数：
//   KisPainter::Private::fillPainterPathImpl
//   KisPainter::drawPainterPath（两个重载）
//   KisPainter::fillPainterPath（两个重载）
//   KisPainter::fillPolygon / paintPolygon / paintPainterPath
//   KisPainter::paintPolyline / paintRect（两个重载）/ paintEllipse（两个重载）
//
// 这一族的共同落点是 fillPainterPathImpl/drawPainterPath 的**路径栅格化**：
// maskPainter->fillPath(path, brush) / drawPath(path) 把路径画进遮罩位图，
// 再以 applyAlphaU8Mask 把覆盖率叠到多边形填充结果上；paintRect/paintEllipse/
// paintPolyline/paintPainterPath 等只是先算出点列/路径再汇入 paintPolygon →
// fillPainterPath → 栅格化。薄壳内 PkPainter 是 no-op 桩（shell compat/QPainter，
// fillPath/drawPath 空实现）：遮罩保持 fill(black) 全不透明黑 → qRed()==0 →
// applyAlphaU8Mask(alpha=0) → 多边形保持包围盒填充。这是**语义降级**而非崩溃，
// 与批次 A/F2c 对同一降级的判法（不可接受、维持 GAP）一致（task8-report 批次
// A/F2c 段）。壳内以 --no-undefined 链接，任何壳内函数引用本族符号都会链接失败，
// 因此整族必须一起离开薄壳；留在 kis_painter.cc 的 getBezierCurvePoints（static
// + 成员）是纯几何辅助、无 Qt 依赖，留在壳内，仅被 GAP 侧 paintEllipse 调用。
//
// 关闭条件：KisFillPainter 重实现路径栅格化后，将本文件加入薄壳 SHELL_SOURCES
// 回编。PkPainter/PkPen/PkBrush 与 qRed/Q_CHECK_PTR/Q_FALLTHROUGH 等宏由壳
// compat PkCompatAll.h 预激活提供（-include PkCompatAll.h），随 SHELL_SOURCES
// 编译即齐。回编时注意：
//   - 栅格化状态 polygonMaskImage/maskPainter 仍留在 KisPainterPrivate（壳侧
//     析构 delete d->maskPainter 与 setMaskImageSize 重置均引用，非仅本文件使用，
//     按任务「若只被拆出的函数用」条件不迁）；
//   - 原 Qt QPainter 实现（QPainterPath/QPainter/QImage/QPen/QBrush 签名）见
//     官方 clone /home/liyang/projects-ssd/krita libs/image/kis_painter.cc
//     L1427-1634（v6.0.3 同源），语义参照以它为准；本文件为剥 Qt 后的 Pk 形态。
// ============================================================================

#include "kis_painter.h"
#include "kis_painter_p.h"
#include "kis_algebra_2d.h"   // KisAlgebra2D::directionBetweenPoints（paintPolygon/paintPolyline 用）

#include "kis_iterator_ng.h"

#include <PkImage.h>
#include <PkRect.h>
#include <PkPainterPath.h>

// 下列符号由薄壳 compat 层提供，回编时随 PkCompatAll.h 预激活：
//   PkPainter / PkPen / PkBrush —— shell compat/QPainter QPen QBrush 三桩
//   qRed —— compat/QRgb
//   Q_CHECK_PTR / Q_FALLTHROUGH —— PkCompatAll.h 宏
//   qMin / qRound / qBound —— PkGlobal.h

void KisPainter::Private::fillPainterPathImpl(const PkPainterPath& path, const PkRect &requestedRect)
{
    if (fillStyle == FillStyleNone) {
        return;
    }

    // Fill the polygon bounding rectangle with the required contents then we'll
    // create a mask for the actual polygon coverage.

    if (!fillPainter) {
        polygon = device->createCompositionSourceDevice();
        fillPainter = new KisFillPainter(polygon);
    } else {
        polygon->clear();
    }

    Q_CHECK_PTR(polygon);

    PkRectF boundingRect = path.boundingRect();
    PkRect fillRect = boundingRect.toAlignedRect();

    // Expand the rectangle to allow for anti-aliasing.
    fillRect.adjust(-1, -1, 1, 1);

    if (requestedRect.isValid()) {
        fillRect &= requestedRect;
    }

    switch (fillStyle) {
    default:
        Q_FALLTHROUGH();
    case FillStyleForegroundColor:
        fillPainter->fillRect(fillRect, q->paintColor(), OPACITY_OPAQUE_U8);
        break;
    case FillStyleBackgroundColor:
        fillPainter->fillRect(fillRect, q->backgroundColor(), OPACITY_OPAQUE_U8);
        break;
    case FillStylePattern:
        if (pattern) { // if the user hasn't got any patterns installed, we shouldn't crash...
            fillPainter->fillRectNoCompose(fillRect, pattern, patternTransform);
        }
        break;
    case FillStyleGenerator:
        if (generator) { // if the user hasn't got any generators, we shouldn't crash...
            fillPainter->fillRect(fillRect.x(), fillRect.y(), fillRect.width(), fillRect.height(), q->generator());
        }
        break;
    }

    if (polygonMaskImage.isNull() || (maskPainter == 0)) {
        polygonMaskImage = PkImage(maskImageWidth, maskImageHeight, PkImage::Format_ARGB32_Premultiplied);
        maskPainter = new PkPainter(&polygonMaskImage);
        maskPainter->setRenderHint(PkPainter::Antialiasing, q->antiAliasPolygonFill());
    }

    // Break the mask up into chunks so we don't have to allocate a potentially very large PkImage.
    const PkColor black(Qt::black);
    const PkBrush brush(Qt::white);
    for (qint32 x = fillRect.x(); x < fillRect.x() + fillRect.width(); x += maskImageWidth) {
        for (qint32 y = fillRect.y(); y < fillRect.y() + fillRect.height(); y += maskImageHeight) {

            polygonMaskImage.fill(black.rgb());
            maskPainter->translate(-x, -y);
            maskPainter->fillPath(path, brush);
            maskPainter->translate(x, y);

            qint32 rectWidth = qMin(fillRect.x() + fillRect.width() - x, maskImageWidth);
            qint32 rectHeight = qMin(fillRect.y() + fillRect.height() - y, maskImageHeight);

            KisHLineIteratorSP lineIt = polygon->createHLineIteratorNG(x, y, rectWidth);

            quint8 tmp;
            for (int row = y; row < y + rectHeight; row++) {
                PkRgb* line = reinterpret_cast<PkRgb*>(polygonMaskImage.scanLine(row - y));
                do {
                    tmp = qRed(line[lineIt->x() - x]);
                    polygon->colorSpace()->applyAlphaU8Mask(lineIt->rawData(), &tmp, 1);
                } while (lineIt->nextPixel());
                lineIt->nextRow();
            }

        }
    }

    PkRect bltRect = !requestedRect.isEmpty() ? requestedRect : fillRect;
    q->bitBlt(bltRect.x(), bltRect.y(), polygon, bltRect.x(), bltRect.y(), bltRect.width(), bltRect.height());
}

void KisPainter::drawPainterPath(const PkPainterPath& path, const PkPen& pen)
{
    drawPainterPath(path, pen, PkRect());
}

void KisPainter::drawPainterPath(const PkPainterPath& path, const PkPen& _pen, const PkRect &requestedRect)
{
    PkPen pen(_pen);
    pen.setColor(Qt::white);

    if (!d->fillPainter) {
        d->polygon = d->device->createCompositionSourceDevice();
        d->fillPainter = new KisFillPainter(d->polygon);
    } else {
        d->polygon->clear();
    }

    Q_CHECK_PTR(d->polygon);

    PkRectF boundingRect = path.boundingRect();
    PkRect fillRect = boundingRect.toAlignedRect();

    // take width of the pen into account
    int penWidth = qRound(pen.widthF());
    fillRect.adjust(-penWidth, -penWidth, penWidth, penWidth);

    // Expand the rectangle to allow for anti-aliasing.
    fillRect.adjust(-1, -1, 1, 1);

    if (!requestedRect.isNull()) {
        fillRect &= requestedRect;
    }

    d->fillPainter->fillRect(fillRect, paintColor(), OPACITY_OPAQUE_U8);

    if (d->polygonMaskImage.isNull() || (d->maskPainter == 0)) {
        d->polygonMaskImage = PkImage(d->maskImageWidth, d->maskImageHeight, PkImage::Format_ARGB32_Premultiplied);
        d->maskPainter = new PkPainter(&d->polygonMaskImage);
        d->maskPainter->setRenderHint(PkPainter::Antialiasing, antiAliasPolygonFill());
    }

    // Break the mask up into chunks so we don't have to allocate a potentially very large PkImage.
    const PkColor black(Qt::black);
    PkPen oldPen = d->maskPainter->pen();
    d->maskPainter->setPen(pen);

    for (qint32 x = fillRect.x(); x < fillRect.x() + fillRect.width(); x += d->maskImageWidth) {
        for (qint32 y = fillRect.y(); y < fillRect.y() + fillRect.height(); y += d->maskImageHeight) {

            d->polygonMaskImage.fill(black.rgb());
            d->maskPainter->translate(-x, -y);
            d->maskPainter->drawPath(path);
            d->maskPainter->translate(x, y);

            qint32 rectWidth = qMin(fillRect.x() + fillRect.width() - x, d->maskImageWidth);
            qint32 rectHeight = qMin(fillRect.y() + fillRect.height() - y, d->maskImageHeight);

            KisHLineIteratorSP lineIt = d->polygon->createHLineIteratorNG(x, y, rectWidth);

            quint8 tmp;
            for (int row = y; row < y + rectHeight; row++) {
                PkRgb* line = reinterpret_cast<PkRgb*>(d->polygonMaskImage.scanLine(row - y));
                do {
                    tmp = qRed(line[lineIt->x() - x]);
                    d->polygon->colorSpace()->applyAlphaU8Mask(lineIt->rawData(), &tmp, 1);
                } while (lineIt->nextPixel());
                lineIt->nextRow();
            }

        }
    }

    d->maskPainter->setPen(oldPen);
    PkRect r = d->polygon->extent();

    bitBlt(r.x(), r.y(), d->polygon, r.x(), r.y(), r.width(), r.height());
}

void KisPainter::fillPolygon(const vQPointF& points, FillStyle fillStyle)
{
    if (points.count() < 3) {
        return;
    }

    if (fillStyle == FillStyleNone) {
        return;
    }

    PkPainterPath polygonPath;

    polygonPath.moveTo(points.at(0));

    for (int pointIndex = 1; pointIndex < points.count(); pointIndex++) {
        polygonPath.lineTo(points.at(pointIndex));
    }

    polygonPath.closeSubpath();

    d->fillStyle = fillStyle;
    fillPainterPath(polygonPath);
}

void KisPainter::paintPolygon(const vQPointF& points)
{
    if (d->fillStyle != FillStyleNone) {
        fillPolygon(points, d->fillStyle);
    }

    if (d->strokeStyle != StrokeStyleNone) {
        if (points.count() > 1) {
            KisDistanceInformation distance(points[0],
                                            KisAlgebra2D::directionBetweenPoints(points[0], points[1], 0.0));

            KisRandomSourceSP rnd = new KisRandomSource();
            KisPerStrokeRandomSourceSP strokeRnd = new KisPerStrokeRandomSource();

            auto point = [rnd, strokeRnd] (const PkPointF &pt) {
                KisPaintInformation pi(pt);
                pi.setRandomSource(rnd);
                pi.setPerStrokeRandomSource(strokeRnd);
                return pi;
            };

            for (int i = 0; i < points.count() - 1; i++) {
                paintLine(point(points[i]), point(points[i + 1]), &distance);
            }
            paintLine(point(points[points.count() - 1]), point(points[0]), &distance);
        }
    }
}

void KisPainter::paintPainterPath(const PkPainterPath& path)
{
    if (d->fillStyle != FillStyleNone) {
        fillPainterPath(path);
    }

    if (d->strokeStyle == StrokeStyleNone) return;

    PkPointF lastPoint, nextPoint;
    int elementCount = path.elementCount();
    KisDistanceInformation saveDist;

    KisRandomSourceSP rnd = new KisRandomSource();
    KisPerStrokeRandomSourceSP strokeRnd = new KisPerStrokeRandomSource();

    auto point = [rnd, strokeRnd] (const PkPointF &pt) {
        KisPaintInformation pi(pt);
        pi.setRandomSource(rnd);
        pi.setPerStrokeRandomSource(strokeRnd);
        return pi;
    };

    for (int i = 0; i < elementCount; i++) {
        PkPainterPath::Element element = path.elementAt(i);
        switch (element.type) {
        case PkPainterPath::MoveToElement:
            lastPoint = PkPointF(element.x, element.y);
            break;
        case PkPainterPath::LineToElement:
            nextPoint = PkPointF(element.x, element.y);
            paintLine(point(lastPoint), point(nextPoint), &saveDist);
            lastPoint = nextPoint;
            break;
        case PkPainterPath::CurveToElement:
            nextPoint = PkPointF(path.elementAt(i + 2).x, path.elementAt(i + 2).y);
            paintBezierCurve(point(lastPoint),
                             PkPointF(path.elementAt(i).x, path.elementAt(i).y),
                             PkPointF(path.elementAt(i + 1).x, path.elementAt(i + 1).y),
                             point(nextPoint), &saveDist);
            lastPoint = nextPoint;
            break;
        default:
            continue;
        }
    }
}

void KisPainter::fillPainterPath(const PkPainterPath& path)
{
    fillPainterPath(path, PkRect());
}

void KisPainter::fillPainterPath(const PkPainterPath& path, const PkRect &requestedRect)
{
    if (d->mirrorHorizontally || d->mirrorVertically) {
        KisLodTransform lod(d->device);
        PkPointF effectiveAxesCenter = lod.map(d->axesCenter);

        PkTransform C1 = PkTransform::fromTranslate(-effectiveAxesCenter.x(), -effectiveAxesCenter.y());
        PkTransform C2 = PkTransform::fromTranslate(effectiveAxesCenter.x(), effectiveAxesCenter.y());

        PkTransform t;
        PkPainterPath newPath;
        PkRect newRect;

        if (d->mirrorHorizontally) {
            t = C1 * PkTransform::fromScale(-1,1) * C2;
            newPath = t.map(path);
            newRect = t.mapRect(requestedRect);
            d->fillPainterPathImpl(newPath, newRect);
        }

        if (d->mirrorVertically) {
            t = C1 * PkTransform::fromScale(1,-1) * C2;
            newPath = t.map(path);
            newRect = t.mapRect(requestedRect);
            d->fillPainterPathImpl(newPath, newRect);
        }

        if (d->mirrorHorizontally && d->mirrorVertically) {
            t = C1 * PkTransform::fromScale(-1,-1) * C2;
            newPath = t.map(path);
            newRect = t.mapRect(requestedRect);
            d->fillPainterPathImpl(newPath, newRect);
        }
    }

    d->fillPainterPathImpl(path, requestedRect);
}


void KisPainter::paintPolyline(const vQPointF &points,
                               int index, int numPoints)
{
    if (d->fillStyle != FillStyleNone) {
        fillPolygon(points, d->fillStyle);
    }

    if (d->strokeStyle == StrokeStyleNone) return;

    if (index >= points.count())
        return;

    if (numPoints < 0)
        numPoints = points.count();

    if (index + numPoints > points.count())
        numPoints = points.count() - index;

    if (numPoints > 1) {
        KisRandomSourceSP rnd = new KisRandomSource();
        KisPerStrokeRandomSourceSP strokeRnd = new KisPerStrokeRandomSource();

        auto point = [rnd, strokeRnd] (const PkPointF &pt) {
            KisPaintInformation pi(pt);
            pi.setRandomSource(rnd);
            pi.setPerStrokeRandomSource(strokeRnd);
            return pi;
        };

        KisDistanceInformation saveDist(points[0],
                KisAlgebra2D::directionBetweenPoints(points[0], points[1], 0.0));
        for (int i = index; i < index + numPoints - 1; i++) {
            paintLine(point(points[i]), point(points[i + 1]), &saveDist);
        }
    }

}


void KisPainter::paintRect(const PkRectF &rect)
{
    PkRectF normalizedRect = rect.normalized();

    vQPointF points;

    points.push_back(normalizedRect.topLeft());
    points.push_back(normalizedRect.bottomLeft());
    points.push_back(normalizedRect.bottomRight());
    points.push_back(normalizedRect.topRight());

    paintPolygon(points);
}

void KisPainter::paintRect(const qreal x,
                           const qreal y,
                           const qreal w,
                           const qreal h)
{
    paintRect(PkRectF(x, y, w, h));
}


void KisPainter::paintEllipse(const PkRectF &rect)
{
    PkRectF r = rect.normalized(); // normalize before checking as negative width and height are empty too
    if (r.isEmpty()) return;

    // See http://www.whizkidtech.redprince.net/bezier/circle/ for explanation.
    // kappa = (4/3*(sqrt(2)-1))
    const qreal kappa = 0.5522847498;
    const qreal lx = (r.width() / 2) * kappa;
    const qreal ly = (r.height() / 2) * kappa;

    PkPointF center = r.center();

    PkPointF p0(r.left(), center.y());
    PkPointF p1(r.left(), center.y() - ly);
    PkPointF p2(center.x() - lx, r.top());
    PkPointF p3(center.x(), r.top());

    vQPointF points;

    getBezierCurvePoints(p0, p1, p2, p3, points);

    PkPointF p4(center.x() + lx, r.top());
    PkPointF p5(r.right(), center.y() - ly);
    PkPointF p6(r.right(), center.y());

    getBezierCurvePoints(p3, p4, p5, p6, points);

    PkPointF p7(r.right(), center.y() + ly);
    PkPointF p8(center.x() + lx, r.bottom());
    PkPointF p9(center.x(), r.bottom());

    getBezierCurvePoints(p6, p7, p8, p9, points);

    PkPointF p10(center.x() - lx, r.bottom());
    PkPointF p11(r.left(), center.y() + ly);

    getBezierCurvePoints(p9, p10, p11, p0, points);

    paintPolygon(points);
}

void KisPainter::paintEllipse(const qreal x,
                              const qreal y,
                              const qreal w,
                              const qreal h)
{
    paintEllipse(PkRectF(x, y, w, h));
}
