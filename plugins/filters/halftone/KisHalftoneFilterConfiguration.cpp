/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2020 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <generator/kis_generator.h>
#include <generator/kis_generator_registry.h>
#include <KoResourceLoadResult.h>

#include "KisHalftoneFilterConfiguration.h"

KisHalftoneFilterConfiguration::KisHalftoneFilterConfiguration(const PkString & name,
                                                               qint32 version,
                                                               KisResourcesInterfaceSP resourcesInterface)
    : KisFilterConfiguration(name, version, resourcesInterface)
{}

KisHalftoneFilterConfiguration::KisHalftoneFilterConfiguration(const KisHalftoneFilterConfiguration &rhs)
    : KisFilterConfiguration(rhs)
{
    // PkHash 迭代器解引用得 value（Qt 形状，非 std::pair），键取 it.key()。
    for (auto it = rhs.m_generatorConfigurationsCache.constBegin();
         it != rhs.m_generatorConfigurationsCache.constEnd(); ++it) {
        m_generatorConfigurationsCache[it.key()] = it.value()->clone();
    }
}

KisHalftoneFilterConfiguration::~KisHalftoneFilterConfiguration()
{}

KisFilterConfigurationSP KisHalftoneFilterConfiguration::clone() const
{
    return new KisHalftoneFilterConfiguration(*this);
}

void KisHalftoneFilterConfiguration::setResourcesInterface(KisResourcesInterfaceSP resourcesInterface)
{
    KisFilterConfiguration::setResourcesInterface(resourcesInterface);

    if (mode() == HalftoneMode_IndependentChannels) {
        const PkString prefix = colorModelId() + "_channel";
        for (int i = 0; i < 4; ++i) {
            const PkString fullPrefix = prefix + PkString("%1").arg(i) + "_";
            KisFilterConfigurationSP generatorConfig = generatorConfiguration(fullPrefix);
            if (generatorConfig) {
                m_generatorConfigurationsCache[fullPrefix]->setResourcesInterface(resourcesInterface);
            }
        }
    } else {
        const PkString prefix = mode() + "_";
        KisFilterConfigurationSP generatorConfig = generatorConfiguration(prefix);
        if (generatorConfig) {
            m_generatorConfigurationsCache[prefix]->setResourcesInterface(resourcesInterface);
        }
    }
}

PkList<KoResourceLoadResult> KisHalftoneFilterConfiguration::linkedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    PkList<KoResourceLoadResult> resourcesList;

    if (mode() == HalftoneMode_IndependentChannels) {
        const PkString prefix = colorModelId() + "_channel";
        for (int i = 0; i < 4; ++i) {
            const PkString fullPrefix = prefix + PkString("%1").arg(i) + "_";
            KisFilterConfigurationSP generatorConfig = generatorConfiguration(fullPrefix);
            if (generatorConfig) {
                resourcesList += generatorConfig->linkedResources(globalResourcesInterface);
            }
        }
    } else {
        const PkString prefix = mode() + "_";
        KisFilterConfigurationSP generatorConfig = generatorConfiguration(prefix);
        if (generatorConfig) {
            resourcesList += generatorConfig->linkedResources(globalResourcesInterface);
        }
    }

    return resourcesList;
}

PkList<KoResourceLoadResult> KisHalftoneFilterConfiguration::embeddedResources(KisResourcesInterfaceSP globalResourcesInterface) const
{
    PkList<KoResourceLoadResult> resourcesList;

    if (mode() == HalftoneMode_IndependentChannels) {
        const PkString prefix = colorModelId() + "_channel";
        for (int i = 0; i < 4; ++i) {
            const PkString fullPrefix = prefix + PkString("%1").arg(i) + "_";
            KisFilterConfigurationSP generatorConfig = generatorConfiguration(fullPrefix);
            if (generatorConfig) {
                resourcesList += generatorConfig->embeddedResources(globalResourcesInterface);
            }
        }
    } else {
        const PkString prefix = mode() + "_";
        KisFilterConfigurationSP generatorConfig = generatorConfiguration(prefix);
        if (generatorConfig) {
            resourcesList += generatorConfig->embeddedResources(globalResourcesInterface);
        }
    }

    return resourcesList;
}

PkString KisHalftoneFilterConfiguration::colorModelId() const
{
    return getString("color_model_id", "");
}

PkString KisHalftoneFilterConfiguration::mode() const
{
    return getString("mode", "");
}

PkString KisHalftoneFilterConfiguration::generatorId(const PkString &prefix) const
{
    return getString(prefix + "generator", "");
}

KisFilterConfigurationSP KisHalftoneFilterConfiguration::generatorConfiguration(const PkString &prefix) const
{
    if (m_generatorConfigurationsCache.contains(prefix)) {
        return m_generatorConfigurationsCache[prefix];
    } else {
        PkStringList generatorIds = KisGeneratorRegistry::instance()->keys();
        PkString generatorId = this->generatorId(prefix);
        if (generatorIds.indexOf(generatorId) != -1) {
            PkString fullGeneratorId = prefix + "generator_" + generatorId;
            KisGeneratorSP generator = KisGeneratorRegistry::instance()->get(generatorId);
            KisFilterConfigurationSP generatorConfig = generator->defaultConfiguration(resourcesInterface());
            getPrefixedProperties(fullGeneratorId + "_", generatorConfig);
            m_generatorConfigurationsCache[prefix] = generatorConfig;
            return generatorConfig;
        }
    }
    return nullptr;
}

qreal KisHalftoneFilterConfiguration::hardness(const PkString &prefix) const
{
    return getDouble(prefix + "hardness", defaultHardness());
}

bool KisHalftoneFilterConfiguration::invert(const PkString &prefix) const
{
    return getBool(prefix + "invert", defaultInvert());
}

KoColor KisHalftoneFilterConfiguration::foregroundColor(const PkString &prefix) const
{
    return getColor(prefix + "foreground_color", defaultForegroundColor());
}

int KisHalftoneFilterConfiguration::foregroundOpacity(const PkString &prefix) const
{
    return getInt(prefix + "foreground_opacity", defaultForegroundOpacity());
}

KoColor KisHalftoneFilterConfiguration::backgroundColor(const PkString &prefix) const
{
    return getColor(prefix + "background_color", defaultBackgroundColor());
}

int KisHalftoneFilterConfiguration::backgroundOpacity(const PkString &prefix) const
{
    return getInt(prefix + "background_opacity", defaultForegroundOpacity());
}

void KisHalftoneFilterConfiguration::setColorModelId(const PkString &newColorModelId)
{
    setProperty("color_model_id", newColorModelId);
}

void KisHalftoneFilterConfiguration::setMode(const PkString &newMode)
{
    setProperty("mode", newMode);
}

void KisHalftoneFilterConfiguration::setGeneratorId(const PkString &prefix, const PkString &id)
{
    setProperty(prefix + "generator", id);
}

void KisHalftoneFilterConfiguration::setGeneratorConfiguration(const PkString &prefix, KisFilterConfigurationSP config)
{
    if (!config) {
        return;
    }

    PkString generatorId = this->generatorId(prefix);
    PkString fullGeneratorId = prefix + "generator_" + generatorId;
    setPrefixedProperties(fullGeneratorId + "_", config);
    m_generatorConfigurationsCache[prefix] = config;
}

void KisHalftoneFilterConfiguration::setHardness(const PkString & prefix, qreal newHardness)
{
    setProperty(prefix + "hardness", newHardness);
}

void KisHalftoneFilterConfiguration::setInvert(const PkString & prefix, bool newInvert)
{
    setProperty(prefix + "invert", newInvert);
}

void KisHalftoneFilterConfiguration::setForegroundColor(const PkString & prefix, const KoColor & newForegroundColor)
{
    PkVariant v;
    v.setValue(newForegroundColor);
    setProperty(prefix + "foreground_color", v);
}

void KisHalftoneFilterConfiguration::setForegroundOpacity(const PkString & prefix, int newForegroundOpacity)
{
    setProperty(prefix + "foreground_opacity", newForegroundOpacity);
}
void KisHalftoneFilterConfiguration::setBackgroundColor(const PkString & prefix, const KoColor & newBackgroundColor)
{
    PkVariant v;
    v.setValue(newBackgroundColor);
    setProperty(prefix + "background_color", v);
}

void KisHalftoneFilterConfiguration::setBackgroundOpacity(const PkString & prefix, int newBackgroundOpacity)
{
    setProperty(prefix + "background_opacity", newBackgroundOpacity);
}

void KisHalftoneFilterConfiguration::setProperty(const PkString &name, const PkVariant &value)
{
    KisFilterConfiguration::setProperty(name, value);

    // The generator configurations are cached, so we need to check if the
    // property name of a property being set represents a property of a
    // generator configuration, and in that case we must remove the cache entry
    // that property belongs to, so that the configuration can be regenerated
    // later. This is an issue mainly when setting the properties directly
    // (through python for example) instead of using the high level methods.
    const std::vector<PkString> nameParts = name.split('_');
    if (nameParts.size() < 3) {
        return;
    }
    const int generatorKeywordIndex = nameParts[0] == "alpha" || nameParts[0] == "intensity" ? 1 : 2;
    if (nameParts[generatorKeywordIndex] != "generator") {
        return;
    }
    if (generatorKeywordIndex == 1) {
        m_generatorConfigurationsCache.remove(nameParts[0] + "_");
    } else {
        m_generatorConfigurationsCache.remove(nameParts[0] + "_" + nameParts[1] + "_");
    }
}
