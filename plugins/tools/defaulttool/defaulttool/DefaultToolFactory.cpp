/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "DefaultToolFactory.h"
#include "DefaultTool.h"
#include "DefaultToolPlatform.h"



DefaultToolFactory::DefaultToolFactory()
    : KoToolFactoryBase(KoInteractionTool_ID)
{
    setToolTip(PkString("Select Shapes Tool"));
    setSection(ToolBoxSection::Main);
    setPriority(0);
    setActivationShapeId("flake/always");
}

DefaultToolFactory::DefaultToolFactory(const PkString &id)
    : KoToolFactoryBase(id)
{
}

DefaultToolFactory::~DefaultToolFactory()
{
}

KoToolBase *DefaultToolFactory::createTool(KoCanvasBase *canvas)
{
    return new DefaultTool(canvas, true);
}

PkList<DefaultToolAction *> DefaultToolFactory::createActionsImpl()
{
    PkList<DefaultToolAction *> actions;
    for (const auto &descriptor : defaultToolActionDescriptors()) {
        if (!descriptor.availableInDefaultTool) continue;
        auto *action = new DefaultToolAction();
        action->setObjectName(descriptor.actionId);
        actions << action;
    }
    return actions;
}
