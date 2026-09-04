/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Jos van den Oever <jos@vandenoever.info>
 * SPDX-FileCopyrightText: 2009 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2010 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoFlake.h"
#include "KoShape.h"

#include <PkGradient.h>
#include <math.h>
#include "kis_global.h"

PkGradient *KoFlake::cloneGradient(const PkGradient *gradient)
{
    if (! gradient)
        return 0;

    // PkGradient 是值类型（S-03-a，非多态）：整支拷贝即可。
    return new PkGradient(*gradient);
}

PkGradient *KoFlake::mergeGradient(const PkGradient *coordsSource, const PkGradient *fillSource)
{
    PkPointF start;
    PkPointF end;
    PkPointF focalPoint;

    switch (coordsSource->type()) {
    case PkGradient::LinearGradient: {
        start = coordsSource->start();
        focalPoint = start;
        end = coordsSource->finalStop();
        break;
    }
    case PkGradient::RadialGradient: {
        start = coordsSource->center();
        end = start + PkPointF(coordsSource->radius(), 0);
        focalPoint = coordsSource->focalPoint();
        break;
    }
    case PkGradient::ConicalGradient: {
        start = coordsSource->center();
        focalPoint = start;

        PkLineF l (start, start + PkPointF(1.0, 0));
        l.setAngle(coordsSource->angle());
        end = l.p2();
        break;
    }
    default:
        return 0;
    }

    PkGradient *clone = 0;

    switch (fillSource->type()) {
    case PkGradient::LinearGradient:
        clone = new PkGradient(PkGradient::linear(start, end));
        break;
    case PkGradient::RadialGradient:
        clone = new PkGradient(PkGradient::radial(start, kisDistance(start, end), focalPoint));
        break;
    case PkGradient::ConicalGradient: {
        PkLineF l(start, end);
        clone = new PkGradient(PkGradient::conical(l.p1(), l.angle()));
        break;
    }
    default:
        return 0;
    }

    clone->setCoordinateMode(fillSource->coordinateMode());
    clone->setSpread(fillSource->spread());
    clone->setStops(fillSource->stops());

    return clone;
}

PkPointF KoFlake::toRelative(const PkPointF &absolute, const PkSizeF &size)
{
    return PkPointF(size.width() == 0 ? 0: absolute.x() / size.width(),
                   size.height() == 0 ? 0: absolute.y() / size.height());
}

PkPointF KoFlake::toAbsolute(const PkPointF &relative, const PkSizeF &size)
{
    return PkPointF(relative.x() * size.width(), relative.y() * size.height());
}

#include <PkTransform.h>
#include "kis_debug.h"
#include "kis_algebra_2d.h"

namespace {

qreal getScaleByPointsPair(qreal x1, qreal x2, qreal expX1, qreal expX2)
{
    static const qreal eps = 1e-10;

    const qreal diff = x2 - x1;
    const qreal expDiff = expX2 - expX1;

    return qAbs(diff) > eps ? expDiff / diff : 1.0;
}

void findMinMaxPoints(const PkPolygonF &poly, int *minPoint, int *maxPoint, std::function<qreal(const PkPointF&)> dimension)
{
    KIS_ASSERT_RECOVER_RETURN(minPoint);
    KIS_ASSERT_RECOVER_RETURN(maxPoint);

    qreal minValue = dimension(poly[*minPoint]);
    qreal maxValue = dimension(poly[*maxPoint]);

    for (int i = 0; i < poly.size(); i++) {
        const qreal value = dimension(poly[i]);

        if (value < minValue) {
            *minPoint = i;
            minValue = value;
        }

        if (value > maxValue) {
            *maxPoint = i;
            maxValue = value;
        }
    }
}

}


Qt::Orientation KoFlake::significantScaleOrientation(qreal scaleX, qreal scaleY)
{
    const qreal scaleXDeviation = qAbs(1.0 - scaleX);
    const qreal scaleYDeviation = qAbs(1.0 - scaleY);

    return scaleXDeviation > scaleYDeviation ? Qt::Horizontal : Qt::Vertical;
}

void KoFlake::scaleShape(KoShape *shape, qreal scaleX, qreal scaleY,
                          const PkPointF &absoluteStillPoint,
                          const PkTransform &postScalingCoveringTransform)
{
    const PkTransform scale = PkTransform::fromScale(scaleX, scaleY);
    PkPointF localStillPoint = postScalingCoveringTransform.inverted().map(absoluteStillPoint);
    const PkTransform localStillPointOffset = PkTransform::fromTranslate(-localStillPoint.x(), -localStillPoint.y());

    shape->setTransformation( shape->transformation() *
                postScalingCoveringTransform.inverted() *
                localStillPointOffset *
                scale *
                localStillPointOffset.inverted() *
                postScalingCoveringTransform);
}

void KoFlake::scaleShapeGlobal(KoShape *shape, qreal scaleX, qreal scaleY,
                               const PkPointF &absoluteStillPoint)
{
    const PkTransform scale = PkTransform::fromScale(scaleX, scaleY);
    const PkTransform absoluteStillPointOffset = PkTransform::fromTranslate(-absoluteStillPoint.x(), -absoluteStillPoint.y());

    const PkTransform uniformGlobalTransform =
            shape->absoluteTransformation() *
            absoluteStillPointOffset *
            scale *
            absoluteStillPointOffset.inverted() *
            shape->absoluteTransformation().inverted() *
            shape->transformation();

    shape->setTransformation(uniformGlobalTransform);
}

void KoFlake::resizeShape(KoShape *shape, qreal scaleX, qreal scaleY,
                          const PkPointF &absoluteStillPoint,
                          bool useGlobalMode)
{
    using namespace KisAlgebra2D;

    if (useGlobalMode) {
        const PkTransform scale = PkTransform::fromScale(scaleX, scaleY);
        const PkTransform uniformGlobalTransform =
                shape->absoluteTransformation() *
                scale *
                shape->absoluteTransformation().inverted();

        const PkRectF rect = shape->outlineRect();

        /**
         * The basic idea of such global scaling:
         *
         * 1) We choose two the most distant points of the original outline rect
         * 2) Calculate their expected position if transformed using `uniformGlobalTransform`
         * 3) NOTE1: we do not transform the entire shape using `uniformGlobalTransform`,
         *           because it will cause massive shearing. We transform only two points
         *           and adjust other points using dumb scaling.
         * 4) NOTE2: given that `scale` transform is much more simpler than
         *           `uniformGlobalTransform`, we cannot guarantee equivalent changes on
         *           both globalScaleX and globalScaleY at the same time. We can guarantee
         *           only one of them. Therefore we select the most "important" axis and
         *           guarantee scale along it. The scale along the other direction is not
         *           controlled.
         * 5) After we have the two most distant points, we can just calculate the scale
         *    by dividing difference between their expected and original positions. This
         *    formula can be derived from equation:
         *
         *    localPoint_i * ScaleMatrix = localPoint_i * UniformGlobalTransform = expectedPoint_i
         */

        // choose the most significant scale direction
        Qt::Orientation significantOrientation = significantScaleOrientation(scaleX, scaleY);

        std::function<qreal(const PkPointF&)> dimension;

        if (significantOrientation == Qt::Horizontal) {
            dimension = [] (const PkPointF &pt) {
                return pt.x();
            };

        } else {
            dimension = [] (const PkPointF &pt) {
                return pt.y();
            };
        }

        // find min and max points (in absolute coordinates),
        // by default use top-left and bottom-right
        PkPolygonF localPoints(rect);
        PkPolygonF globalPoints = shape->absoluteTransformation().map(localPoints);

        int minPointIndex = 0;
        int maxPointIndex = 2;

        findMinMaxPoints(globalPoints, &minPointIndex, &maxPointIndex, dimension);

        // calculate the scale using the extremum points
        const PkPointF minPoint = localPoints[minPointIndex];
        const PkPointF maxPoint = localPoints[maxPointIndex];

        const PkPointF minPointExpected = uniformGlobalTransform.map(minPoint);
        const PkPointF maxPointExpected = uniformGlobalTransform.map(maxPoint);

        scaleX = getScaleByPointsPair(minPoint.x(), maxPoint.x(),
                                      minPointExpected.x(), maxPointExpected.x());
        scaleY = getScaleByPointsPair(minPoint.y(), maxPoint.y(),
                                      minPointExpected.y(), maxPointExpected.y());
    }

    const PkSizeF oldSize(shape->size());
    const PkSizeF newSize(oldSize.width() * qAbs(scaleX), oldSize.height() * qAbs(scaleY));

    const PkTransform mirrorTransform = PkTransform::fromScale(signPZ(scaleX), signPZ(scaleY));

    /**
     * NOTE: when resizing a shape we expect top-left corner in parent's
     *       coordinates to keep it's position.
     */

    shape->setSize(newSize);

    PkPointF localStillPoint = shape->absoluteTransformation().inverted().map(absoluteStillPoint);
    const PkTransform localStillPointOffset = PkTransform::fromTranslate(-localStillPoint.x(), -localStillPoint.y());
    const PkSizeF realNewSize = shape->size();

    const PkTransform realResizeTransform =
        PkTransform::fromScale(oldSize.width() > 0 ? realNewSize.width() / oldSize.width() : 1.0,
                              oldSize.height() > 0 ? realNewSize.height() / oldSize.height() : 1.0);

    shape->setTransformation(realResizeTransform.inverted() *
                             localStillPointOffset *
                             realResizeTransform *
                             mirrorTransform *
                             localStillPointOffset.inverted() *
                             shape->transformation()
                             );
}

void KoFlake::resizeShapeCommon(KoShape *shape, qreal scaleX, qreal scaleY,
                          const PkPointF &absoluteStillPoint,
                          bool useGlobalMode,
                          bool usePostScaling, const PkTransform &postScalingCoveringTransform)
{
    if (usePostScaling) {
        if (!useGlobalMode) {
            scaleShape(shape, scaleX, scaleY, absoluteStillPoint, postScalingCoveringTransform);
        } else {
            scaleShapeGlobal(shape, scaleX, scaleY, absoluteStillPoint);
        }
    } else {
        resizeShape(shape, scaleX, scaleY, absoluteStillPoint, useGlobalMode);
    }
}

PkPointF KoFlake::anchorToPoint(AnchorPosition anchor, const PkRectF rect, bool *valid)
{
    static PkVector<PkPointF> anchorTable;

    if (anchorTable.isEmpty()) {
        anchorTable << PkPointF(0.0,0.0);
        anchorTable << PkPointF(0.5,0.0);
        anchorTable << PkPointF(1.0,0.0);

        anchorTable << PkPointF(0.0,0.5);
        anchorTable << PkPointF(0.5,0.5);
        anchorTable << PkPointF(1.0,0.5);

        anchorTable << PkPointF(0.0,1.0);
        anchorTable << PkPointF(0.5,1.0);
        anchorTable << PkPointF(1.0,1.0);
    }

    if (valid)
        *valid = false;

    switch(anchor)
    {
        case AnchorPosition::TopLeft:
        case AnchorPosition::Top:
        case AnchorPosition::TopRight:
        case AnchorPosition::Left:
        case AnchorPosition::Center:
        case AnchorPosition::Right:
        case AnchorPosition::BottomLeft:
        case AnchorPosition::Bottom:
        case AnchorPosition::BottomRight:
            if (valid)
                *valid = true;
            return KisAlgebra2D::relativeToAbsolute(anchorTable[int(anchor)], rect);
        default:
            KIS_SAFE_ASSERT_RECOVER_NOOP(anchor >= AnchorPosition::TopLeft && anchor < AnchorPosition::NumAnchorPositions);
            return rect.topLeft();
    }
}
