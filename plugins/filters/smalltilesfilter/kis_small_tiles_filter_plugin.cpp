/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2005 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "kis_small_tiles_filter.h"
#include "kis_global.h"
#include "filter/kis_filter_registry.h"

namespace {
struct KisSmallTilesFilterPluginFilterRegistration
{
    KisSmallTilesFilterPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisSmallTilesFilter());
    }
};
} // namespace
static KisSmallTilesFilterPluginFilterRegistration s_kisSmallTilesFilterPluginFilterRegistration;
