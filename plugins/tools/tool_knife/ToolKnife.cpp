/*
 *  SPDX-FileCopyrightText: 2025 Agata Cacko
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ToolKnife.h"

#include <KoToolRegistry.h>
#include "KisToolKnife.h"
#include <mutex>

void registerToolKnife()
{
    static std::once_flag once;
    std::call_once(once, [] { KoToolRegistry::instance()->add(new KisToolKnifeFactory()); });
}
