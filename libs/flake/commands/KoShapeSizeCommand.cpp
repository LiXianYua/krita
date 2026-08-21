/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkXmlCompat.h>

#include "KoShapeSizeCommand.h"

#include <KoShape.h>
class Q_DECL_HIDDEN KoShapeSizeCommand::Private
{
public:
    PkList<KoShape*> shapes;
    PkList<PkSizeF> previousSizes, newSizes;
};

KoShapeSizeCommand::KoShapeSizeCommand(const PkList<KoShape*> &shapes, const PkList<PkSizeF> &previousSizes, const PkList<PkSizeF> &newSizes, KUndo2Command *parent)
        : KUndo2Command(parent),
        d(new Private())
{
    d->previousSizes = previousSizes;
    d->newSizes = newSizes;
    d->shapes = shapes;
    Q_ASSERT(d->shapes.count() == d->previousSizes.count());
    Q_ASSERT(d->shapes.count() == d->newSizes.count());

    setText(kundo2_text("Resize shapes"));
}

KoShapeSizeCommand::~KoShapeSizeCommand()
{
    delete d;
}

void KoShapeSizeCommand::redo()
{
    KUndo2Command::redo();
    int i = 0;
    for (KoShape *shape : d->shapes) {
        shape->update();
        shape->setSize(d->newSizes[i++]);
        shape->update();
    }
}

void KoShapeSizeCommand::undo()
{
    KUndo2Command::undo();
    int i = 0;
    for (KoShape *shape : d->shapes) {
        shape->update();
        shape->setSize(d->previousSizes[i++]);
        shape->update();
    }
}
