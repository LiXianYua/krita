/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoPathToolFactory.h"
#include "KoPathTool.h"
#include "KoPathShape.h"
#include <klocalizedstring.h>

#include <QAction>

KoPathToolFactory::KoPathToolFactory()
        : KoToolFactoryBase("PathTool")
{
    setToolTip(i18n("Edit Shapes Tool"));
    setSection(ToolBoxSection::Main);
    setIconName("shape_handling");
    setPriority(2);
    setActivationShapeId("flake/always,KoPathShape");
}

KoPathToolFactory::~KoPathToolFactory()
{
}

KoToolBase * KoPathToolFactory::createTool(KoCanvasBase *canvas)
{
    return new KoPathTool(canvas);
}

QList<QAction *> KoPathToolFactory::createActionsImpl()
{
    QList<QAction *> actions;

    QAction *action = new QAction(this);
    action->setObjectName("pathpoint-corner");
    actions << action;

    action = new QAction(this);
    action->setObjectName("pathpoint-smooth");
    actions << action;

    action = new QAction(this);
    action->setObjectName("pathpoint-symmetric");
    actions << action;

    action = new QAction(this);
    action->setObjectName("pathpoint-curve");
    actions << action;

    action = new QAction(this);
    action->setObjectName("pathpoint-line");
    actions << action;

    action = new QAction(this);
    action->setObjectName("pathsegment-line");
    actions << action;

    action = new QAction(this);
    action->setObjectName("pathsegment-curve");
    actions << action;

    action = new QAction(this);
    action->setObjectName("pathpoint-insert");
    actions << action;

    action = new QAction(this);
    action->setObjectName("pathpoint-remove");
    actions << action;

    action = new QAction(this);
    action->setObjectName("path-break-point");
    actions << action;

    action = new QAction(this);
    action->setObjectName("path-break-segment");
    actions << action;

    action = new QAction(this);
    action->setObjectName("path-break-selection");
    actions << action;

    action = new QAction(this);
    action->setObjectName("pathpoint-join");
    actions << action;

    action = new QAction(this);
    action->setObjectName("pathpoint-merge");
    actions << action;

    action = new QAction(this);
    action->setObjectName("convert-to-path");
    actions << action;

    return actions;
}
