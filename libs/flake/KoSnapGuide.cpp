/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008-2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkFlakeBridge.h>
#include "KoSnapGuide.h"
#include "KoSnapProxy.h"
#include "KoSnapStrategy.h"

#include <KoPathShape.h>
#include <KoPathPoint.h>
#include <KoViewConverter.h>
#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>

#include <pk/render/PkPainter.h>
#include <PkPainterPath.h>

#include <math.h>
#include "kis_pointer_utils.h"

class Q_DECL_HIDDEN KoSnapGuide::Private
{
public:
    Private(KoCanvasBase *parentCanvas)
        : canvas(parentCanvas), additionalEditedShape(0), currentStrategy(0),
        active(true),
        snapDistance(10)
    {
    }

    ~Private()
    {
        strategies.clear();
    }

    KoCanvasBase *canvas;
    KoShape *additionalEditedShape;

    typedef PkSharedPointer<KoSnapStrategy> KoSnapStrategySP;
    typedef PkList<KoSnapStrategySP> StrategiesList;
    StrategiesList strategies;
    KoSnapStrategySP currentStrategy;

    KoSnapGuide::Strategies usedStrategies;
    bool active;
    int snapDistance;
    PkList<KoPathPoint*> ignoredPoints;
    PkList<KoShape*> ignoredShapes;
};

KoSnapGuide::KoSnapGuide(KoCanvasBase *canvas)
    : d(new Private(canvas))
{
    d->strategies.append(PkSharedPointer<GridSnapStrategy>(new GridSnapStrategy()));
    d->strategies.append(PkSharedPointer<NodeSnapStrategy>(new NodeSnapStrategy()));
    d->strategies.append(PkSharedPointer<OrthogonalSnapStrategy>(new OrthogonalSnapStrategy()));
    d->strategies.append(PkSharedPointer<ExtensionSnapStrategy>(new ExtensionSnapStrategy()));
    d->strategies.append(PkSharedPointer<IntersectionSnapStrategy>(new IntersectionSnapStrategy()));
    d->strategies.append(PkSharedPointer<BoundingBoxSnapStrategy>(new BoundingBoxSnapStrategy()));
}

KoSnapGuide::~KoSnapGuide()
{
}

void KoSnapGuide::setAdditionalEditedShape(KoShape *shape)
{
    d->additionalEditedShape = shape;
}

KoShape *KoSnapGuide::additionalEditedShape() const
{
    return d->additionalEditedShape;
}

void KoSnapGuide::enableSnapStrategy(Strategy type, bool value)
{
    if (value) {
        d->usedStrategies |= type;
    } else {
        d->usedStrategies &= ~type;
    }
}

bool KoSnapGuide::isStrategyEnabled(Strategy type) const
{
    return d->usedStrategies & type;
}

void KoSnapGuide::enableSnapStrategies(Strategies strategies)
{
    d->usedStrategies = strategies;
}

KoSnapGuide::Strategies KoSnapGuide::enabledSnapStrategies() const
{
    return d->usedStrategies;
}

bool KoSnapGuide::addCustomSnapStrategy(KoSnapStrategy *customStrategy)
{
    if (!customStrategy || customStrategy->type() != CustomSnapping)
        return false;

    d->strategies.append(PkSharedPointer<KoSnapStrategy>(customStrategy));
    return true;
}

void KoSnapGuide::overrideSnapStrategy(Strategy type, KoSnapStrategy *strategy)
{
    for (auto it = d->strategies.begin(); it != d->strategies.end(); /*noop*/) {
        if ((*it)->type() == type) {
            if (strategy) {
                *it = PkSharedPointer<KoSnapStrategy>(strategy);
            } else {
                it = d->strategies.erase(it);
            }
            return;
        } else {
            ++it;
        }
    }

    if (strategy) {
        d->strategies.append(PkSharedPointer<KoSnapStrategy>(strategy));
    }
}

void KoSnapGuide::enableSnapping(bool on)
{
    d->active = on;
}

bool KoSnapGuide::isSnapping() const
{
    return d->active;
}

void KoSnapGuide::setSnapDistance(int distance)
{
    d->snapDistance = qAbs(distance);
}

int KoSnapGuide::snapDistance() const
{
    return d->snapDistance;
}

PkPointF KoSnapGuide::snap(const PkPointF &mousePosition, const PkPointF &dragOffset, Pk::KeyboardModifiers modifiers)
{
    PkPointF pos = mousePosition + dragOffset;
    pos = snap(pos, modifiers);
    return pos - dragOffset;
}

PkPointF KoSnapGuide::snap(const PkPointF &mousePosition, Pk::KeyboardModifiers modifiers)
{
    d->currentStrategy.clear();

    if (! d->active || (modifiers & Pk::ShiftModifier))
        return mousePosition;

    KoSnapProxy proxy(this);

    using PriorityTuple = std::tuple<KoSnapStrategy::SnapType, qreal>;
    PriorityTuple minPriority(KoSnapStrategy::ToLine, HUGE_VAL);

    const qreal maxSnapDistance = d->canvas->viewConverter()->
            viewToDocument(PkSizeF(d->snapDistance,
                                  d->snapDistance)).width();

    foreach (Private::KoSnapStrategySP strategy, d->strategies) {
        if (d->usedStrategies & strategy->type() ||
            strategy->type() == GridSnapping ||
            strategy->type() == CustomSnapping) {

            if (! strategy->snap(mousePosition, &proxy, maxSnapDistance))
                continue;

            PkPointF snapCandidate = strategy->snappedPosition();
            qreal distance = KoSnapStrategy::squareDistance(snapCandidate, mousePosition);

            const PriorityTuple priority(strategy->snappedType(), distance);
            if (priority < minPriority) {
                d->currentStrategy = strategy;
                minPriority = priority;
            }
        }
    }

    if (! d->currentStrategy)
        return mousePosition;

    return d->currentStrategy->snappedPosition();
}

PkRectF KoSnapGuide::boundingRect()
{
    PkRectF rect;

    if (d->currentStrategy) {
        rect = d->currentStrategy->decoration(*d->canvas->viewConverter()).boundingRect();
        return rect.adjusted(-2, -2, 2, 2);
    } else {
        return rect;
    }
}

void KoSnapGuide::paint(PkPainter &painter, const KoViewConverter &converter)
{
    if (! d->currentStrategy || ! d->active)
        return;

    PkPainterPath decoration = d->currentStrategy->decoration(converter);

    int thickness = d->canvas->resourceManager()? d->canvas->resourceManager()->decorationThickness(): 1;

    painter.setBrush(Pk::NoBrush);

    PkPen whitePen(Pk::white, thickness);
    whitePen.setCosmetic(true);
    whitePen.setStyle(Pk::SolidLine);
    painter.setPen(whitePen);
    painter.drawPath(decoration);

    PkPen redPen(Pk::red, thickness);
    redPen.setCosmetic(true);
    redPen.setStyle(Pk::DotLine);
    painter.setPen(redPen);
    painter.drawPath(decoration);
}

KoCanvasBase *KoSnapGuide::canvas() const
{
    return d->canvas;
}

void KoSnapGuide::setIgnoredPathPoints(const PkList<KoPathPoint*> &ignoredPoints)
{
    d->ignoredPoints = ignoredPoints;
}

PkList<KoPathPoint*> KoSnapGuide::ignoredPathPoints() const
{
    return d->ignoredPoints;
}

void KoSnapGuide::setIgnoredShapes(const PkList<KoShape*> &ignoredShapes)
{
    d->ignoredShapes = ignoredShapes;
}

PkList<KoShape*> KoSnapGuide::ignoredShapes() const
{
    return d->ignoredShapes;
}

void KoSnapGuide::reset()
{
    d->currentStrategy.clear();
    d->additionalEditedShape = 0;
    d->ignoredPoints.clear();
    d->ignoredShapes.clear();
    // remove all custom strategies
    int strategyCount = d->strategies.count();
    for (int i = strategyCount-1; i >= 0; --i) {
        if (d->strategies[i]->type() == CustomSnapping) {
            d->strategies.removeAt(i);
        }
    }
}
