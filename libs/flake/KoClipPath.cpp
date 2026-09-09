/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkFlakeBridge.h>
#include "KoClipPath.h"
#include "KoPathShape.h"
#include "KoShapeGroup.h"

#include <PkTransform.h>
#include <PkPainterPath.h>
#include <PkPainter.h>
#include <PkContainerAlgo.h>

#include <algorithm>

#include <kis_algebra_2d.h>


PkTransform scaleToPercent(const PkSizeF &size)
{
    const qreal w = std::max(static_cast<qreal>(1e-5), size.width());
    const qreal h = std::max(static_cast<qreal>(1e-5), size.height());
    return PkTransform().scale(1.0/w, 1.0/h);
}

PkTransform scaleFromPercent(const PkSizeF &size)
{
    const qreal w = std::max(static_cast<qreal>(1e-5), size.width());
    const qreal h = std::max(static_cast<qreal>(1e-5), size.height());
    return PkTransform().scale(w/1.0, h/1.0);
}

class KoClipPath::Private
{
public:
    Private()
        {}

    Private(const Private &rhs)
        : clipPath(rhs.clipPath)
        , clipRule(rhs.clipRule)
        , coordinates(rhs.coordinates)
        , initialTransformToShape(rhs.initialTransformToShape)
        , initialShapeSize(rhs.initialShapeSize)
    {
        PK_FOREACH (KoShape *shape, rhs.shapes) {
            KoShape *clonedShape = shape->cloneShape();
            KIS_ASSERT_RECOVER(clonedShape) { continue; }

            shapes.append(clonedShape);
        }
    }

    ~Private()
    {
        pkDeleteAll(shapes);
        shapes.clear();
    }

    void collectShapePath(PkPainterPath *result, const KoShape *shape) {
        if (const KoPathShape *pathShape = dynamic_cast<const KoPathShape*>(shape)) {
            // different shapes add up to the final path using Windind Fill rule (acc. to SVG 1.1)
            PkTransform t = pathShape->absoluteTransformation();
            result->addPath(t.map(pathShape->outline()));
        } else if (const KoShapeGroup *groupShape = dynamic_cast<const KoShapeGroup*>(shape)) {
            PkList<KoShape*> shapes = groupShape->shapes();
            std::sort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);

            PK_FOREACH (const KoShape *child, shapes) {
                collectShapePath(result, child);
            }
        }
    }


    void compileClipPath()
    {
        PkList<KoShape*> clipShapes = this->shapes;
        if (clipShapes.isEmpty())
            return;

        clipPath = PkPainterPath();
        clipPath.setFillRule(Pk::WindingFill);

        std::sort(clipShapes.begin(), clipShapes.end(), KoShape::compareShapeZIndex);

        PK_FOREACH (KoShape *path, clipShapes) {
            if (!path) continue;

            collectShapePath(&clipPath, path);
        }
    }

    PkList<KoShape*> shapes;
    PkPainterPath clipPath; ///< the compiled clip path in shape coordinates of the clipped shape
    Pk::FillRule clipRule = Pk::WindingFill;
    KoFlake::CoordinateSystem coordinates = KoFlake::ObjectBoundingBox;
    PkTransform initialTransformToShape; ///< initial transformation to shape coordinates of the clipped shape
    PkSizeF initialShapeSize; ///< initial size of clipped shape
};

KoClipPath::KoClipPath(PkList<KoShape*> clipShapes, KoFlake::CoordinateSystem coordinates)
   : d(new Private())
{
    d->shapes = clipShapes;
    d->coordinates = coordinates;
    d->compileClipPath();
}

KoClipPath::~KoClipPath()
{
}

KoClipPath::KoClipPath(const KoClipPath &rhs)
    : d(new Private(*rhs.d))
{
}

KoClipPath &KoClipPath::operator=(const KoClipPath &rhs)
{
    d = rhs.d;
    return *this;
}

KoClipPath *KoClipPath::clone() const
{
    return new KoClipPath(*this);
}

void KoClipPath::setClipRule(Pk::FillRule clipRule)
{
    d->clipRule = clipRule;
}

Pk::FillRule KoClipPath::clipRule() const
{
    return d->clipRule;
}

KoFlake::CoordinateSystem KoClipPath::coordinates() const
{
    return d->coordinates;
}

void KoClipPath::applyClipping(KoShape *shape, PkPainter &painter)
{
    if (shape->clipPath()) {
        PkPainterPath path = shape->clipPath()->path();

        if (shape->clipPath()->coordinates() == KoFlake::ObjectBoundingBox) {
            const PkRectF shapeLocalBoundingRect = shape->outline().boundingRect();
            path = KisAlgebra2D::mapToRect(shapeLocalBoundingRect).map(path);
        }

        if (!path.isEmpty()) {
            painter.setClipPath(path, Pk::IntersectClip);
        }
    }
}

PkPainterPath KoClipPath::path() const
{
    return d->clipPath;
}

PkPainterPath KoClipPath::pathForSize(const PkSizeF &size) const
{
    return scaleFromPercent(size).map(d->clipPath);
}

PkList<KoPathShape*> KoClipPath::clipPathShapes() const
{
    // TODO: deprecate this method!

    PkList<KoPathShape*> shapes;

    PK_FOREACH (KoShape *shape, d->shapes) {
        KoPathShape *pathShape = dynamic_cast<KoPathShape*>(shape);
        if (pathShape) {
            shapes << pathShape;
        }
    }

    return shapes;
}

PkList<KoShape *> KoClipPath::clipShapes() const
{
    return d->shapes;
}

PkTransform KoClipPath::clipDataTransformation(KoShape *clippedShape) const
{
    if (!clippedShape)
        return d->initialTransformToShape;

    // the current transformation of the clipped shape
    PkTransform currentShapeTransform = clippedShape->absoluteTransformation();

    // calculate the transformation which represents any resizing of the clipped shape
    const PkSizeF currentShapeSize = clippedShape->outline().boundingRect().size();
    const qreal sx = currentShapeSize.width() / d->initialShapeSize.width();
    const qreal sy = currentShapeSize.height() / d->initialShapeSize.height();
    PkTransform scaleTransform = PkTransform().scale(sx, sy);

    // 1. transform to initial clipped shape coordinates
    // 2. apply resizing transformation
    // 3. convert to current clipped shape document coordinates
    return d->initialTransformToShape * scaleTransform * currentShapeTransform;
}
