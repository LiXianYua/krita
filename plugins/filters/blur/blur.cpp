/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "kis_blur_filter.h"
#include "kis_gaussian_blur_filter.h"
#include "kis_motion_blur_filter.h"
#include "kis_lens_blur_filter.h"
#include "filter/kis_filter_registry.h"

namespace {
struct BlurFilterPluginFilterRegistration
{
    BlurFilterPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisBlurFilter());
        KisFilterRegistry::instance()->add(new KisGaussianBlurFilter());
        KisFilterRegistry::instance()->add(new KisMotionBlurFilter());
        KisFilterRegistry::instance()->add(new KisLensBlurFilter());
    }
};
} // namespace
static BlurFilterPluginFilterRegistration s_blurFilterPluginFilterRegistration;
