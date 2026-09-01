/*
 * tool_crop.cc -- Part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Boudewijn Rempt (boud@valdyas.org)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tool_crop.h"
#include "kis_tool_crop.h"

#include <KoToolRegistry.h>
#include <mutex>

void registerToolCrop()
{
    static std::once_flag once;
    std::call_once(once, [] { KoToolRegistry::instance()->add(new KisToolCropFactory()); });
}
