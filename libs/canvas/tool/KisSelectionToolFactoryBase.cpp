/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include <pk/container/PkList.h>
#include "KisSelectionToolFactoryBase.h"

KisSelectionToolFactoryBase::KisSelectionToolFactoryBase(const PkString &id)
    : KisToolPaintFactoryBase(id)
{
}

KisSelectionToolFactoryBase::~KisSelectionToolFactoryBase()
{
}

PkList<KisHostActionSpec> KisSelectionToolFactoryBase::createActionsImpl()
{
    PkList<KisHostActionSpec> actions = KisToolPaintFactoryBase::createActionsImpl();

    actions << createHostAction("", "selection_tool_mode_add");
    actions << createHostAction("", "selection_tool_mode_replace");
    actions << createHostAction("", "selection_tool_mode_subtract");
    actions << createHostAction("", "selection_tool_mode_intersect");

    return actions;
}

KisToolPolyLineFactoryBase::KisToolPolyLineFactoryBase(const PkString &id)
    : KisToolPaintFactoryBase(id)
{
}

KisToolPolyLineFactoryBase::~KisToolPolyLineFactoryBase()
{

}

PkList<KisHostActionSpec> KisToolPolyLineFactoryBase::createActionsImpl()
{
    PkList<KisHostActionSpec> actions = KisToolPaintFactoryBase::createActionsImpl();

    actions << createHostAction("", "undo_polygon_selection");
    actions << createHostAction("", "selection_tool_mode_add");

    return actions;
}
