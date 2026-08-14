/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_COLOR_SAMPLER_CONFIG_H
#define KIS_COLOR_SAMPLER_CONFIG_H

#include <kritaimage_export.h>

class KRITAIMAGE_EXPORT KisColorSamplerConfig
{
public:
    KisColorSamplerConfig();

    bool toForegroundColor;
    bool updateColor;
    bool addColorToCurrentPalette;
    bool normaliseValues;
    bool sampleMerged;
    int radius;
    int blend;

    void save() const;
    void load();
};

#endif
