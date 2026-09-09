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

PkList<QAction *> KisSelectionToolFactoryBase::createActionsImpl()
{
    PkList<QAction *> actions = KisToolPaintFactoryBase::createActionsImpl();

    QAction *actionAdd = createHostAction("", "selection_tool_mode_add");
    actions << actionAdd;

    QAction *actionReplace = createHostAction("", "selection_tool_mode_replace");
    actions << actionReplace;

    QAction *actionSubtract = createHostAction("", "selection_tool_mode_subtract");
    actions << actionSubtract;

    QAction *actionIntersect = createHostAction("", "selection_tool_mode_intersect");
    actions << actionIntersect;

    return actions;
}

KisToolPolyLineFactoryBase::KisToolPolyLineFactoryBase(const PkString &id)
    : KisToolPaintFactoryBase(id)
{
}

KisToolPolyLineFactoryBase::~KisToolPolyLineFactoryBase()
{

}

PkList<QAction *> KisToolPolyLineFactoryBase::createActionsImpl()
{
    PkList<QAction *> actions = KisToolPaintFactoryBase::createActionsImpl();

    QAction *actionUndo = createHostAction("", "undo_polygon_selection");
    actions << actionUndo;

    QAction *actionAdd = createHostAction("", "selection_tool_mode_add");
    actions << actionAdd;

    return actions;
}
