/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include <pk/pointer/PkSharedPointer.h>

#include "KoShapeBackgroundCommand.h"
#include "KoShape.h"
#include "KoShapeBackground.h"
#include "kis_command_ids.h"

// PkSharedPointer ↔ QSharedPointer 过渡助手现居 PkFlakeBridge.h（保活 deleter 模式，
// 同款实现，本文件 include 该桥接头）：命令类在 KoShape::background()（真 Qt 返回
// QSharedPointer）与命令参数（PkSharedPointer）之间跨界时调用。flake 剥完（共享指针
// 归 Pk）后桥接连同本文件调用点一起删。

class Q_DECL_HIDDEN KoShapeBackgroundCommand::Private
{
public:
    Private() {
    }
    ~Private() {
        oldFills.clear();
        newFills.clear();
    }

    void addOldFill(PkSharedPointer<KoShapeBackground>  oldFill)
    {
        oldFills.append(oldFill);
    }

    void addNewFill(PkSharedPointer<KoShapeBackground>  newFill)
    {
        newFills.append(newFill);
    }

    PkList<KoShape*> shapes;    ///< the shapes to set background for
    PkList<PkSharedPointer<KoShapeBackground> > oldFills;
    PkList<PkSharedPointer<KoShapeBackground> > newFills;
};

KoShapeBackgroundCommand::KoShapeBackgroundCommand(const PkList<KoShape*> &shapes, PkSharedPointer<KoShapeBackground>  fill,
        KUndo2Command *parent)
        : KUndo2Command(parent)
        , d(new Private())
{
    d->shapes = shapes;
    for (KoShape *shape : d->shapes) {
        d->addOldFill(toPkSharedPointer(shape->background()));
        d->addNewFill(fill);
    }

    setText(kundo2_text("Set background"));
}

KoShapeBackgroundCommand::KoShapeBackgroundCommand(KoShape * shape, PkSharedPointer<KoShapeBackground>  fill, KUndo2Command *parent)
        : KUndo2Command(parent)
        , d(new Private())
{
    d->shapes.append(shape);
    d->addOldFill(toPkSharedPointer(shape->background()));
    d->addNewFill(fill);

    setText(kundo2_text("Set background"));
}

KoShapeBackgroundCommand::KoShapeBackgroundCommand(const PkList<KoShape*> &shapes, const PkList<PkSharedPointer<KoShapeBackground> > &fills, KUndo2Command *parent)
        : KUndo2Command(parent)
        , d(new Private())
{
    d->shapes = shapes;
    for (KoShape *shape : d->shapes) {
        d->addOldFill(toPkSharedPointer(shape->background()));
    }
    for (PkSharedPointer<KoShapeBackground>  fill : fills) {
        d->addNewFill(fill);
    }

    setText(kundo2_text("Set background"));
}

void KoShapeBackgroundCommand::redo()
{
    KUndo2Command::redo();
    PkList<PkSharedPointer<KoShapeBackground> >::iterator brushIt = d->newFills.begin();
    for (KoShape *shape : d->shapes) {
        shape->setBackground(toQSharedPointer(*brushIt));
        shape->update();
        ++brushIt;
    }
}

void KoShapeBackgroundCommand::undo()
{
    KUndo2Command::undo();
    PkList<PkSharedPointer<KoShapeBackground> >::iterator brushIt = d->oldFills.begin();
    for (KoShape *shape : d->shapes) {
        shape->setBackground(toQSharedPointer(*brushIt));
        shape->update();
        ++brushIt;
    }
}

int KoShapeBackgroundCommand::id() const
{
    return KisCommandUtils::ChangeShapeBackgroundId;
}

bool KoShapeBackgroundCommand::mergeWith(const KUndo2Command *command)
{
    const KoShapeBackgroundCommand *other = dynamic_cast<const KoShapeBackgroundCommand*>(command);

    if (!other ||
        other->d->shapes != d->shapes) {

        return false;
    }

    d->newFills= other->d->newFills;
    return true;
}

KoShapeBackgroundCommand::~KoShapeBackgroundCommand()
{
    delete d;
}
