/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisDisplayConfigUiAdapter.h"

#include "kis_config.h"

KisDisplayConfig::Options kisDisplayConfigOptionsFromKisConfig(const KisConfig &cfg)
{
    KoColorConversionTransformation::ConversionFlags conversionFlags =
        KoColorConversionTransformation::HighQuality;

    if (cfg.useBlackPointCompensation()) {
        conversionFlags |= KoColorConversionTransformation::BlackpointCompensation;
    }
    if (!cfg.allowLCMSOptimization()) {
        conversionFlags |= KoColorConversionTransformation::NoOptimization;
    }

    return {static_cast<KoColorConversionTransformation::Intent>(cfg.monitorRenderIntent()),
            conversionFlags};
}
