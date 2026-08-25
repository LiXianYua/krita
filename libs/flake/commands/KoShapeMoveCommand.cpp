/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

#include "KoShapeMoveCommand.h"

#include <KoShape.h>
#include "kis_command_ids.h"
#include <kis_assert.h>
#include <KoShapeBulkActionLock.h>


class Q_DECL_HIDDEN KoShapeMoveCommand::Private
{
public:
    PkList<KoShape*> shapes;
    PkList<PkPointF> previousPositions, newPositions;
    KoFlake::AnchorPosition anchor;
};

KoShapeMoveCommand::KoShapeMoveCommand(const PkList<KoShape*> &shapes, PkList<PkPointF> &previousPositions, PkList<PkPointF> &newPositions, KoFlake::AnchorPosition anchor, KUndo2Command *parent)
        : KUndo2Command(kundo2_text("Move shapes"), parent),
        d(new Private())
{
    d->shapes = shapes;
    d->previousPositions = previousPositions;
    d->newPositions = newPositions;
    d->anchor = anchor;
    Q_ASSERT(d->shapes.count() == d->previousPositions.count());
    Q_ASSERT(d->shapes.count() == d->newPositions.count());
}

KoShapeMoveCommand::KoShapeMoveCommand(const PkList<KoShape *> &shapes, const PkPointF &offset, KUndo2Command *parent)
    : KUndo2Command(kundo2_text("Move shapes"), parent),
      d(new Private())
{
    d->shapes = shapes;
    d->anchor = KoFlake::Center;

    for (KoShape *shape : d->shapes) {
        const PkPointF pos = toPkPointF(shape->absolutePosition());

        d->previousPositions << pos;
        d->newPositions << pos + offset;
    }
}

KoShapeMoveCommand::~KoShapeMoveCommand()
{
    delete d;
}

void KoShapeMoveCommand::redo()
{
    KUndo2Command::redo();

    KoShapeBulkActionLock lock(toQList(d->shapes));

    for (int i = 0; i < d->shapes.count(); i++) {
        KoShape *shape = d->shapes.at(i);
        shape->setAbsolutePosition(toQPointF(d->newPositions.at(i)), d->anchor);
    }

    KoShapeBulkActionLock::bulkShapesUpdate(lock.unlock());
}

void KoShapeMoveCommand::undo()
{
    KUndo2Command::undo();

    KoShapeBulkActionLock lock(toQList(d->shapes));

    for (int i = 0; i < d->shapes.count(); i++) {
        KoShape *shape = d->shapes.at(i);
        shape->setAbsolutePosition(toQPointF(d->previousPositions.at(i)), d->anchor);
    }

    KoShapeBulkActionLock::bulkShapesUpdate(lock.unlock());
}

int KoShapeMoveCommand::id() const
{
    return KisCommandUtils::MoveShapeId;
}

bool KoShapeMoveCommand::mergeWith(const KUndo2Command *command)
{
    const KoShapeMoveCommand *other = dynamic_cast<const KoShapeMoveCommand*>(command);
    KIS_ASSERT(other);

    if (other->d->shapes != d->shapes ||
        other->d->anchor != d->anchor) {

        return false;
    }

    d->newPositions = other->d->newPositions;
    return true;
}
