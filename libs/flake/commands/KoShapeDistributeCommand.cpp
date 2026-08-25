/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

#include "KoShapeDistributeCommand.h"

#include "commands/KoShapeMoveCommand.h"
#include "KoShape.h"
#include <pk/container/PkMap.h>
#include <pk/container/PkMapIterator.h>
class Q_DECL_HIDDEN KoShapeDistributeCommand::Private
{
public:
    Private() : command(0) {}
    ~Private() {
        delete command;
    }

    qreal getAvailableSpace(KoShape *first, KoShape *last, qreal extent, const PkRectF &boundingRect);

    Distribute distribute;
    KoShapeMoveCommand *command;
};

KoShapeDistributeCommand::KoShapeDistributeCommand(const PkList<KoShape*> &shapes, Distribute distribute, const PkRectF &boundingRect, KUndo2Command *parent)
        : KUndo2Command(parent),
        d(new Private())
{
    d->distribute = distribute;
    PkMap<qreal, KoShape*> sortedPos;
    PkRectF bRect;
    qreal extent = 0.0;
    // sort by position and calculate sum of objects width/height
    for (KoShape *shape : shapes) {
        bRect = toPkRectF(shape->absoluteOutlineRect());
        switch (d->distribute) {
        case HorizontalCenterDistribution:
            sortedPos[bRect.center().x()] = shape;
            break;
        case HorizontalGapsDistribution:
        case HorizontalLeftDistribution:
            sortedPos[bRect.left()] = shape;
            extent += bRect.width();
            break;
        case HorizontalRightDistribution:
            sortedPos[bRect.right()] = shape;
            break;
        case VerticalCenterDistribution:
            sortedPos[bRect.center().y()] = shape;
            break;
        case VerticalGapsDistribution:
        case VerticalBottomDistribution:
            sortedPos[bRect.bottom()] = shape;
            extent += bRect.height();
            break;
        case VerticalTopDistribution:
            sortedPos[bRect.top()] = shape;
            break;
        }
    }
    KoShape* first = sortedPos.begin().value();
    KoShape* last = sortedPos.begin().value();
    for (PkMap<qreal, KoShape*>::iterator it = sortedPos.begin(); it != sortedPos.end(); ++it) {
        last = it.value();
    }

    // determine the available space to distribute
    qreal space = d->getAvailableSpace(first, last, extent, boundingRect);
    qreal pos = 0.0, step = space / qreal(shapes.count() - 1);

    PkList<PkPointF> previousPositions;
    PkList<PkPointF> newPositions;
    PkPointF position;
    PkPointF delta;
    PkMapIterator<qreal, KoShape*> it(sortedPos);
    while (it.hasNext()) {
        it.next();
        position = toPkPointF(it.value()->absolutePosition());
        previousPositions  << position;

        bRect = toPkRectF(it.value()->absoluteOutlineRect());
        switch (d->distribute)        {
        case HorizontalCenterDistribution:
            delta = PkPointF(boundingRect.x() + first->absoluteOutlineRect().width() / 2 + pos - bRect.width() / 2, bRect.y()) - bRect.topLeft();
            break;
        case HorizontalGapsDistribution:
            delta = PkPointF(boundingRect.left() + pos, bRect.y()) - bRect.topLeft();
            pos += bRect.width();
            break;
        case HorizontalLeftDistribution:
            delta = PkPointF(boundingRect.left() + pos, bRect.y()) - bRect.topLeft();
            break;
        case HorizontalRightDistribution:
            delta = PkPointF(boundingRect.left() + first->absoluteOutlineRect().width() + pos - bRect.width(), bRect.y()) - bRect.topLeft();
            break;
        case VerticalCenterDistribution:
            delta = PkPointF(bRect.x(), boundingRect.y() + first->absoluteOutlineRect().height() / 2 + pos - bRect.height() / 2) - bRect.topLeft();
            break;
        case VerticalGapsDistribution:
            delta = PkPointF(bRect.x(), boundingRect.top() + pos) - bRect.topLeft();
            pos += bRect.height();
            break;
        case VerticalBottomDistribution:
            delta = PkPointF(bRect.x(), boundingRect.top() + first->absoluteOutlineRect().height() + pos - bRect.height()) - bRect.topLeft();
            break;
        case VerticalTopDistribution:
            delta = PkPointF(bRect.x(), boundingRect.top() + pos) - bRect.topLeft();
            break;
        };
        newPositions  << position + delta;
        pos += step;
    }
    d->command = new KoShapeMoveCommand(sortedPos.values(), previousPositions, newPositions);

    setText(kundo2_text("Distribute shapes"));
}

KoShapeDistributeCommand::~KoShapeDistributeCommand()
{
    delete d;
}

void KoShapeDistributeCommand::redo()
{
    KUndo2Command::redo();
    d->command->redo();
}

void KoShapeDistributeCommand::undo()
{
    KUndo2Command::undo();
    d->command->undo();
}

qreal KoShapeDistributeCommand::Private::getAvailableSpace(KoShape *first, KoShape *last, qreal extent, const PkRectF &boundingRect)
{
    switch (distribute) {
    case HorizontalCenterDistribution:
        return boundingRect.width() - last->absoluteOutlineRect().width() / 2 - first->absoluteOutlineRect().width() / 2;
        break;
    case HorizontalGapsDistribution:
        return boundingRect.width() - extent;
        break;
    case HorizontalLeftDistribution:
        return boundingRect.width() - last->absoluteOutlineRect().width();
        break;
    case HorizontalRightDistribution:
        return boundingRect.width() - first->absoluteOutlineRect().width();
        break;
    case VerticalCenterDistribution:
        return boundingRect.height() - last->absoluteOutlineRect().height() / 2 - first->absoluteOutlineRect().height() / 2;
        break;
    case VerticalGapsDistribution:
        return boundingRect.height() - extent;
        break;
    case VerticalBottomDistribution:
        return boundingRect.height() - first->absoluteOutlineRect().height();
        break;
    case VerticalTopDistribution:
        return boundingRect.height() - last->absoluteOutlineRect().height();
        break;
    }
    return 0.0;
}
