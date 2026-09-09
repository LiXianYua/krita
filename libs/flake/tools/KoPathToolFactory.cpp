/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoPathToolFactory.h"
#include <PkFlakeBridge.h>
#include "KoPathTool.h"
#include "KoPathShape.h"
#include <klocalizedstring.h>

#include <QAction>

KoPathToolFactory::KoPathToolFactory()
        : KoToolFactoryBase("PathTool")
{
    setToolTip(toPkString(i18n("Edit Shapes Tool")));
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

PkList<QAction *> KoPathToolFactory::createActionsImpl()
{
    PkList<QAction *> actions;

    QAction *action = createHostAction("", "pathpoint-corner");
    actions << action;

    action = createHostAction("", "pathpoint-smooth");
    actions << action;

    action = createHostAction("", "pathpoint-symmetric");
    actions << action;

    action = createHostAction("", "pathpoint-curve");
    actions << action;

    action = createHostAction("", "pathpoint-line");
    actions << action;

    action = createHostAction("", "pathsegment-line");
    actions << action;

    action = createHostAction("", "pathsegment-curve");
    actions << action;

    action = createHostAction("", "pathpoint-insert");
    actions << action;

    action = createHostAction("", "pathpoint-remove");
    actions << action;

    action = createHostAction("", "path-break-point");
    actions << action;

    action = createHostAction("", "path-break-segment");
    actions << action;

    action = createHostAction("", "path-break-selection");
    actions << action;

    action = createHostAction("", "pathpoint-join");
    actions << action;

    action = createHostAction("", "pathpoint-merge");
    actions << action;

    action = createHostAction("", "convert-to-path");
    actions << action;

    return actions;
}
