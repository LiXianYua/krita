/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "SvgTextToolFactory.h"

#include "KoSvgTextShape.h"
#include "SvgTextTool.h"
#include "SvgTextShortCuts.h"

#include <PkFlakeBridge.h>

#include <QAction>
#include <klocalizedstring.h>

SvgTextToolFactory::SvgTextToolFactory()
    : KoToolFactoryBase("SvgTextTool")
{
    setToolTip(toPkString(i18n("SVG Text Tool")));
    setSection(ToolBoxSection::Main);
    setPriority(1);
    setActivationShapeId(PkString("flake/always,%1").arg(KoSvgTextShape_SHAPEID));
}

SvgTextToolFactory::~SvgTextToolFactory()
{
}

KoToolBase *SvgTextToolFactory::createTool(KoCanvasBase *canvas)
{
    return new SvgTextTool(canvas);
}

PkList<QAction *> SvgTextToolFactory::createActionsImpl()
{
    PkList<QAction *> actions;
    for (const PkString &name : SvgTextShortCuts::possibleActions()) {
        { QAction *action = new QAction(this); action->setObjectName(toQString(name)); actions << action; }
    }
    { QAction *action = new QAction(this); action->setObjectName("svg_paste_rich_text"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("svg_paste_plain_text"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("text_type_preformatted"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("text_type_pre_positioned"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("text_type_inline_wrap"); actions << action; }

    { QAction *action = new QAction(this); action->setObjectName("svg_type_setting_move_selection_start_down_1_px"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("svg_type_setting_move_selection_start_up_1_px"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("svg_type_setting_move_selection_start_left_1_px"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("svg_type_setting_move_selection_start_right_1_px"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("svg_remove_transforms_from_range"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("svg_clear_formatting"); actions << action; }
    return actions;
}
