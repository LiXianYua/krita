/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include <memory>

#include "KoShapeUngroupCommand.h"
#include "KoShapeContainer.h"
#include "KoShapeReorderCommand.h"
#include "kis_assert.h"


struct KoShapeUngroupCommand::Private
{
    Private(KoShapeContainer *_container,
            const PkList<KoShape *> &_shapes,
            const PkList<KoShape*> &_topLevelShapes)
        : container(_container),
          shapes(_shapes),
          topLevelShapes(_topLevelShapes)
    {
        std::stable_sort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);
        std::sort(topLevelShapes.begin(), topLevelShapes.end(), KoShape::compareShapeZIndex);
    }


    KoShapeContainer *container;
    PkList<KoShape*> shapes;
    PkList<KoShape*> topLevelShapes;
    std::unique_ptr<KUndo2Command> shapesReorderCommand;

};

KoShapeUngroupCommand::KoShapeUngroupCommand(KoShapeContainer *container, const PkList<KoShape *> &shapes,
                                             const PkList<KoShape*> &topLevelShapes, KUndo2Command *parent)
    : KUndo2Command(parent),
      m_d(new Private(container, shapes, topLevelShapes))
{
    setText(kundo2_text("Ungroup shapes"));
}

KoShapeUngroupCommand::~KoShapeUngroupCommand()
{
}

void KoShapeUngroupCommand::redo()
{
    using IndexedShape = KoShapeReorderCommand::IndexedShape;

    KoShapeContainer *newParent = m_d->container->parent();

    PkList<IndexedShape> indexedSiblings;
    PkList<KoShape*> perspectiveSiblings;

    if (newParent) {
        perspectiveSiblings = toPkList(newParent->shapes());
        std::sort(perspectiveSiblings.begin(), perspectiveSiblings.end(), KoShape::compareShapeZIndex);
    } else {
        perspectiveSiblings = m_d->topLevelShapes;
    }

    for (KoShape *shape : perspectiveSiblings) {
        indexedSiblings.append(shape);
    }

    // find the place where the ungrouped shapes should be inserted
    // (right on the top of their current container)
    auto insertIt = std::upper_bound(indexedSiblings.begin(),
                                     indexedSiblings.end(),
                                     IndexedShape(m_d->container));

    std::copy(m_d->shapes.begin(), m_d->shapes.end(),
              std::inserter(indexedSiblings, insertIt));

    indexedSiblings = KoShapeReorderCommand::homogenizeZIndexesLazy(indexedSiblings);

    const PkTransform ungroupTransform = toPkTransform(m_d->container->absoluteTransformation());
    for (auto it = m_d->shapes.begin(); it != m_d->shapes.end(); ++it) {
        KoShape *shape = *it;
        KIS_SAFE_ASSERT_RECOVER(shape->parent() == m_d->container) { continue; }

        shape->setParent(newParent);
        shape->applyAbsoluteTransformation(toQTransform(ungroupTransform));
    }

    if (!indexedSiblings.isEmpty()) {
        m_d->shapesReorderCommand.reset(new KoShapeReorderCommand(indexedSiblings));
        m_d->shapesReorderCommand->redo();
    }
}

void KoShapeUngroupCommand::undo()
{
    const PkTransform groupTransform = toPkTransform(m_d->container->absoluteTransformation().inverted());
    for (auto it = m_d->shapes.begin(); it != m_d->shapes.end(); ++it) {
        KoShape *shape = *it;

        shape->setParent(m_d->container);
        shape->applyAbsoluteTransformation(toQTransform(groupTransform));
    }

    if (m_d->shapesReorderCommand) {
        m_d->shapesReorderCommand->undo();
        m_d->shapesReorderCommand.reset();
    }
}
