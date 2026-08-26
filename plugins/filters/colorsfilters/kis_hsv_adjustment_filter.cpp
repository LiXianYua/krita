/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2022 Sam Linnfer <littlelightlittlefire@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-only
*/

#include <array>

#include "kis_hsv_adjustment_filter.h"


#include <filter/kis_filter_category_ids.h>
#include <filter/kis_color_transformation_configuration.h>
#include <kis_selection.h>
#include <kis_paint_device.h>
#include <kis_processing_information.h>
#include <KoColorSpace.h>
#include <KoColorProfile.h>
#include <KoColorSpaceRegistry.h>
#include <KoColorModelStandardIds.h>
#include <KoColorConversions.h>
#include <KisGlobalResourcesInterface.h>

#include "kis_signals_blocker.h"

namespace {

enum class SLIDER_TYPE {
    HUE,
    SATURATION,
    VALUE,
    LIGHTNESS,
    LUMA,
    INTENSITY,
    YELLOW_BLUE,
    GREEN_RED,
    LUMA_YUV,
};

// Corresponds the 5 transformations defined in kis_hsv_adjustment.cpp
// 0:HSV, 1:HSL, 2:HSI, 3:HSY, 4:YUV
enum class SLIDER_SET {
    HSV, HSL, HSI, HSY, YUV,
};

struct SliderSettings {
    SliderSettings(SLIDER_TYPE type, int min, int max, int minRelative, int maxRelative, int resetValue)
        : m_type(type)
        , m_min(min)
        , m_max(max)
        , m_minRelative(minRelative)
        , m_maxRelative(maxRelative)
        , m_resetValue(resetValue)
    {
    }

    double scale(bool colorize, double value) const {
        if (colorize) {
            return value / m_max;
        }
        return value / m_maxRelative;
    }

    double normalize(bool colorize, double value) const {
        if (colorize) {
            return (value - m_min) / (m_max - m_min);
        }
        return (value - m_minRelative) / (m_maxRelative - m_minRelative);
    }

    SLIDER_TYPE m_type;
    int m_min, m_max;
    int m_minRelative, m_maxRelative;
    int m_resetValue;
};

// Slider configuration based on their SLIDER_TYPE
const SliderSettings SLIDER_TABLE[9] = {
    SliderSettings(SLIDER_TYPE::HUE,        0,    360, -180, 180, 0),
    SliderSettings(SLIDER_TYPE::SATURATION, 0,    100, -100, 100, 0),
    SliderSettings(SLIDER_TYPE::VALUE,      -100, 100, -100, 100, 0),
    SliderSettings(SLIDER_TYPE::LIGHTNESS,  -100, 100, -100, 100, 0),
    SliderSettings(SLIDER_TYPE::LUMA,       -100, 100, -100, 100, 0),
    SliderSettings(SLIDER_TYPE::INTENSITY,  -100, 100, -100, 100, 0),
    SliderSettings(SLIDER_TYPE::YELLOW_BLUE,0,    100, -100, 100, 0),
    SliderSettings(SLIDER_TYPE::GREEN_RED,  0,    100, -100, 100, 0),
    SliderSettings(SLIDER_TYPE::LUMA_YUV,   -100, 100, -100, 100, 0),
};

// Defines which sliders to display in each set.
// One for each SLIDER_SET.
const std::array<SLIDER_TYPE, 3> SLIDER_SETS[5] = {
    { SLIDER_TYPE::HUE,         SLIDER_TYPE::SATURATION, SLIDER_TYPE::VALUE     },
    { SLIDER_TYPE::HUE,         SLIDER_TYPE::SATURATION, SLIDER_TYPE::LIGHTNESS },
    { SLIDER_TYPE::HUE,         SLIDER_TYPE::SATURATION, SLIDER_TYPE::INTENSITY },
    { SLIDER_TYPE::HUE,         SLIDER_TYPE::SATURATION, SLIDER_TYPE::LUMA      },
    { SLIDER_TYPE::YELLOW_BLUE, SLIDER_TYPE::GREEN_RED,  SLIDER_TYPE::LUMA_YUV  },
};

SliderSettings sliderSetting(SLIDER_TYPE type) {
    return SLIDER_TABLE[static_cast<int>(type)];
}

}

KisHSVAdjustmentFilter::KisHSVAdjustmentFilter()
        : KisColorTransformationFilter(id(), FiltersCategoryAdjustId, PkString("&HSV Adjustment..."))
{
    setSupportsPainting(true);
}

KoColorTransformation *KisHSVAdjustmentFilter::createTransformation(const KoColorSpace *cs, const KisFilterConfigurationSP config) const
{
    PkHash<PkString, PkVariant> params;
    if (config) {
        int type = config->getInt("type", 1);
        bool colorize = config->getBool("colorize", false);
        bool compatibilityMode = config->getBool("compatibilityMode", true);

        const std::array<SLIDER_TYPE, 3> sliderSet = SLIDER_SETS[static_cast<int>(type)];

        params["h"] = sliderSetting(sliderSet[0]).scale(colorize, config->getInt("h", 0));
        params["s"] = sliderSetting(sliderSet[1]).scale(colorize, config->getInt("s", 0));
        params["v"] = sliderSetting(sliderSet[2]).scale(colorize, config->getInt("v", 0));

        params["type"] = type;
        params["colorize"] = colorize;
        params["lumaRed"] = cs->lumaCoefficients()[0];
        params["lumaGreen"] = cs->lumaCoefficients()[1];
        params["lumaBlue"] = cs->lumaCoefficients()[2];
        params["compatibilityMode"] = compatibilityMode;
    }
    return cs->createColorTransformation("hsv_adjustment", params);
}

KisFilterConfigurationSP KisHSVAdjustmentFilter::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    config->setProperty("h", 0);
    config->setProperty("s", 0);
    config->setProperty("v", 0);
    config->setProperty("type", 1);
    config->setProperty("colorize", false);
    config->setProperty("compatibilityMode", false);
    return config;
}

