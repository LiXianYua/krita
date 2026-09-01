/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KarbonCalligraphyToolFactory.h"
#include "KarbonCalligraphyTool.h"

#include <KoToolRegistry.h>

KarbonCalligraphyToolFactory::KarbonCalligraphyToolFactory()
    : KoToolFactoryBase("KarbonCalligraphyTool")
{
    setToolTip(PkString("Calligraphy"));
    setSection(ToolBoxSection::Main);
    setPriority(6);
    setActivationShapeId("flake/edit");
}

KarbonCalligraphyToolFactory::~KarbonCalligraphyToolFactory()
{
}

KoToolBase *KarbonCalligraphyToolFactory::createTool(KoCanvasBase *canvas)
{
    return new KarbonCalligraphyTool(canvas);
}
