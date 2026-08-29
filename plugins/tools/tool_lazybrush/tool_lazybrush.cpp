/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tool_lazybrush.h"

#include <kis_debug.h>
#include <mutex>

#include <kis_tool.h>
#include <KoToolRegistry.h>

#include "kis_paint_device.h"
#include "kis_tool_lazy_brush.h"


void registerToolLazyBrush()
{
    static std::once_flag once;
    std::call_once(once, [] { KoToolRegistry::instance()->add(new KisToolLazyBrushFactory()); });
}
namespace { struct Registration { Registration() { registerToolLazyBrush(); } } registration; }
