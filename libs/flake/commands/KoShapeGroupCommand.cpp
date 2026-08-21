/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006, 2010 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006, 2007 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkXmlCompat.h>
#include <memory>

#include "KoShapeGroupCommand.h"
#include "KoShape.h"
#include "KoShapeGroup.h"
#include "KoShapeContainer.h"

#include <commands/KoShapeReorderCommand.h>
// static
KoShapeGroupCommand * KoShapeGroupCommand::createCommand(KoShapeContainer *container, const PkList<KoShape *> &shapes, bool shouldNormalize)
{
    PkList<KoShape*> orderedShapes(shapes);
    if (!orderedShapes.isEmpty()) {
        KoShape * top = orderedShapes.last();
        container->setParent(top->parent());
        container->setZIndex(top->zIndex());
    }

    return new KoShapeGroupCommand(container, orderedShapes, shouldNormalize, 0);
}

class KoShapeGroupCommandPrivate
{
public:
    KoShapeGroupCommandPrivate(KoShapeContainer *container, const PkList<KoShape *> &shapes, bool _shouldNormalize);
    PkRectF containerBoundingRect();

    PkList<KoShape*> shapes; ///<list of shapes to be grouped
    bool shouldNormalize; ///< Adjust the coordinate system of the group to its origin into the topleft of the group
    KoShapeContainer *container; ///< the container where the grouping should be for.
    PkList<KoShapeContainer*> oldParents; ///< the old parents of the shapes

    std::unique_ptr<KUndo2Command> shapesReorderCommand;
};

KoShapeGroupCommandPrivate::KoShapeGroupCommandPrivate(KoShapeContainer *c, const PkList<KoShape *> &s, bool _shouldNormalize)
    : shapes(s),
      shouldNormalize(_shouldNormalize),
      container(c)
{
    std::stable_sort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
}

KoShapeGroupCommand::KoShapeGroupCommand(KoShapeContainer *container, const PkList<KoShape *> &shapes, KUndo2Command *parent)
    : KoShapeGroupCommand(container, shapes, false, parent)
{
}

KoShapeGroupCommand::KoShapeGroupCommand(KoShapeContainer *container, const PkList<KoShape *> &shapes,
                                         bool shouldNormalize, KUndo2Command *parent)
    : KUndo2Command(parent),
      d(new KoShapeGroupCommandPrivate(container, shapes, shouldNormalize))
{
    for (KoShape* shape : d->shapes) {
        d->oldParents.append(shape->parent());
    }

    if (d->container->shapes().isEmpty()) {
        setText(kundo2_text("Group shapes"));
    } else {
        setText(kundo2_text("Add shapes to group"));
    }
}

KoShapeGroupCommand::~KoShapeGroupCommand()
{
}

void KoShapeGroupCommand::redo()
{
    KUndo2Command::redo();

    if (d->shouldNormalize &&  dynamic_cast<KoShapeGroup*>(d->container)) {
        PkRectF bound = d->containerBoundingRect();
        PkPointF oldGroupPosition = d->container->absolutePosition(KoFlake::TopLeft);
        d->container->setAbsolutePosition(bound.topLeft(), KoFlake::TopLeft);
        d->container->setSize(bound.size());

        if (d->container->shapeCount() > 0) {
            // the group has changed position and so have the group child shapes
            // -> we need compensate the group position change
            PkPointF positionOffset = oldGroupPosition - bound.topLeft();
            for (KoShape * child : d->container->shapes())
                child->setAbsolutePosition(child->absolutePosition() + positionOffset);
        }
    }

    PkTransform groupTransform = d->container->absoluteTransformation().inverted();

    PkList<KoShape*> containerShapes(d->container->shapes());
    std::stable_sort(containerShapes.begin(), containerShapes.end(), KoShape::compareShapeZIndex);

    PkList<KoShapeReorderCommand::IndexedShape> indexedShapes;
    for (KoShape *shape : containerShapes) {
        indexedShapes.append(KoShapeReorderCommand::IndexedShape(shape));
    }

    PkList<KoShapeReorderCommand::IndexedShape> prependIndexedShapes;

    for (KoShape *shape : d->shapes) {
        // test if they inherit the same parent

        if (!shape->hasCommonParent(d->container) ||
            !KoShape::compareShapeZIndex(shape, d->container)) {

            indexedShapes.append(KoShapeReorderCommand::IndexedShape(shape));
        } else {
            prependIndexedShapes.append(KoShapeReorderCommand::IndexedShape(shape));
        }
    }

    prependIndexedShapes.append(indexedShapes);
    indexedShapes = prependIndexedShapes;
    indexedShapes = KoShapeReorderCommand::homogenizeZIndexesLazy(indexedShapes);

    if (!indexedShapes.isEmpty()) {
        d->shapesReorderCommand.reset(new KoShapeReorderCommand(indexedShapes));
        d->shapesReorderCommand->redo();
    }

    uint shapeCount = d->shapes.count();
    for (uint i = 0; i < shapeCount; ++i) {
        KoShape * shape = d->shapes[i];

        shape->applyAbsoluteTransformation(groupTransform);
        d->container->addShape(shape);
    }


}

void KoShapeGroupCommand::undo()
{
    KUndo2Command::undo();

    PkTransform ungroupTransform = d->container->absoluteTransformation();
    for (int i = 0; i < d->shapes.count(); i++) {
        KoShape * shape = d->shapes[i];
        d->container->removeShape(shape);
        if (d->oldParents.at(i)) {
            d->oldParents.at(i)->addShape(shape);
        }
        shape->applyAbsoluteTransformation(ungroupTransform);
    }

    if (d->shapesReorderCommand) {
        d->shapesReorderCommand->undo();
        d->shapesReorderCommand.reset();
    }

    if (d->shouldNormalize && dynamic_cast<KoShapeGroup*>(d->container)) {
        PkPointF oldGroupPosition = d->container->absolutePosition(KoFlake::TopLeft);
        if (d->container->shapeCount() > 0) {
            bool boundingRectInitialized = false;
            PkRectF bound;
            for (KoShape * shape : d->container->shapes()) {
                if (! boundingRectInitialized) {
                    bound = shape->boundingRect();
                    boundingRectInitialized = true;
                } else
                    bound = bound.united(shape->boundingRect());
            }
            // the group has changed position and so have the group child shapes
            // -> we need compensate the group position change
            PkPointF positionOffset = oldGroupPosition - bound.topLeft();
            for (KoShape * child : d->container->shapes())
                child->setAbsolutePosition(child->absolutePosition() + positionOffset);

            d->container->setAbsolutePosition(bound.topLeft(), KoFlake::TopLeft);
            d->container->setSize(bound.size());
        }
    }
}

PkRectF KoShapeGroupCommandPrivate::containerBoundingRect()
{
    PkRectF bound;
    if (container->shapeCount() > 0) {
        bound = container->absoluteTransformation().mapRect(container->outlineRect());
    }

    for (KoShape *shape : shapes) {
        bound |= shape->absoluteTransformation().mapRect(shape->outlineRect());
    }

    return bound;
}
