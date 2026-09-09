/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KarbonToolsPlugin.h"
#include "KarbonToolsResources.h"
#include "CalligraphyTool/KarbonCalligraphyToolFactory.h"
#include "CalligraphyTool/KarbonCalligraphicShapeFactory.h"

#include <KoToolRegistry.h>
#include <KoShapeRegistry.h>

#include <mutex>

void registerKarbonTools()
{
    karbonToolsResourceAnchor();

    static std::once_flag once;
    std::call_once(once, [] {
        KoToolRegistry::instance()->add(new KarbonCalligraphyToolFactory());
        KoShapeRegistry::instance()->add(new KarbonCalligraphicShapeFactory());
    });
}
