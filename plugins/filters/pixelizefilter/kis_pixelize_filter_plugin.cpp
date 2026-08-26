/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2005 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include <kis_paint_device.h>
#include <kis_global.h>
#include <filter/kis_filter_registry.h>

#include "kis_pixelize_filter.h"

namespace {
struct KisPixelizeFilterPluginFilterRegistration
{
    KisPixelizeFilterPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisPixelizeFilter());
    }
};
} // namespace
static KisPixelizeFilterPluginFilterRegistration s_kisPixelizeFilterPluginFilterRegistration;
