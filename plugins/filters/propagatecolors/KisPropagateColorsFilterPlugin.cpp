/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2024 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */


#include <filter/kis_filter_registry.h>

#include "KisPropagateColorsFilter.h"

namespace {
struct KisPropagateColorsFilterPluginFilterRegistration
{
    KisPropagateColorsFilterPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisPropagateColorsFilter());
    }
};
} // namespace
static KisPropagateColorsFilterPluginFilterRegistration s_kisPropagateColorsFilterPluginFilterRegistration;
