/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_GUI_CONTEXT_COMMAND_H
#define __KIS_GUI_CONTEXT_COMMAND_H

#include <PkObject.h>
#include <PkScopedPointer.h>
#include "kundo2command.h"
#include <kritacanvas_export.h>

class KisGuiContextCommandDelegate;

/**
 * KisGuiContextCommand is a special command-wrapper which ensures
 * that the holding command is executed in the GUI thread only. Please
 * note that any activity done by the containing command must *not*
 * lead to the blocking on the image, otherwise you'll get a deadlock!
 */
class KRITACANVAS_EXPORT KisGuiContextCommand : public PkObject, public KUndo2Command
{
public:
    KisGuiContextCommand(KUndo2Command *command, PkObject *guiObject);
    ~KisGuiContextCommand() override;

    void undo() override;
    void redo() override;

private:
    PkScopedPointer<KUndo2Command> m_command;
    PkScopedPointer<KisGuiContextCommandDelegate> m_delegate;
};

#endif /* __KIS_GUI_CONTEXT_COMMAND_H */
