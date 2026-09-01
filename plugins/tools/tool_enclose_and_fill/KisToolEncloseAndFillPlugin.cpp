/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2022 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KoToolRegistry.h>
#include <mutex>

#include "KisToolEncloseAndFillPlugin.h"
#include "KisToolEncloseAndFillFactory.h"

void registerToolEncloseAndFill()
{
    static std::once_flag once;
    std::call_once(once, [] {
        KoToolRegistry::instance()->add(new KisToolEncloseAndFillFactory());
    });
}
