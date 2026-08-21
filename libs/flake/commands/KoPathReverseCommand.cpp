/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkXmlCompat.h>
#include "KoPathReverseCommand.h"
#include "KoPathShape.h"

class Q_DECL_HIDDEN KoPathReverseCommand::Private
{
public:
    Private(const PkList<KoPathShape*> &p)
            : paths(p) {
    }
    ~Private() {
    }

    void reverse() {
        if (! paths.size())
            return;

        for (KoPathShape* shape : paths) {
            int subpathCount = shape->subpathCount();
            for (int i = 0; i < subpathCount; ++i)
                shape->reverseSubpath(i);
        }
    }

    PkList<KoPathShape*> paths;
};

KoPathReverseCommand::KoPathReverseCommand(const PkList<KoPathShape*> &paths, KUndo2Command *parent)
        : KUndo2Command(parent),
        d(new Private(paths))
{
    setText(kundo2_text("Reverse paths"));
}

KoPathReverseCommand::~KoPathReverseCommand()
{
    delete d;
}

void KoPathReverseCommand::redo()
{
    KUndo2Command::redo();

    d->reverse();
}

void KoPathReverseCommand::undo()
{
    KUndo2Command::undo();

    d->reverse();
}
