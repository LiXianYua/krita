/*
 *  SPDX-FileCopyrightText: 2009 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.0-or-later
 */


#include <filter/kis_filter_registry.h>

#include "DodgeBurn.h"

namespace {
struct DodgeBurnPluginFilterRegistration
{
    DodgeBurnPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisFilterDodgeBurn("dodge", "Dodge", PkString("Dodge...")));
        KisFilterRegistry::instance()->add(new KisFilterDodgeBurn("burn", "Burn", PkString("Burn...")));
    }
};
} // namespace
static DodgeBurnPluginFilterRegistration s_dodgeBurnPluginFilterRegistration;
