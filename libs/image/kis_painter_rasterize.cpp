/*
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2008-2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// This path family preserves the official Krita v6.0.3 fill-source ->
// chunked coverage mask -> applyAlphaU8Mask -> bitBlt pipeline. Coverage is
// produced by the private Qt-5.15-derived, oracle-checked rasterizer in
// private/kis_path_rasterizer.cpp; no painter/image shell is involved.

#include "kis_painter.h"
#include "kis_painter_p.h"
#include "kis_algebra_2d.h"   // KisAlgebra2D::directionBetweenPoints（paintPolygon/paintPolyline 用）
#include <brushengine/kis_paint_information.h>
#include "kis_lod_transform.h"

#include "kis_iterator_ng.h"

#include <PkRect.h>
#include <PkPainterPath.h>
#include <PkPen.h>

#include "private/kis_path_rasterizer_p.h"

namespace {

void applyCoverageMask(KisPaintDeviceSP &device,
                       const KisPathRasterizer::CoverageMask &mask,
                       const PkRect &chunk)
{
    if (mask.isEmpty()) {
        device->clear(chunk);
        return;
    }

    for (int row = mask.bounds.y(); row <= mask.bounds.bottom(); ++row) {
        const uint8_t *coverage = mask.scanLine(row);
        KisHLineIteratorSP it = device->createHLineIteratorNG(
            mask.bounds.x(), row, mask.bounds.width());
        do {
            const uint8_t alpha = coverage[it->x() - mask.bounds.x()];
            device->colorSpace()->applyAlphaU8Mask(it->rawData(), &alpha, 1);
        } while (it->nextPixel());
    }
}

} // namespace

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

    KIS_SAFE_ASSERT_RECOVER_RETURN(polygon);

    PkRectF boundingRect = path.boundingRect();
    PkRect fillRect = boundingRect.toAlignedRect();

    // Expand the rectangle to allow for anti-aliasing.
    fillRect.adjust(-1, -1, 1, 1);

    if (requestedRect.isValid()) {
        fillRect &= requestedRect;
    }

    switch (fillStyle) {
    default:
        [[fallthrough]];
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

    // Break the mask up into chunks so we don't allocate one potentially huge mask.
    for (qint32 x = fillRect.x(); x < fillRect.x() + fillRect.width(); x += maskImageWidth) {
        for (qint32 y = fillRect.y(); y < fillRect.y() + fillRect.height(); y += maskImageHeight) {
            const PkRect chunk(x, y,
                               qMin(fillRect.x() + fillRect.width() - x, maskImageWidth),
                               qMin(fillRect.y() + fillRect.height() - y, maskImageHeight));
            const KisPathRasterizer::CoverageMask mask =
                KisPathRasterizer::rasterizeFill(path, chunk, q->antiAliasPolygonFill());
            applyCoverageMask(polygon, mask, chunk);
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

    KIS_SAFE_ASSERT_RECOVER_RETURN(d->polygon);

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

    // Break the mask up into chunks so we don't allocate one potentially huge mask.
    for (qint32 x = fillRect.x(); x < fillRect.x() + fillRect.width(); x += d->maskImageWidth) {
        for (qint32 y = fillRect.y(); y < fillRect.y() + fillRect.height(); y += d->maskImageHeight) {
            const PkRect chunk(x, y,
                               qMin(fillRect.x() + fillRect.width() - x, d->maskImageWidth),
                               qMin(fillRect.y() + fillRect.height() - y, d->maskImageHeight));
            const KisPathRasterizer::CoverageMask mask =
                KisPathRasterizer::rasterizeStroke(path, pen, chunk, antiAliasPolygonFill());
            applyCoverageMask(d->polygon, mask, chunk);
        }
    }

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
