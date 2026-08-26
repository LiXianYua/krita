/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <stdlib.h>
#include <vector>

#include <PkPoint.h>


#include <kis_debug.h>

#include <kis_image.h>
#include <kis_layer.h>
#include <filter/kis_filter_registry.h>
#include <kis_global.h>
#include <kis_types.h>
#include "kis_simple_noise_reducer.h"
#include "kis_wavelet_noise_reduction.h"

namespace {
struct KritaImageEnhancementFilterRegistration
{
    KritaImageEnhancementFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisSimpleNoiseReducer());
        KisFilterRegistry::instance()->add(new KisWaveletNoiseReduction());
    }
};
} // namespace
static KritaImageEnhancementFilterRegistration s_kritaImageEnhancementFilterRegistration;
