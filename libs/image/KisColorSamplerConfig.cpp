/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisColorSamplerConfig.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include "kis_properties_configuration.h"

namespace {
const QString configGroupName = QStringLiteral("tool_color_sampler");
}

KisColorSamplerConfig::KisColorSamplerConfig()
    : toForegroundColor(true)
    , updateColor(true)
    , addColorToCurrentPalette(false)
    , normaliseValues(false)
    , sampleMerged(true)
    , radius(1)
    , blend(100)
{
}

void KisColorSamplerConfig::save() const
{
    KisPropertiesConfiguration props;
    props.setProperty("toForegroundColor", toForegroundColor);
    props.setProperty("updateColor", updateColor);
    props.setProperty("addPalette", addColorToCurrentPalette);
    props.setProperty("normaliseValues", normaliseValues);
    props.setProperty("sampleMerged", sampleMerged);
    props.setProperty("radius", radius);
    props.setProperty("blend", blend);

    KConfigGroup config = KSharedConfig::openConfig()->group(configGroupName);
    config.writeEntry("ColorSamplerDefaultActivation", props.toXML());
}

void KisColorSamplerConfig::load()
{
    KisPropertiesConfiguration props;
    KConfigGroup config = KSharedConfig::openConfig()->group(configGroupName);
    props.fromXML(config.readEntry("ColorSamplerDefaultActivation"));

    toForegroundColor = props.getBool("toForegroundColor", true);
    updateColor = props.getBool("updateColor", true);
    addColorToCurrentPalette = props.getBool("addPalette", false);
    normaliseValues = props.getBool("normaliseValues", false);
    sampleMerged = props.getBool("sampleMerged", true);
    radius = props.getInt("radius", 1);
    blend = props.getInt("blend", 100);
}
