/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOSHAPESIZECOMMAND_H
#define KOSHAPESIZECOMMAND_H

#include <PkXmlCompat.h>

#include "kritaflake_export.h"

#include <kundo2command.h>
#include <pk/container/PkList.h>

class KoShape;

/// The undo / redo command for shape sizing.
class KRITAFLAKE_EXPORT KoShapeSizeCommand : public KUndo2Command
{
public:
    /**
     * The undo / redo command for shape sizing.
     * @param shapes all the shapes that will be resized at the same time
     * @param previousSizes the old sizes; in a list with a member for each shape
     * @param newSizes the new sizes; in a list with a member for each shape
     * @param parent the parent command used for macro commands
     */
    KoShapeSizeCommand(const PkList<KoShape*> &shapes, const PkList<PkSizeF> &previousSizes,
            const PkList<PkSizeF> &newSizes, KUndo2Command *parent = 0);
    ~KoShapeSizeCommand() override;

    /// redo the command
    void redo() override;
    /// revert the actions done in redo
    void undo() override;

private:
    class Private;
    Private * const d;
};

#endif
