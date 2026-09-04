/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2011 Thorsten Zachmann <zachmann@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QtCore/QtCore>
#include <PkGradient.h>
#include <KoGradientBridge.h>
#include <PkFlakeBridge.h>
#include "KoMarker.h"

#include <KoXmlNS.h>
#include "KoPathShape.h"
#include "KoPathShapeLoader.h"
#include "KoShapeLoadingContext.h"
#include "KoShapeSavingContext.h"
#include "KoShapePainter.h"
#include <KoShapeStroke.h>
#include <KoGradientBackground.h>
#include <KoColorBackground.h>


#include <PkString.h>
#include <QUrl>
#include <PkPainterPath.h>
#include <QPainter>

#include "kis_global.h"
#include "kis_algebra_2d.h"

class Q_DECL_HIDDEN KoMarker::Private
{
public:
    Private()
        : coordinateSystem(StrokeWidth),
          referenceSize(3,3),
          hasAutoOrientation(false),
          explicitOrientation(0)
    {}

    ~Private() {
        // shape manager that is stored in the painter should be destroyed
        // before the shapes themselves
        shapePainter.reset();
        qDeleteAll(shapes);
    }

    bool operator==(const KoMarker::Private &other) const
    {
        // WARNING: comparison of shapes is extremely fuzzy! Don't
        //          trust it in life-critical cases!

        return name == other.name &&
            coordinateSystem == other.coordinateSystem &&
            referencePoint == other.referencePoint &&
            referenceSize == other.referenceSize &&
            hasAutoOrientation == other.hasAutoOrientation &&
            explicitOrientation == other.explicitOrientation &&
            compareShapesTo(other.shapes);
    }

    Private(const Private &rhs)
        : name(rhs.name),
          coordinateSystem(rhs.coordinateSystem),
          referencePoint(rhs.referencePoint),
          referenceSize(rhs.referenceSize),
          hasAutoOrientation(rhs.hasAutoOrientation),
          explicitOrientation(rhs.explicitOrientation)
    {
        Q_FOREACH (KoShape *shape, rhs.shapes) {
            shapes << shape->cloneShape();
        }
    }

    PkString name;
    MarkerCoordinateSystem coordinateSystem;
    PkPointF referencePoint;
    PkSizeF referenceSize;

    bool hasAutoOrientation;
    qreal explicitOrientation;

    PkList<KoShape*> shapes;
    PkScopedPointer<KoShapePainter> shapePainter;

    bool compareShapesTo(const PkList<KoShape*> other) const {
        if (shapes.size() != other.size()) return false;

        for (int i = 0; i < shapes.size(); i++) {
            if (shapes[i]->outline() != other[i]->outline() ||
                shapes[i]->absoluteTransformation() != other[i]->absoluteTransformation()) {

                return false;
            }
        }

        return true;
    }

    PkTransform markerTransform(qreal strokeWidth, qreal nodeAngle, const PkPointF &pos = PkPointF()) {
        const PkTransform translate = PkTransform::fromTranslate(-referencePoint.x(), -referencePoint.y());

        PkTransform t = translate;

        if (coordinateSystem == StrokeWidth) {
            t *= PkTransform::fromScale(strokeWidth, strokeWidth);
        }

        const qreal angle = hasAutoOrientation ? nodeAngle : explicitOrientation;
        if (angle != 0.0) {
            PkTransform r;
            r.rotateRadians(angle);
            t *= r;
        }

        t *= PkTransform::fromTranslate(pos.x(), pos.y());

        return t;
    }
};

KoMarker::KoMarker()
: d(new Private())
{
}

KoMarker::~KoMarker()
{
    delete d;
}

PkString KoMarker::name() const
{
    return d->name;
}

KoMarker::KoMarker(const KoMarker &rhs)
    : QSharedData(rhs),
      d(new Private(*rhs.d))
{
}

bool KoMarker::operator==(const KoMarker &other) const
{
    return *d == *other.d;
}

void KoMarker::setCoordinateSystem(KoMarker::MarkerCoordinateSystem value)
{
    d->coordinateSystem = value;
}

KoMarker::MarkerCoordinateSystem KoMarker::coordinateSystem() const
{
    return d->coordinateSystem;
}

KoMarker::MarkerCoordinateSystem KoMarker::coordinateSystemFromString(const PkString &value)
{
    MarkerCoordinateSystem result = StrokeWidth;

    if (value == "userSpaceOnUse") {
        result = UserSpaceOnUse;
    }

    return result;
}

PkString KoMarker::coordinateSystemToString(KoMarker::MarkerCoordinateSystem value)
{
    return
        value == StrokeWidth ?
        "strokeWidth" :
                "userSpaceOnUse";
}

void KoMarker::setReferencePoint(const PkPointF &value)
{
    d->referencePoint = value;
}

PkPointF KoMarker::referencePoint() const
{
    return d->referencePoint;
}

void KoMarker::setReferenceSize(const PkSizeF &size)
{
    d->referenceSize = size;
}

PkSizeF KoMarker::referenceSize() const
{
    return d->referenceSize;
}

bool KoMarker::hasAutoOrientation() const
{
    return d->hasAutoOrientation;
}

void KoMarker::setAutoOrientation(bool value)
{
    d->hasAutoOrientation = value;
}

qreal KoMarker::explicitOrientation() const
{
    return d->explicitOrientation;
}

void KoMarker::setExplicitOrientation(qreal value)
{
    d->explicitOrientation = value;
}

void KoMarker::setShapes(const PkList<KoShape *> &shapes)
{
    d->shapes = shapes;

    if (d->shapePainter) {
        d->shapePainter->setShapes(shapes);
    }
}

PkList<KoShape *> KoMarker::shapes() const
{
    return d->shapes;
}

void KoMarker::paintAtPosition(QPainter *painter, const PkPointF &pos, qreal strokeWidth, qreal nodeAngle)
{
    QTransform oldTransform = painter->transform();

    if (!d->shapePainter) {
        d->shapePainter.reset(new KoShapePainter());
        d->shapePainter->setShapes(d->shapes);
    }

    painter->setTransform(toQTransform(d->markerTransform(strokeWidth, nodeAngle, pos)), true);
    d->shapePainter->paint(*painter);

    painter->setTransform(oldTransform);
}

qreal KoMarker::maxInset(qreal strokeWidth) const
{
    PkRectF shapesBounds = boundingRect(strokeWidth, 0.0); // normalized to 0,0
    qreal result = 0.0;

    result = qMax(KisAlgebra2D::norm(shapesBounds.topLeft()), result);
    result = qMax(KisAlgebra2D::norm(shapesBounds.topRight()), result);
    result = qMax(KisAlgebra2D::norm(shapesBounds.bottomLeft()), result);
    result = qMax(KisAlgebra2D::norm(shapesBounds.bottomRight()), result);

    return result;
}

PkRectF KoMarker::boundingRect(qreal strokeWidth, qreal nodeAngle) const
{
    PkRectF shapesBounds = KoShape::boundingRect(d->shapes);

    const PkTransform t = d->markerTransform(strokeWidth, nodeAngle);

    if (!t.isIdentity()) {
        shapesBounds = t.mapRect(shapesBounds);
    }

    return shapesBounds;
}

PkPainterPath KoMarker::outline(qreal strokeWidth, qreal nodeAngle) const
{
    PkPainterPath outline;
    Q_FOREACH (KoShape *shape, d->shapes) {
        outline |= shape->absoluteTransformation().map(shape->outline());
    }

    const PkTransform t = d->markerTransform(strokeWidth, nodeAngle);

    if (!t.isIdentity()) {
        outline = t.map(outline);
    }

    return outline;
}

void KoMarker::drawPreview(QPainter *painter, const PkRectF &previewRect, const PkPen &pen, KoFlake::MarkerPosition position)
{
    const PkRectF outlineRect = outline(pen.widthF(), 0).boundingRect(); // normalized to 0,0
    PkPointF marker;
    PkPointF start;
    PkPointF end;

    if (position == KoFlake::StartMarker) {
        marker = PkPointF(-outlineRect.left() + previewRect.left(), previewRect.center().y());
        start = marker;
        end = PkPointF(previewRect.right(), start.y());
    } else if (position == KoFlake::MidMarker) {
        start = PkPointF(previewRect.left(), previewRect.center().y());
        marker = PkPointF(-outlineRect.center().x() + previewRect.center().x(), start.y());
        end = PkPointF(previewRect.right(), start.y());
    } else if (position == KoFlake::EndMarker) {
        start = PkPointF(previewRect.left(), previewRect.center().y());
        marker = PkPointF(-outlineRect.right() + previewRect.right(), start.y());
        end = marker;
    }

    painter->save();
    painter->setPen(toQPen(pen));
    painter->setClipRect(toQRectF(previewRect));

    painter->drawLine(toQPointF(start), toQPointF(end));
    paintAtPosition(painter, marker, pen.widthF(), 0);

    painter->restore();
}

void KoMarker::applyShapeStroke(const KoShape *parentShape, KoShapeStroke *stroke, const PkPointF &pos, qreal strokeWidth, qreal nodeAngle)
{
    const PkGradient *originalGradient = toPkGradientPtr(stroke->lineBrush().gradient());

    if (!originalGradient) {
        PkList<KoShape*> linearizedShapes = KoShape::linearizeSubtree(d->shapes);
        Q_FOREACH(KoShape *shape, linearizedShapes) {
            // update the stroke
            KoShapeStrokeSP shapeStroke = shape->stroke() ?
                        pkSharedPointerDynamicCast<KoShapeStroke>(shape->stroke()) :
                        KoShapeStrokeSP();

            if (shapeStroke) {
                shapeStroke = PkSharedPointer<KoShapeStroke>(new KoShapeStroke(*shapeStroke));

                shapeStroke->setLineBrush(QBrush());
                shapeStroke->setColor(stroke->color());

                shape->setStroke(shapeStroke);
            }

            // update the background
            if (shape->background()) {
                PkSharedPointer<KoColorBackground> bg(new KoColorBackground(stroke->color()));
                shape->setBackground(bg);
            }
        }
    } else {
        PkScopedPointer<PkGradient> g(KoFlake::cloneGradient(originalGradient));
        KIS_ASSERT_RECOVER_RETURN(g);

        const PkTransform markerTransformInverted =
                d->markerTransform(strokeWidth, nodeAngle, pos).inverted();

        PkTransform gradientToUser;

        // Unwrap the gradient to work in global mode
        if (g->coordinateMode() == PkGradientEnums::ObjectBoundingMode) {
            PkRectF boundingRect =
                parentShape ?
                parentShape->outline().boundingRect() :
                this->boundingRect(strokeWidth, nodeAngle);

            boundingRect = KisAlgebra2D::ensureRectNotSmaller(boundingRect, PkSizeF(1.0, 1.0));

            gradientToUser = PkTransform(boundingRect.width(), 0, 0, boundingRect.height(),
                                        boundingRect.x(), boundingRect.y());

            g->setCoordinateMode(PkGradientEnums::LogicalMode);
        }

        PkList<KoShape*> linearizedShapes = KoShape::linearizeSubtree(d->shapes);
        Q_FOREACH(KoShape *shape, linearizedShapes) {
            // shape-unwinding transform
            PkTransform t = gradientToUser * markerTransformInverted * shape->absoluteTransformation().inverted();

            // update the stroke
            KoShapeStrokeSP shapeStroke = shape->stroke() ?
                        pkSharedPointerDynamicCast<KoShapeStroke>(shape->stroke()) :
                        KoShapeStrokeSP();

            if (shapeStroke) {
                shapeStroke = PkSharedPointer<KoShapeStroke>(new KoShapeStroke(*shapeStroke));

                QBrush brush(toQGradient(*g));
                brush.setTransform(toQTransform(t));
                shapeStroke->setLineBrush(brush);
                shapeStroke->setColor(PkColor(Pk::transparent));
                shape->setStroke(shapeStroke);
            }

            // update the background
            if (shape->background()) {

                PkSharedPointer<KoGradientBackground> bg(new KoGradientBackground(KoFlake::cloneGradient(g.data()), t));
                shape->setBackground(bg);
            }
        }
    }
}
