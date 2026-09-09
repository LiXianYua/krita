/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006 Boudewijn Rempt <boud@valdyas.org>
   SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2006-2007, 2009 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoSelection.h"
#include "KoSelection_p.h"
#include "KoShapeContainer.h"
#include "KoShapeGroup.h"
#include "KoPointerEvent.h"
#include "kis_algebra_2d.h"
#include "krita_container_utils.h"

#include <PkPainter.h>

#include "kis_debug.h"
KoSelection::KoSelection(PkObject *parent)
    : PkObject(parent)
    , KoShape()
    , d(new Private)
{
    PkObject::connect(d->selectionChangedCompressor, &KisThreadSafeSignalCompressor::timeout,
                      this, [this]() { selectionChanged(); });
}

KoSelection::KoSelection(const KoSelection &rhs)
    : PkObject()
    , KoShape(rhs)
    , d(rhs.d)
{
}

KoSelection::~KoSelection()
{
}

void KoSelection::selectionChanged()
{
    activateSignal<>(this, PkMemberFnKey::from(&KoSelection::selectionChanged));
}

void KoSelection::currentLayerChanged(const KoShapeLayer *layer)
{
    activateSignal<const KoShapeLayer *>(
        this, PkMemberFnKey::from(&KoSelection::currentLayerChanged), layer);
}

void KoSelection::paint(PkPainter &painter) const
{
    Q_UNUSED(painter);
}

void KoSelection::setSize(const PkSizeF &size)
{
    Q_UNUSED(size);
    qWarning() << "WARNING: KoSelection::setSize() should never be used!";
}

PkSizeF KoSelection::size() const
{
    return outlineRect().size();
}

PkRectF KoSelection::outlineRect() const
{
    const PkTransform invertedTransform = transformation().inverted();
    PkRectF boundingRect;

    Q_FOREACH (KoShape *shape, selectedVisibleShapes()) {
        // it is cheaper to invert-transform each outline, than
        // to group 300+ rotated rectangles into a polygon
        boundingRect |=
            invertedTransform.map(
                shape->absoluteTransformation().map(
                        PkPolygonF(shape->outlineRect()))).boundingRect();
    }

    return boundingRect;
}

PkRectF KoSelection::boundingRect() const
{
    return KoShape::boundingRect(selectedVisibleShapes());
}

void KoSelection::select(KoShape *shape)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(shape != this);
    KIS_SAFE_ASSERT_RECOVER_RETURN(shape);

    if (!shape->isSelectable() || !shape->isVisible()) {
        return;
    }

    // check recursively
    if (isSelected(shape)) {
        return;
    }

    // find the topmost parent to select
    while (KoShapeGroup *parentGroup = dynamic_cast<KoShapeGroup*>(shape->parent())) {
        if (parentGroup && parentGroup->isSelectable()) {
            shape = parentGroup;
        } else {
            break;
        }
    }

    d->selectedShapes << shape;
    shape->addShapeChangeListener(this);

    if (d->selectedShapes.size() == 1) {
        setTransformation(shape->absoluteTransformation());
    } else {
        setTransformation(PkTransform());
    }

    d->selectionChangedCompressor->start();
}

void KoSelection::deselect(KoShape *shape)
{
    if (!d->selectedShapes.contains(shape))
        return;

    d->selectedShapes.removeAll(shape);
    shape->removeShapeChangeListener(this);

    if (d->selectedShapes.size() == 1) {
        setTransformation(d->selectedShapes.first()->absoluteTransformation());
    }

    d->selectionChangedCompressor->start();
}

void KoSelection::deselectAll()
{

    if (d->selectedShapes.isEmpty())
        return;

    Q_FOREACH (KoShape *shape, d->selectedShapes) {
        shape->removeShapeChangeListener(this);
    }

    // reset the transformation matrix of the selection
    setTransformation(PkTransform());

    d->selectedShapes.clear();
    d->selectionChangedCompressor->start();
}

int KoSelection::count() const
{
    return d->selectedShapes.size();
}

bool KoSelection::hitTest(const PkPointF &position) const
{

    Q_FOREACH (KoShape *shape, d->selectedShapes) {
        if (shape->isVisible()) continue;
        if (shape->hitTest(position)) return true;
    }

    return false;
}

const PkList<KoShape*> KoSelection::selectedShapes() const
{
    return d->selectedShapes;
}

const PkList<KoShape *> KoSelection::selectedVisibleShapes() const
{
    PkList<KoShape*> shapes = selectedShapes();

    KritaUtils::filterContainer (shapes, [](KoShape *shape) {
        return shape->isVisible();
    });

    return shapes;
}

const PkList<KoShape *> KoSelection::selectedEditableShapes() const
{
    PkList<KoShape*> shapes = selectedShapes();

    KritaUtils::filterContainer (shapes, [](KoShape *shape) {
        return shape->isShapeEditable();
    });

    return shapes;
}

const PkList<KoShape *> KoSelection::selectedEditableShapesAndDelegates() const
{
    PkList<KoShape*> shapes;
    Q_FOREACH (KoShape *shape, selectedShapes()) {
        PkSet<KoShape *> delegates = shape->toolDelegates();
        if (delegates.isEmpty()) {
            shapes.append(shape);
        } else {
            Q_FOREACH (KoShape *delegatedShape, delegates) {
                shapes.append(delegatedShape);
            }
        }
    }
    return shapes;
}

bool KoSelection::isSelected(const KoShape *shape) const
{
    if (shape == this)
        return true;

    const KoShape *tmpShape = shape;
    while (tmpShape && std::find(d->selectedShapes.begin(), d->selectedShapes.end(), tmpShape) == d->selectedShapes.end()) {
        tmpShape = tmpShape->parent();
    }

    return tmpShape;
}

KoShape *KoSelection::firstSelectedShape() const
{
    return !d->selectedShapes.isEmpty() ? d->selectedShapes.first() : 0;
}

void KoSelection::setActiveLayer(KoShapeLayer *layer)
{
    d->activeLayer = layer;
    Q_EMIT currentLayerChanged(layer);
}

KoShapeLayer* KoSelection::activeLayer() const
{
    return d->activeLayer;
}

void KoSelection::notifyShapeChanged(KoShape::ChangeType type, KoShape *shape)
{
    Q_UNUSED(shape);
    if (type == KoShape::Deleted) {
        deselect(shape);

        // HACK ALERT: the caller will also remove the listener, which was
        // removed in deselect(), so re-add it here
        shape->addShapeChangeListener(this);
    }
}
