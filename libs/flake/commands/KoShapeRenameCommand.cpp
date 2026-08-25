/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoShapeRenameCommand.h"

#include "KoShape.h"

class Q_DECL_HIDDEN KoShapeRenameCommand::Private
{
public:
    Private(KoShape *shape, const PkString &newName)
    : shape(shape)
    , newName(newName)
    , oldName(toPkString(shape->name()))
    {}

    KoShape *shape;
    PkString newName;
    PkString oldName;
};

KoShapeRenameCommand::KoShapeRenameCommand(KoShape *shape, const PkString &newName, KUndo2Command *parent)
    : KUndo2Command(kundo2_text("Rename Shape"), parent)
, d(new Private(shape, newName))
{
}

KoShapeRenameCommand::~KoShapeRenameCommand()
{
   delete d;
}

void KoShapeRenameCommand::redo()
{
    KUndo2Command::redo();
    d->shape->setName(toQString(d->newName));
}

void KoShapeRenameCommand::undo()
{
    KUndo2Command::undo();
    d->shape->setName(toQString(d->oldName));
}
