/*
 * SPDX-FileCopyrightText: 2017 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_asccdl_filter.h"
#include <filter/kis_filter_category_ids.h>
#include <filter/kis_filter_registry.h>
#include <filter/kis_color_transformation_configuration.h>
#include <cmath>

namespace {
struct KritaASCCDLFilterRegistration
{
    KritaASCCDLFilterRegistration()
    {
        KisFilterRegistry::instance()->add(KisFilterSP(new KisFilterASCCDL()));
    }
};
} // namespace
static KritaASCCDLFilterRegistration s_kritaASCCDLFilterRegistration;


KisFilterASCCDL::KisFilterASCCDL(): KisColorTransformationFilter(id(), FiltersCategoryAdjustId, PkString("&Slope, Offset, Power..."))
{
    setSupportsPainting(true);
    setSupportsAdjustmentLayers(true);
    setSupportsLevelOfDetail(true);
    setSupportsThreading(true);
    setColorSpaceIndependence(FULLY_INDEPENDENT);
    setShowConfigurationWidget(true);
}

KoColorTransformation *KisFilterASCCDL::createTransformation(const KoColorSpace *cs,
                                                             const KisFilterConfigurationSP config) const
{
    KoColor black(Qt::black, cs);
    KoColor white(Qt::white, cs);
    return new KisASCCDLTransformation(cs,
                                       config->getColor("slope", black),
                                       config->getColor("offset", white),
                                       config->getColor("power", black));
}

bool KisFilterASCCDL::needsTransparentPixels(const KisFilterConfigurationSP config, const KoColorSpace *cs) const
{
    KoColor black(Qt::black, cs);
    KoColor offset = config->getColor("offset", black);
    offset.convertTo(cs);
    if (cs->difference(black.data(), offset.data())>0) {
        return true;
    }
    return false;
}

KisFilterConfigurationSP KisFilterASCCDL::defaultConfiguration(KisResourcesInterfaceSP resourcesInterface) const
{
    KisFilterConfigurationSP config = factoryConfiguration(resourcesInterface);
    PkVariant colorVariant("KoColor");
    KoColor black;
    black.fromQColor(PkColor(Qt::black));
    KoColor white;
    white.fromQColor(PkColor(Qt::white));
    colorVariant.setValue(white);
    config->setProperty( "slope", colorVariant);
    config->setProperty( "power", colorVariant);
    colorVariant.setValue(black);
    config->setProperty("offset", colorVariant);
    return config;
}

KisASCCDLTransformation::KisASCCDLTransformation(const KoColorSpace *cs, KoColor slope, KoColor offset, KoColor power)
{
    PkVector<float> slopeN(cs->channelCount());
    slope.convertTo(cs);
    slope.colorSpace()->normalisedChannelsValue(slope.data(), slopeN);
    m_slope = slopeN;
    offset.convertTo(cs);
    PkVector<float> offsetN(cs->channelCount());
    offset.colorSpace()->normalisedChannelsValue(offset.data(), offsetN);
    m_offset = offsetN;
    power.convertTo(cs);
    PkVector<float> powerN(cs->channelCount());
    power.colorSpace()->normalisedChannelsValue(power.data(), powerN);
    m_power = powerN;
    m_cs = cs;
}

void KisASCCDLTransformation::transform(const quint8 *src, quint8 *dst, qint32 nPixels) const
{
    PkVector<float> normalised(m_cs->channelCount());
    const quint32 pixelSize = m_cs->pixelSize();
    const quint32 alphaPos = m_cs->alphaPos();
    const quint32 channelCount = m_cs->channelCount();

    while (nPixels--) {
        m_cs->normalisedChannelsValue(src, normalised);

        for (uint c = 0; c < channelCount; c++){
            if (c != alphaPos) {
                normalised[c] = std::pow( (normalised.at(c)*m_slope.at(c))+m_offset.at(c), m_power.at(c));
            }
        }
        m_cs->fromNormalisedChannelsValue(dst, normalised);
        src += pixelSize;
        dst += pixelSize;
    }
}
