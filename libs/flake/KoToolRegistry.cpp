/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoToolRegistry.h"

#include <FlakeDebug.h>
#include <kconfiggroup.h>
#include <ksharedconfig.h>

#include "tools/KoPathToolFactory.h"
#include "tools/KoZoomTool.h"
#include "tools/KoZoomToolFactory.h"
#include "KoToolManager.h"

#include <QGlobalStatic>

Q_GLOBAL_STATIC(KoToolRegistry, s_instance)

KoToolRegistry::KoToolRegistry()
  : d(0)
{
}

void KoToolRegistry::init()
{
    // S-08: 插件加载已随 D-18 删除，只保留硬编码 factory。

    // register generic tools
    KoToolFactoryBase *pathToolFactory = new KoPathToolFactory();
    add(toPkString(pathToolFactory->id()), pathToolFactory);
    KoToolFactoryBase *zoomToolFactory = new KoZoomToolFactory();
    add(toPkString(zoomToolFactory->id()), zoomToolFactory);

    KConfigGroup cfg =  KSharedConfig::openConfig()->group("krita");
    QStringList toolsBlacklist = cfg.readEntry("ToolsBlacklist", QStringList());
    foreach (const QString& toolID, toolsBlacklist) {
        delete value(toPkString(toolID));
        remove(toPkString(toolID));
    }
}

KoToolRegistry::~KoToolRegistry()
{
    qDeleteAll(doubleEntries());
    qDeleteAll(values());
}

KoToolRegistry* KoToolRegistry::instance()
{
    if (!s_instance.exists()) {
        s_instance->init();
    }
    return s_instance;
}
