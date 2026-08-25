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

namespace {
// PkSharedPointer ↔ QSharedPointer 过渡助手（桥接头没有共享指针互转：真 Qt 的
// QSharedPointer 无 std::shared_ptr 互操作构造）。用「保活 deleter」模式——两边
// 各自持有独立控制块，转换方把原指针的副本捕获进 deleter，借副本维持所有权；
// 对象只被原控制块删除一次，不会双删。
template <typename T>
inline PkSharedPointer<T> toPkSharedPointer(const QSharedPointer<T> &p)
{
    QSharedPointer<T> keep = p;
    return PkSharedPointer<T>(p.data(), [keep](T *) { (void)keep; });
}

template <typename T>
inline QSharedPointer<T> toQSharedPointer(const PkSharedPointer<T> &p)
{
    PkSharedPointer<T> keep = p;
    return QSharedPointer<T>(p.data(), [keep](T *) { (void)keep; });
}
}


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
