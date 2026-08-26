/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2016 Spencer Brown <sbrown655@gmail.com>
 * SPDX-FileCopyrightText: 2020 Deif Lou <ginoba@gmail.com>
 * 
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <kis_filter_registry.h>

#include "KisGradientMapFilter.h"

namespace {
struct KisGradientMapFilterPluginFilterRegistration
{
    KisGradientMapFilterPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisGradientMapFilter());
    }
};
} // namespace
static KisGradientMapFilterPluginFilterRegistration s_kisGradientMapFilterPluginFilterRegistration;
