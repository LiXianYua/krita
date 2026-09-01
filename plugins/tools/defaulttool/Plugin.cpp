/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "Plugin.h"
#include "defaulttool/DefaultToolFactory.h"
#include "referenceimagestool/ToolReferenceImages.h"

#include <KoToolRegistry.h>

#include <mutex>

void registerDefaultToolPlugin()
{
    static std::once_flag once;
    std::call_once(once, [] {
        KoToolRegistry::instance()->add(new DefaultToolFactory());
        KoToolRegistry::instance()->add(new ToolReferenceImagesFactory());
    });
}
