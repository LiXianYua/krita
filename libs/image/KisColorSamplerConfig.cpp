/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisColorSamplerConfig.h"

#include <KConfigGroup>
#include <KSharedConfig>
#include <QByteArray>
#include <QString>

#include "kis_properties_configuration.h"

namespace {
const PkString configGroupName = PkString("tool_color_sampler");

QString pkToQString(const PkString &value)
{
    const std::string utf8 = value.PkToUtf8();
    return QString::fromUtf8(utf8.data(), int(utf8.size()));
}

PkString qStringToPk(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return PkString::PkFromUtf8(utf8.constData(), utf8.size());
}
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

    KConfigGroup config = KSharedConfig::openConfig()->group(pkToQString(configGroupName));
    config.writeEntry("ColorSamplerDefaultActivation", pkToQString(props.toXML()));
}

void KisColorSamplerConfig::load()
{
    KisPropertiesConfiguration props;
    KConfigGroup config = KSharedConfig::openConfig()->group(pkToQString(configGroupName));
    props.fromXML(qStringToPk(config.readEntry("ColorSamplerDefaultActivation", QString())));

    toForegroundColor = props.getBool("toForegroundColor", true);
    updateColor = props.getBool("updateColor", true);
    addColorToCurrentPalette = props.getBool("addPalette", false);
    normaliseValues = props.getBool("normaliseValues", false);
    sampleMerged = props.getBool("sampleMerged", true);
    radius = props.getInt("radius", 1);
    blend = props.getInt("blend", 100);
}
