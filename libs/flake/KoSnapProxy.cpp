/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008-2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoSnapProxy.h"
#include "KoSnapGuide.h"
#include "KoCanvasBase.h"
#include "KoShapeManager.h"
#include "KoPathShape.h"
#include "KoPathPoint.h"
#include <KoSnapData.h>
#include <KoShapeLayer.h>

KoSnapProxy::KoSnapProxy(KoSnapGuide * snapGuide)
        : m_snapGuide(snapGuide)
{
}

PkList<PkPointF> KoSnapProxy::pointsInRect(const PkRectF &rect, bool omitEditedShape)
{
    PkList<PkPointF> points;
    PkList<KoShape*> shapes = shapesInRect(rect, omitEditedShape);
    Q_FOREACH (KoShape * shape, shapes) {
        Q_FOREACH (const PkPointF & point, pointsFromShape(shape)) {
            if (rect.contains(point))
                points.append(point);
        }
    }

    return points;
}

PkList<KoShape*> KoSnapProxy::shapesInRect(const PkRectF &rect, bool omitEditedShape)
{
    PkList<KoShape*> shapes = m_snapGuide->canvas()->shapeManager()->shapesAt(rect);
    Q_FOREACH (KoShape * shape, m_snapGuide->ignoredShapes()) {
        const int index = shapes.indexOf(shape);
        if (index >= 0) {
            shapes.removeAt(index);
        }
    }


    if (omitEditedShape) {
        Q_FOREACH (KoPathPoint *point, m_snapGuide->ignoredPathPoints()) {
            const int index = shapes.indexOf(point->parent());
            if (index >= 0) {
                shapes.removeAt(index);
            }
        }
    }

    if (!omitEditedShape && m_snapGuide->additionalEditedShape()) {
        PkRectF bound = m_snapGuide->additionalEditedShape()->boundingRect();
        if (rect.intersects(bound) || rect.contains(bound))
            shapes.append(m_snapGuide->additionalEditedShape());
    }
    return shapes;
}

PkList<PkPointF> KoSnapProxy::pointsFromShape(KoShape * shape)
{
    PkList<PkPointF> snapPoints;
    // no snapping to hidden shapes
    if (! shape->isVisible())
        return snapPoints;

    // return the special snap points of the shape
    snapPoints += shape->snapData().snapPoints();

    KoPathShape * path = dynamic_cast<KoPathShape*>(shape);
    if (path) {
        PkTransform m = path->absoluteTransformation();

        PkList<KoPathPoint*> ignoredPoints = m_snapGuide->ignoredPathPoints();

        int subpathCount = path->subpathCount();
        for (int subpathIndex = 0; subpathIndex < subpathCount; ++subpathIndex) {
            int pointCount = path->subpathPointCount(subpathIndex);
            for (int pointIndex = 0; pointIndex < pointCount; ++pointIndex) {
                KoPathPoint * p = path->pointByIndex(KoPathPointIndex(subpathIndex, pointIndex));
                if (! p || ignoredPoints.contains(p))
                    continue;

                snapPoints.append(m.map(p->point()));
            }
        }
    }
    else
    {
        // add the bounding box corners as default snap points
        PkRectF bbox = shape->boundingRect();
        snapPoints.append(bbox.topLeft());
        snapPoints.append(bbox.topRight());
        snapPoints.append(bbox.bottomRight());
        snapPoints.append(bbox.bottomLeft());
    }

    return snapPoints;
}

PkList<KoPathSegment> KoSnapProxy::segmentsInRect(const PkRectF &rect, bool omitEditedShape)
{
    
    PkList<KoShape*> shapes = shapesInRect(rect, omitEditedShape);
    PkList<KoPathPoint*> ignoredPoints = m_snapGuide->ignoredPathPoints();

    PkList<KoPathSegment> segments;
    Q_FOREACH (KoShape * shape, shapes) {
        PkList<KoPathSegment> shapeSegments;
        PkRectF rectOnShape = shape->documentToShape(rect);
        KoPathShape * path = dynamic_cast<KoPathShape*>(shape);
        if (path) {
            shapeSegments = path->segmentsAt(rectOnShape);
        } else {
            Q_FOREACH (const KoPathSegment & s, shape->snapData().snapSegments()) {
                PkRectF controlRect = s.controlPointRect();
                if (! rect.intersects(controlRect) && ! controlRect.contains(rect))
                    continue;
                PkRectF bound = s.boundingRect();
                if (! rect.intersects(bound) && ! bound.contains(rect))
                    continue;
                shapeSegments.append(s);
            }
        }

        PkTransform m = shape->absoluteTransformation();
        // transform segments to document coordinates
        Q_FOREACH (const KoPathSegment & s, shapeSegments) {
            if (ignoredPoints.contains(s.first()) || ignoredPoints.contains(s.second()))
                continue;
            segments.append(s.mapped(m));
        }
    }
    return segments;
}

PkList<KoShape*> KoSnapProxy::shapes(bool omitEditedShape)
{
    PkList<KoShape*> allShapes = m_snapGuide->canvas()->shapeManager()->shapes();
    PkList<KoShape*> filteredShapes;
    PkList<KoShape*> ignoredShapes = m_snapGuide->ignoredShapes();

    // filter all hidden and ignored shapes
    Q_FOREACH (KoShape * shape, allShapes) {
        if (shape->isVisible() &&
            !ignoredShapes.contains(shape) &&
            !dynamic_cast<KoShapeLayer*>(shape)) {

            filteredShapes.append(shape);
        }
    }

    if (omitEditedShape) {
        Q_FOREACH (KoPathPoint *point, m_snapGuide->ignoredPathPoints()) {
            const int index = filteredShapes.indexOf(point->parent());
            if (index >= 0) {
                filteredShapes.removeAt(index);
            }
        }
    }

    if (!omitEditedShape && m_snapGuide->additionalEditedShape()) {
        filteredShapes.append(m_snapGuide->additionalEditedShape());
    }

    return filteredShapes;
}

KoCanvasBase * KoSnapProxy::canvas()
{
    return m_snapGuide->canvas();
}

