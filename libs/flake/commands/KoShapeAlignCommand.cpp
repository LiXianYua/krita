/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

#include "KoShapeAlignCommand.h"
#include "KoShape.h"
#include "KoShapeGroup.h"
#include "commands/KoShapeMoveCommand.h"
// #include <FlakeDebug.h>

class Q_DECL_HIDDEN KoShapeAlignCommand::Private
{
public:
    Private() : command(0) {}
    ~Private() {
        delete command;
    }
    KoShapeMoveCommand *command;
};

KoShapeAlignCommand::KoShapeAlignCommand(const PkList<KoShape*> &shapes, Align align, const PkRectF &boundingRect, KUndo2Command *parent)
        : KUndo2Command(parent),
        d(new Private())
{
    PkList<PkPointF> previousPositions;
    PkList<PkPointF> newPositions;
    PkPointF position;
    PkPointF delta;
    PkRectF bRect;
    for (KoShape *shape : shapes) {
//   if (dynamic_cast<KoShapeGroup*> (shape))
//       debugFlake <<"Found Group";
//   else if (dynamic_cast<KoShapeContainer*> (shape))
//       debugFlake <<"Found Container";
//   else
//       debugFlake <<"Found shape";
        position = toPkPointF(shape->absolutePosition());
        previousPositions  << position;
        bRect = toPkRectF(shape->absoluteOutlineRect());
        switch (align) {
        case HorizontalLeftAlignment:
            delta = PkPointF(boundingRect.left(), bRect.y()) - bRect.topLeft();
            break;
        case HorizontalCenterAlignment:
            delta = PkPointF(boundingRect.center().x() - bRect.width() / 2, bRect.y()) - bRect.topLeft();
            break;
        case HorizontalRightAlignment:
            delta = PkPointF(boundingRect.right() - bRect.width(), bRect.y()) - bRect.topLeft();
            break;
        case VerticalTopAlignment:
            delta = PkPointF(bRect.x(), boundingRect.top()) - bRect.topLeft();
            break;
        case VerticalCenterAlignment:
            delta = PkPointF(bRect.x(), boundingRect.center().y() - bRect.height() / 2) - bRect.topLeft();
            break;
        case VerticalBottomAlignment:
            delta = PkPointF(bRect.x(), boundingRect.bottom() - bRect.height()) - bRect.topLeft();
            break;
        };
        newPositions  << position + delta;
//debugFlake <<"-> moving" <<  position.x() <<"," << position.y() <<" to" <<
//        (position + delta).x() << ", " << (position+delta).y() << Qt::endl;
    }
    d->command = new KoShapeMoveCommand(shapes, previousPositions, newPositions);

    setText(kundo2_text("Align shapes"));
}

KoShapeAlignCommand::~KoShapeAlignCommand()
{
    delete d;
}

void KoShapeAlignCommand::redo()
{
    KUndo2Command::redo();
    d->command->redo();
}

void KoShapeAlignCommand::undo()
{
    KUndo2Command::undo();
    d->command->undo();
}
