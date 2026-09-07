/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/
#ifndef CHANGETEXTNGDATACOMMAND_H
#define CHANGETEXTNGDATACOMMAND_H

#include <kundo2command.h>

#include "KoSvgTextShape.h"

class SvgTextChangeCommand : public KUndo2Command
{
public:
    SvgTextChangeCommand(KoSvgTextShape *shape,
                         const PkString &svg,
                         const PkString &defs,
                         KUndo2Command *parent = 0);
    virtual ~SvgTextChangeCommand();

    /// redo the command
    void redo() override;
    /// revert the actions done in redo
    void undo() override;

private:
    KoSvgTextShape *m_shape;
    PkString m_svg;
    PkString m_defs;
    PkString m_oldSvg;
    PkString m_oldDefs;
};

#endif /* CHANGETEXTNGDATACOMMAND_H */
