/*
 * tool_polygon.cc -- Part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "tool_polygon.h"

#include <stdlib.h>
#include <vector>

#include <PkPoint.h>

#include <kis_debug.h>
#include <kis_paint_device.h>
#include <mutex>

#include <kis_global.h>
#include <kis_types.h>
#include <KoToolRegistry.h>

#include "kis_tool_polygon.h"

void registerToolPolygon()
{
    static std::once_flag once;
    std::call_once(once, [] { KoToolRegistry::instance()->add(new KisToolPolygonFactory()); });
}
namespace { struct Registration { Registration() { registerToolPolygon(); } } registration; }
