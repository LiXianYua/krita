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

PkList<KisHostActionSpec> KoPathToolFactory::createActionsImpl()
{
    PkList<KisHostActionSpec> actions;

    actions << createHostAction("", "pathpoint-corner");
    actions << createHostAction("", "pathpoint-smooth");
    actions << createHostAction("", "pathpoint-symmetric");
    actions << createHostAction("", "pathpoint-curve");
    actions << createHostAction("", "pathpoint-line");
    actions << createHostAction("", "pathsegment-line");
    actions << createHostAction("", "pathsegment-curve");
    actions << createHostAction("", "pathpoint-insert");
    actions << createHostAction("", "pathpoint-remove");
    actions << createHostAction("", "path-break-point");
    actions << createHostAction("", "path-break-segment");
    actions << createHostAction("", "path-break-selection");
    actions << createHostAction("", "pathpoint-join");
    actions << createHostAction("", "pathpoint-merge");
    actions << createHostAction("", "convert-to-path");

    return actions;
}
