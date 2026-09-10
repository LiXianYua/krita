/* This file is part of the KDE project

   SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "SvgTextToolFactory.h"
#include "SvgTextToolResources.h"

#include "KoSvgTextShape.h"
#include "SvgTextTool.h"
#include "SvgTextShortCuts.h"

#include <KoToolRegistry.h>
#include <PkFlakeBridge.h>

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

PkList<KisHostActionSpec> SvgTextToolFactory::createActionsImpl()
{
    PkList<KisHostActionSpec> actions;
    for (const PkString &name : SvgTextShortCuts::possibleActions()) {
        actions << createHostAction("", name);
    }
    actions << createHostAction("", "svg_paste_rich_text");
    actions << createHostAction("", "svg_paste_plain_text");
    actions << createHostAction("", "text_type_preformatted");
    actions << createHostAction("", "text_type_pre_positioned");
    actions << createHostAction("", "text_type_inline_wrap");

    actions << createHostAction("", "svg_type_setting_move_selection_start_down_1_px");
    actions << createHostAction("", "svg_type_setting_move_selection_start_up_1_px");
    actions << createHostAction("", "svg_type_setting_move_selection_start_left_1_px");
    actions << createHostAction("", "svg_type_setting_move_selection_start_right_1_px");
    actions << createHostAction("", "svg_remove_transforms_from_range");
    actions << createHostAction("", "svg_clear_formatting");
    return actions;
}

void registerSvgTextToolFactory()
{
    static bool registered = false;
    if (registered) {
        return;
    }
    registered = true;
    KoToolRegistry::instance()->add(new SvgTextToolFactory());
}
