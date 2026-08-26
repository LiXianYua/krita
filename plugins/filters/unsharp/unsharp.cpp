/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "kis_unsharp_filter.h"

#include <filter/kis_filter_registry.h>

namespace {
struct UnsharpPluginFilterRegistration
{
    UnsharpPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisUnsharpFilter());
    }
};
} // namespace
static UnsharpPluginFilterRegistration s_unsharpPluginFilterRegistration;
