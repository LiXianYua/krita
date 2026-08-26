/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2006 Frederic Coiffier <fcoiffie@gmail.com>
 * SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include <filter/kis_filter_registry.h>

#include "KisLevelsFilter.h"

namespace {
struct KisLevelsFilterPluginFilterRegistration
{
    KisLevelsFilterPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisLevelsFilter());
    }
};
} // namespace
static KisLevelsFilterPluginFilterRegistration s_kisLevelsFilterPluginFilterRegistration;
