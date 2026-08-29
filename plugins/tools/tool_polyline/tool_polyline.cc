/*
 * tool_polyline.cc -- Part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tool_polyline.h"
#include <stdlib.h>
#include <vector>

#include <PkPoint.h>

#include <kis_debug.h>
#include <mutex>

#include <kis_global.h>
#include <kis_types.h>
#include <KoToolRegistry.h>


#include "kis_tool_polyline.h"

void registerToolPolyline()
{
    static std::once_flag once;
    std::call_once(once, [] { KoToolRegistry::instance()->add(new KisToolPolylineFactory()); });
}
namespace { struct Registration { Registration() { registerToolPolyline(); } } registration; }
