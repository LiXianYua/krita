/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

#include "kis_command_ids.h"

#include <KoShapeBulkActionLock.h>
#include "KoShapeTransformCommand.h"
#include "KoShape.h"

#include <pk/container/PkList.h>
#include <pk/geometry/PkTransform.h>

#include <FlakeDebug.h>

class Q_DECL_HIDDEN KoShapeTransformCommand::Private
{
public:
    Private(const PkList<KoShape*> &list) : shapes(list) { }
    PkList<KoShape*> shapes;
    PkList<PkTransform> oldState;
    PkList<PkTransform> newState;
};

KoShapeTransformCommand::KoShapeTransformCommand(const PkList<KoShape*> &shapes, const PkList<PkTransform> &oldState, const PkList<PkTransform> &newState, KUndo2Command * parent)
        : KUndo2Command(parent),
        d(new Private(shapes))
{
    Q_ASSERT(shapes.count() == oldState.count());
    Q_ASSERT(shapes.count() == newState.count());
    d->oldState = oldState;
    d->newState = newState;
}

KoShapeTransformCommand::~KoShapeTransformCommand()
{
    delete d;
}

void KoShapeTransformCommand::redo()
{
    KUndo2Command::redo();

    KoShapeBulkActionLock lock(toQList(d->shapes));

    const int shapeCount = d->shapes.count();
    for (int i = 0; i < shapeCount; ++i) {
        KoShape * shape = d->shapes[i];
        shape->setTransformation(toQTransform(d->newState[i]));
    }

    KoShapeBulkActionLock::bulkShapesUpdate(lock.unlock());
}

void KoShapeTransformCommand::undo()
{
    KUndo2Command::undo();

    KoShapeBulkActionLock lock(toQList(d->shapes));

    const int shapeCount = d->shapes.count();
    for (int i = 0; i < shapeCount; ++i) {
        KoShape * shape = d->shapes[i];
        shape->setTransformation(toQTransform(d->oldState[i]));
    }

    KoShapeBulkActionLock::bulkShapesUpdate(lock.unlock());
}

int KoShapeTransformCommand::id() const
{
    return KisCommandUtils::TransformShapeId;
}

bool KoShapeTransformCommand::mergeWith(const KUndo2Command *command)
{
    const KoShapeTransformCommand *other = dynamic_cast<const KoShapeTransformCommand*>(command);

    if (!other ||
        other->d->shapes != d->shapes ||
        other->text() != text()) {

        return false;
    }

    d->newState = other->d->newState;
    return true;
}
