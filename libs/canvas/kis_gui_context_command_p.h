/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_GUI_CONTEXT_COMMAND_P_H
#define __KIS_GUI_CONTEXT_COMMAND_P_H

#include <PkObject.h>
class KUndo2Command;

class KisGuiContextCommandDelegate : public PkObject
{
public:
    KisGuiContextCommandDelegate(PkObject *parent = nullptr);

    void executeCommand(KUndo2Command *command, bool undo);
};


#endif /* __KIS_GUI_CONTEXT_COMMAND_P_H */
