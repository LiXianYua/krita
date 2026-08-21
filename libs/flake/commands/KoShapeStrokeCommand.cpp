/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006-2008 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2012 Inge Wallin <inge@lysator.liu.se>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkXmlCompat.h>

#include "KoShapeStrokeCommand.h"
#include "KoShape.h"
#include "KoShapeStrokeModel.h"
#include <KoShapeBulkActionLock.h>
#include "kis_command_ids.h"


class Q_DECL_HIDDEN KoShapeStrokeCommand::Private
{
public:
    Private() {}
    ~Private()
    {
    }

    void addOldStroke(KoShapeStrokeModelSP oldStroke)
    {
        oldStrokes.append(oldStroke);
    }

    void addNewStroke(KoShapeStrokeModelSP newStroke)
    {
        newStrokes.append(newStroke);
    }

    PkList<KoShape*> shapes;                ///< the shapes to set stroke for
    PkList<KoShapeStrokeModelSP> oldStrokes; ///< the old strokes, one for each shape
    PkList<KoShapeStrokeModelSP> newStrokes; ///< the new strokes to set
};

KoShapeStrokeCommand::KoShapeStrokeCommand(const PkList<KoShape*> &shapes, KoShapeStrokeModelSP stroke, KUndo2Command *parent)
    : KUndo2Command(parent)
    , d(new Private())
{
    d->shapes = shapes;

    // save old strokes
    for (KoShape *shape : d->shapes) {
        d->addOldStroke(shape->stroke());
        d->addNewStroke(stroke);
    }

    setText(kundo2_text("Set stroke"));
}

KoShapeStrokeCommand::KoShapeStrokeCommand(const PkList<KoShape*> &shapes,
        const PkList<KoShapeStrokeModelSP> &strokes,
        KUndo2Command *parent)
        : KUndo2Command(parent)
        , d(new Private())
{
    Q_ASSERT(shapes.count() == strokes.count());

    d->shapes = shapes;

    // save old strokes
    for (KoShape *shape : shapes)
        d->addOldStroke(shape->stroke());
    for (KoShapeStrokeModelSP stroke : strokes)
        d->addNewStroke(stroke);

    setText(kundo2_text("Set stroke"));
}

KoShapeStrokeCommand::KoShapeStrokeCommand(KoShape* shape, KoShapeStrokeModelSP stroke, KUndo2Command *parent)
        : KUndo2Command(parent)
        , d(new Private())
{
    d->shapes.append(shape);
    d->addNewStroke(stroke);
    d->addOldStroke(shape->stroke());

    setText(kundo2_text("Set stroke"));
}

KoShapeStrokeCommand::~KoShapeStrokeCommand()
{
    delete d;
}

void KoShapeStrokeCommand::redo()
{
    KUndo2Command::redo();

    KoShapeBulkActionLock lock(d->shapes);

    PkList<KoShapeStrokeModelSP>::iterator strokeIt = d->newStrokes.begin();
    for (KoShape *shape : d->shapes) {
        shape->setStroke(*strokeIt);
        ++strokeIt;
    }

    KoShapeBulkActionLock::bulkShapesUpdate(lock.unlock());
}

void KoShapeStrokeCommand::undo()
{
    KUndo2Command::undo();

    KoShapeBulkActionLock lock(d->shapes);

    PkList<KoShapeStrokeModelSP>::iterator strokeIt = d->oldStrokes.begin();
    for (KoShape *shape : d->shapes) {
        shape->setStroke(*strokeIt);
        ++strokeIt;
    }

    KoShapeBulkActionLock::bulkShapesUpdate(lock.unlock());
}

int KoShapeStrokeCommand::id() const
{
    return KisCommandUtils::ChangeShapeStrokeId;
}

bool KoShapeStrokeCommand::mergeWith(const KUndo2Command *command)
{
    const KoShapeStrokeCommand *other = dynamic_cast<const KoShapeStrokeCommand*>(command);

    if (!other ||
        other->d->shapes != d->shapes) {

        return false;
    }

    d->newStrokes = other->d->newStrokes;
    return true;
}
