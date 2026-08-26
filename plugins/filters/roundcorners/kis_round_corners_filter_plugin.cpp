/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2005 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "kis_round_corners_filter.h"
#include "kis_global.h"
#include "filter/kis_filter_registry.h"

namespace {
struct KisRoundCornersFilterPluginFilterRegistration
{
    KisRoundCornersFilterPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisRoundCornersFilter());
    }
};
} // namespace
static KisRoundCornersFilterPluginFilterRegistration s_kisRoundCornersFilterPluginFilterRegistration;
