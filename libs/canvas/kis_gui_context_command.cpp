/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_gui_context_command.h"
#include "kis_gui_context_command_p.h"

#include <PkThreadCallQueue.h>


KisGuiContextCommand::KisGuiContextCommand(KUndo2Command *command, PkObject *guiObject)
    : m_command(command),
      m_delegate(new KisGuiContextCommandDelegate(nullptr))
{
    /**
     * We owe the delegate ourselves, so don't assign a parent to it,
     * but just move it to the GUI thread
     */
    m_delegate->moveToThread(guiObject->thread());
}

KisGuiContextCommand::~KisGuiContextCommand()
{
}

void KisGuiContextCommand::undo()
{
    PkThreadCallQueue::postBlocking(m_delegate->thread(), [this] {
        m_delegate->executeCommand(m_command.data(), true);
    });
}

void KisGuiContextCommand::redo()
{
    PkThreadCallQueue::postBlocking(m_delegate->thread(), [this] {
        m_delegate->executeCommand(m_command.data(), false);
    });
}
