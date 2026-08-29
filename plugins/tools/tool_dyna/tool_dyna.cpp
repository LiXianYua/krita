/*
 * tool_dyna.cpp -- Part of Krita
 *
 * SPDX-FileCopyrightText: 2009 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tool_dyna.h"

#include <stdlib.h>
#include <vector>

#include <PkPoint.h>

#include <kis_debug.h>
#include <kis_paint_device.h>
#include <mutex>

#include <kis_global.h>
#include <kis_types.h>
#include <KoToolRegistry.h>


#include "kis_tool_dyna.h"

void registerToolDyna()
{
    static std::once_flag once;
    std::call_once(once, [] { KoToolRegistry::instance()->add(new KisToolDynaFactory()); });
}
namespace { struct Registration { Registration() { registerToolDyna(); } } registration; }
