/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "kis_minmax_filters.h"
#include "kis_color_to_alpha.h"
#include "KisFilterFastColorOverlay.h"
#include <filter/kis_filter_registry.h>

namespace {
struct KritaExtensionsColorsFilterRegistration
{
    KritaExtensionsColorsFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisFilterMax());
        KisFilterRegistry::instance()->add(new KisFilterMin());
        KisFilterRegistry::instance()->add(new KisFilterColorToAlpha());
        KisFilterRegistry::instance()->add(new KisFilterFastColorOverlay());
    }
};
} // namespace
static KritaExtensionsColorsFilterRegistration s_kritaExtensionsColorsFilterRegistration;
