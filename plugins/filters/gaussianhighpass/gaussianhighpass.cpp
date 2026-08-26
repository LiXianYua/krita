/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2019 Miguel Lopez <reptillia39@live.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "gaussianhighpass_filter.h"

#include <filter/kis_filter_registry.h>

namespace {
struct GaussianHighPassPluginFilterRegistration
{
    GaussianHighPassPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisGaussianHighPassFilter());
    }
};
} // namespace
static GaussianHighPassPluginFilterRegistration s_gaussianHighPassPluginFilterRegistration;
