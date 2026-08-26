/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include <filter/kis_filter_registry.h>


#include "kis_raindrops_filter.h"

namespace {
struct KisRainDropsFilterPluginFilterRegistration
{
    KisRainDropsFilterPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisRainDropsFilter());
    }
};
} // namespace
static KisRainDropsFilterPluginFilterRegistration s_kisRainDropsFilterPluginFilterRegistration;
