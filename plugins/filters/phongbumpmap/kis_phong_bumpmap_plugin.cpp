/*
 *  SPDX-FileCopyrightText: 2010-2011 José Luis Vergara <pentalis@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_phong_bumpmap_filter.h"

#include <filter/kis_filter_registry.h>

namespace {
struct KisPhongBumpmapPluginFilterRegistration
{
    KisPhongBumpmapPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisFilterPhongBumpmap());
    }
};
} // namespace
static KisPhongBumpmapPluginFilterRegistration s_kisPhongBumpmapPluginFilterRegistration;
