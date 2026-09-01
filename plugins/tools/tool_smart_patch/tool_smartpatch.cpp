/*
 *  SPDX-FileCopyrightText: 2017 Eugene Ingerman
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tool_smartpatch.h"

#include <KoToolRegistry.h>
#include "kis_tool_smart_patch.h"
#include <mutex>

void registerToolSmartPatch()
{
    static std::once_flag once;
    std::call_once(once, [] { KoToolRegistry::instance()->add(new KisToolSmartPatchFactory()); });
}
