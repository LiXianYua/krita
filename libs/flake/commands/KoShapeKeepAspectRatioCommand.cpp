/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Peter Simonsson <peter.simonsson@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkXmlCompat.h>

#include "KoShapeKeepAspectRatioCommand.h"
#include <KoShape.h>

KoShapeKeepAspectRatioCommand::KoShapeKeepAspectRatioCommand(const PkList<KoShape *> &shapes, bool newKeepAspectRatio, KUndo2Command *parent)
    : KUndo2Command(kundo2_text("Keep Aspect Ratio"), parent)
    , m_shapes(shapes)
{
    for (KoShape *shape : shapes) {
            m_oldKeepAspectRatio << shape->keepAspectRatio();
            m_newKeepAspectRatio << newKeepAspectRatio;
    }
}

KoShapeKeepAspectRatioCommand::~KoShapeKeepAspectRatioCommand()
{
}

void KoShapeKeepAspectRatioCommand::redo()
{
    KUndo2Command::redo();
    for (int i = 0; i < m_shapes.count(); ++i) {
        m_shapes[i]->setKeepAspectRatio(m_newKeepAspectRatio.at(i));
    }
}

void KoShapeKeepAspectRatioCommand::undo()
{
    KUndo2Command::undo();
    for (int i = 0; i < m_shapes.count(); ++i) {
        m_shapes[i]->setKeepAspectRatio(m_oldKeepAspectRatio.at(i));
    }
}
