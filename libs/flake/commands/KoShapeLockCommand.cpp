/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>

#include "KoShapeLockCommand.h"
#include "KoShape.h"
KoShapeLockCommand::KoShapeLockCommand(const PkList<KoShape*> &shapes, const PkList<bool> &oldLock, const PkList<bool> &newLock, KUndo2Command *parent)
        : KUndo2Command(parent)
        , m_shapes(shapes)
        , m_oldLock(oldLock)
        , m_newLock(newLock)
{
    Q_ASSERT(m_shapes.count() == m_oldLock.count());
    Q_ASSERT(m_shapes.count() == m_newLock.count());

    setText(kundo2_text("Lock shapes"));
}

KoShapeLockCommand::~KoShapeLockCommand()
{
}

void KoShapeLockCommand::redo()
{
    KUndo2Command::redo();
    for (int i = 0; i < m_shapes.count(); ++i) {
        m_shapes[i]->setGeometryProtected(m_newLock.at(i));
    }
}

void KoShapeLockCommand::undo()
{
    KUndo2Command::undo();
    for (int i = 0; i < m_shapes.count(); ++i) {
        m_shapes[i]->setGeometryProtected(m_oldLock.at(i));
    }
}
