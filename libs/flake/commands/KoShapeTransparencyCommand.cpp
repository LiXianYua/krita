/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkXmlCompat.h>

#include "KoShapeTransparencyCommand.h"
#include "KoShape.h"
#include "kis_command_ids.h"

class Q_DECL_HIDDEN KoShapeTransparencyCommand::Private
{
public:
    Private() {
    }
    ~Private() {
    }

    PkList<KoShape*> shapes;    ///< the shapes to set background for
    PkList<qreal> oldTransparencies; ///< the old transparencies
    PkList<qreal> newTransparencies; ///< the new transparencies
};

KoShapeTransparencyCommand::KoShapeTransparencyCommand(const PkList<KoShape*> &shapes, qreal transparency, KUndo2Command *parent)
    : KUndo2Command(parent)
    , d(new Private())
{
    d->shapes = shapes;
    for (KoShape *shape : d->shapes) {
        d->oldTransparencies.append(shape->transparency());
        d->newTransparencies.append(transparency);
    }

    setText(kundo2_text("Set opacity"));
}

KoShapeTransparencyCommand::KoShapeTransparencyCommand(KoShape * shape, qreal transparency, KUndo2Command *parent)
    : KUndo2Command(parent)
    , d(new Private())
{
    d->shapes.append(shape);
    d->oldTransparencies.append(shape->transparency());
    d->newTransparencies.append(transparency);

    setText(kundo2_text("Set opacity"));
}

KoShapeTransparencyCommand::KoShapeTransparencyCommand(const PkList<KoShape*> &shapes, const PkList<qreal> &transparencies, KUndo2Command *parent)
    : KUndo2Command(parent)
    , d(new Private())
{
    d->shapes = shapes;
    for (KoShape *shape : d->shapes) {
        d->oldTransparencies.append(shape->transparency());
    }
    d->newTransparencies = transparencies;

    setText(kundo2_text("Set opacity"));
}

KoShapeTransparencyCommand::~KoShapeTransparencyCommand()
{
    delete d;
}

void KoShapeTransparencyCommand::redo()
{
    KUndo2Command::redo();
    PkList<qreal>::iterator transparencyIt = d->newTransparencies.begin();
    for (KoShape *shape : d->shapes) {
        shape->setTransparency(*transparencyIt);
        shape->update();
        ++transparencyIt;
    }
}

void KoShapeTransparencyCommand::undo()
{
    KUndo2Command::undo();
    PkList<qreal>::iterator transparencyIt = d->oldTransparencies.begin();
    for (KoShape *shape : d->shapes) {
        shape->setTransparency(*transparencyIt);
        shape->update();
        ++transparencyIt;
    }
}

int KoShapeTransparencyCommand::id() const
{
    return KisCommandUtils::ChangeShapeTransparencyId;
}

bool KoShapeTransparencyCommand::mergeWith(const KUndo2Command *command)
{
    const KoShapeTransparencyCommand *other = dynamic_cast<const KoShapeTransparencyCommand*>(command);

    if (!other || other->d->shapes != d->shapes) {
        return false;
    }

    d->newTransparencies = other->d->newTransparencies;
    return true;
}
