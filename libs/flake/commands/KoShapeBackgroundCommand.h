/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006, 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOSHAPEBACKGROUNDCOMMAND_H
#define KOSHAPEBACKGROUNDCOMMAND_H
#include <PkList.h>
#include <PkSharedPointer.h>


#include "kritaflake_export.h"

#include <kundo2command.h>
#include <pk/container/PkList.h>
#include <pk/pointer/PkSharedPointer.h>

class KoShape;
class KoShapeBackground;

/// The undo / redo command for setting the shape background
class KRITAFLAKE_EXPORT KoShapeBackgroundCommand : public KUndo2Command
{
public:
    /**
     * Command to set a new shape background.
     * @param shapes a set of all the shapes that should get the new background.
     * @param fill the new shape background
     * @param parent the parent command used for macro commands
     */
    KoShapeBackgroundCommand(const PkList<KoShape*> &shapes, PkSharedPointer<KoShapeBackground> fill, KUndo2Command *parent = 0);

    /**
    * Command to set a new shape background.
    * @param shape a single shape that should get the new background.
    * @param fill the new shape background
    * @param parent the parent command used for macro commands
    */
    KoShapeBackgroundCommand(KoShape *shape, PkSharedPointer<KoShapeBackground> fill, KUndo2Command *parent = 0);

    /**
     * Command to set new shape backgrounds.
     * @param shapes a set of all the shapes that should get a new background.
     * @param fills the new backgrounds, one for each shape
     * @param parent the parent command used for macro commands
     */
    KoShapeBackgroundCommand(const PkList<KoShape*> &shapes, const PkList<PkSharedPointer<KoShapeBackground> > &fills, KUndo2Command *parent = 0);

    ~KoShapeBackgroundCommand() override;
    /// redo the command
    void redo() override;
    /// revert the actions done in redo
    void undo() override;

    int id() const override;
    bool mergeWith(const KUndo2Command *command) override;

private:
    class Private;
    Private * const d;
};

#endif
