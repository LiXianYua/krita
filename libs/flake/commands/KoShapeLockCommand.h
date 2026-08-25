/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOSHAPELOCKCOMMAND_H
#define KOSHAPELOCKCOMMAND_H
#include <PkList.h>


#include <kundo2command.h>
#include <pk/container/PkList.h>

class KoShape;

/// The undo / redo command to lock a set of shapes position and size
class KoShapeLockCommand : public KUndo2Command
{
public:
    /**
     * Command to lock a set of shapes position and size
     * @param shapes a set of shapes that should change lock state
     * @param oldLock list of old lock states the same length as @p shapes
     * @param newLock list of new lock states the same length as @p shapes
     * @param parent the parent command used for macro commands
     */
    KoShapeLockCommand(const PkList<KoShape*> &shapes, const PkList<bool> &oldLock, const PkList<bool> &newLock,
                       KUndo2Command *parent = 0);
    ~KoShapeLockCommand() override;

    /// redo the command
    void redo() override;
    /// revert the actions done in redo
    void undo() override;

private:
    PkList<KoShape*> m_shapes;    /// the shapes to set background for
    PkList<bool> m_oldLock;       /// old lock states
    PkList<bool> m_newLock;       /// new lock states
};

#endif
