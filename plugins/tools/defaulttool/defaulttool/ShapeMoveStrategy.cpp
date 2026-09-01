/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "ShapeMoveStrategy.h"
#include "SelectionDecorator.h"
#include "DefaultToolStrategyMath.h"

#include <KoCanvasBase.h>
#include <KoShapeManager.h>
#include <KoShapeContainer.h>
#include <KoShapeContainerModel.h>
#include <KoCanvasResourceProvider.h>
#include <commands/KoShapeMoveCommand.h>
#include <KoShapeBulkActionLock.h>
#include <KoSnapGuide.h>
#include <KoPointerEvent.h>
#include <KoToolBase.h>
#include <KoSelection.h>
#include <klocalizedstring.h>
#include <kis_global.h>

#include "kis_debug.h"

ShapeMoveStrategy::ShapeMoveStrategy(KoToolBase *tool, KoSelection *selection, const PkPointF &clicked)
    : KoInteractionStrategy(tool)
    , m_start(clicked)
    , m_canvas(tool->canvas())
{
    PkList<KoShape *> selectedShapes = selection->selectedEditableShapes();

    for (KoShape *shape : selectedShapes) {
        m_selectedShapes << shape;
        m_previousPositions << shape->absolutePosition(KoFlake::Center);
        m_newPositions << shape->absolutePosition(KoFlake::Center);
    }

    KoFlake::AnchorPosition anchor =
            KoFlake::AnchorPosition(
                m_canvas->resourceManager()->resource(KoFlake::HotPosition).toInt());

    m_initialOffset = selection->absolutePosition(anchor) - m_start;
    m_canvas->snapGuide()->setIgnoredShapes(KoShape::linearizeSubtree(m_selectedShapes));

    tool->setStatusText(PkString("Press Shift to hold x- or y-position."));
}

void ShapeMoveStrategy::handleMouseMove(const PkPointF &point, Qt::KeyboardModifiers modifiers)
{
    if (m_selectedShapes.isEmpty()) {
        return;
    }
    PkPointF diff = DefaultToolStrategyMath::moveDelta(m_start, point);

    if (modifiers & Qt::ShiftModifier) {
        // Limit change to one direction only
        diff = snapToClosestAxis(diff);
    } else {
        PkPointF positionToSnap = point + m_initialOffset;
        PkPointF snappedPosition = tool()->canvas()->snapGuide()->snap(positionToSnap, modifiers);
        diff = snappedPosition - m_initialOffset - m_start;
    }

    moveSelection(diff);
    m_finalMove = diff;
}

void ShapeMoveStrategy::moveSelection(const PkPointF &diff)
{
    KIS_ASSERT(m_newPositions.count());

    KoShapeBulkActionLock lock(m_selectedShapes);

    int i = 0;
    for (KoShape *shape : m_selectedShapes) {
        PkPointF delta = m_previousPositions.at(i) + diff - shape->absolutePosition(KoFlake::Center);
        if (shape->parent()) {
            shape->parent()->model()->proposeMove(shape, delta);
        }
        tool()->canvas()->clipToDocument(shape, delta);
        PkPointF newPos(shape->absolutePosition(KoFlake::Center) + delta);
        m_newPositions[i] = newPos;

        shape->setAbsolutePosition(newPos, KoFlake::Center);
        i++;
    }

    KoShapeBulkActionLock::bulkShapesUpdate(lock.unlock());
}

KUndo2Command *ShapeMoveStrategy::createCommand()
{
    tool()->canvas()->snapGuide()->reset();
    if (m_finalMove.isNull()) {
        return 0;
    }
    return new KoShapeMoveCommand(m_selectedShapes, m_previousPositions, m_newPositions);
}

void ShapeMoveStrategy::finishInteraction(Qt::KeyboardModifiers modifiers)
{
    (void)modifiers;
}

void ShapeMoveStrategy::paint(PkPainter &painter, const KoViewConverter &converter)
{
    (void)painter;
    (void)converter;
}
