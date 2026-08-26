/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2004 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "kis_oilpaint_filter.h"
#include "kis_global.h"
#include "filter/kis_filter_registry.h"

namespace {
struct KisOilPaintFilterPluginFilterRegistration
{
    KisOilPaintFilterPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisOilPaintFilter());
    }
};
} // namespace
static KisOilPaintFilterPluginFilterRegistration s_kisOilPaintFilterPluginFilterRegistration;
