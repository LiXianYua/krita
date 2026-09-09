/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <PkConfigGroup.h>
#include <PkContainerAlgo.h>
#include <PkSharedConfig.h>
#include <PkStringList.h>
#include "KoToolRegistry.h"

#include "tools/KoPathToolFactory.h"
#include "tools/KoZoomToolFactory.h"

KoToolRegistry::KoToolRegistry()
  : d(0)
{
}

void KoToolRegistry::init()
{
    // S-08: 插件加载已随 D-12 删除，只保留硬编码 factory。

    // register generic tools
    KoToolFactoryBase *pathToolFactory = new KoPathToolFactory();
    add(pathToolFactory->id(), pathToolFactory);
    KoToolFactoryBase *zoomToolFactory = new KoZoomToolFactory();
    add(zoomToolFactory->id(), zoomToolFactory);

    PkConfigGroup cfg = PkSharedConfig::openConfig()->group("krita");
    const PkStringList toolsBlacklist = cfg.readEntry("ToolsBlacklist", PkStringList());
    for (const PkString &toolID : toolsBlacklist) {
        delete value(toolID);
        remove(toolID);
    }
}

KoToolRegistry::~KoToolRegistry()
{
    pkDeleteAll(doubleEntries());
    pkDeleteAll(values());
}

KoToolRegistry* KoToolRegistry::instance()
{
    static KoToolRegistry registry;
    static const bool initialized = (registry.init(), true);
    (void)initialized;
    return &registry;
}
