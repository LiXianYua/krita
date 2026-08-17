/*
 *  SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KisSelectionToolFactoryBase.h"

#include <QAction>

KisSelectionToolFactoryBase::KisSelectionToolFactoryBase(const QString &id)
    : KisToolPaintFactoryBase(id)
{
}

KisSelectionToolFactoryBase::~KisSelectionToolFactoryBase()
{
}

QList<QAction *> KisSelectionToolFactoryBase::createActionsImpl()
{
    QList<QAction *> actions = KisToolPaintFactoryBase::createActionsImpl();

    QAction *actionAdd = new QAction(this);
    actionAdd->setObjectName("selection_tool_mode_add");
    actions << actionAdd;

    QAction *actionReplace = new QAction(this);
    actionReplace->setObjectName("selection_tool_mode_replace");
    actions << actionReplace;

    QAction *actionSubtract = new QAction(this);
    actionSubtract->setObjectName("selection_tool_mode_subtract");
    actions << actionSubtract;

    QAction *actionIntersect = new QAction(this);
    actionIntersect->setObjectName("selection_tool_mode_intersect");
    actions << actionIntersect;

    return actions;
}

KisToolPolyLineFactoryBase::KisToolPolyLineFactoryBase(const QString &id)
    : KisToolPaintFactoryBase(id)
{
}

KisToolPolyLineFactoryBase::~KisToolPolyLineFactoryBase()
{

}

QList<QAction *> KisToolPolyLineFactoryBase::createActionsImpl()
{
    QList<QAction *> actions = KisToolPaintFactoryBase::createActionsImpl();

    QAction *actionUndo = new QAction(this);
    actionUndo->setObjectName("undo_polygon_selection");
    actions << actionUndo;

    QAction *actionAdd = new QAction(this);
    actionAdd->setObjectName("selection_tool_mode_add");
    actions << actionAdd;

    return actions;
}
