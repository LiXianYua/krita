/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2020 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_HALFTONE_FILTER_CONFIGURATION_H
#define KIS_HALFTONE_FILTER_CONFIGURATION_H

#include <PkHash.h>
#include <PkString.h>
#include <PkStringList.h>

#include <kis_filter_configuration.h>
#include <KoColor.h>
#include <KoColorSpaceRegistry.h>
#include <generator/kis_generator_registry.h>

class KisHalftoneFilterConfiguration;
typedef KisPinnedSharedPtr<KisHalftoneFilterConfiguration> KisHalftoneFilterConfigurationSP;

class KisHalftoneFilterConfiguration : public KisFilterConfiguration
{
public:
    KisHalftoneFilterConfiguration(const PkString & name, qint32 version, KisResourcesInterfaceSP resourcesInterface);
    KisHalftoneFilterConfiguration(const KisHalftoneFilterConfiguration &rhs);

    ~KisHalftoneFilterConfiguration() override;

    KisFilterConfigurationSP clone() const override;

    void setResourcesInterface(KisResourcesInterfaceSP resourcesInterface) override;
    PkList<KoResourceLoadResult> linkedResources(KisResourcesInterfaceSP globalResourcesInterface) const override;
    PkList<KoResourceLoadResult> embeddedResources(KisResourcesInterfaceSP globalResourcesInterface) const override;

    static constexpr const char *HalftoneMode_Intensity = "intensity";
    static constexpr const char *HalftoneMode_IndependentChannels = "independent_channels";
    static constexpr const char *HalftoneMode_Alpha = "alpha";

    // default properties
    inline static PkString defaultMode() { return HalftoneMode_Intensity; }

    inline static PkString defaultGeneratorId()
    {
        static PkString defaultGeneratorId;
        if (defaultGeneratorId.isNull()) {
            PkStringList generatorIds = KisGeneratorRegistry::instance()->keys();
            if (generatorIds.size() == 0) {
                defaultGeneratorId = "";
            } else {
                generatorIds.sort();
                if (generatorIds.indexOf("screentone") == -1) {
                    defaultGeneratorId = generatorIds.at(0);
                } else {
                    defaultGeneratorId = "screentone";
                }
            }
        }
        return defaultGeneratorId;
    }

    static constexpr qreal defaultHardness() { return 80.0; }

    static constexpr bool defaultInvert() { return false; }

    inline static const KoColor& defaultForegroundColor()
    {
        static const KoColor c(Qt::black, KoColorSpaceRegistry::instance()->rgb8());
        return c;
    }

    inline static const KoColor& defaultBackgroundColor()
    {
        static const KoColor c(Qt::white, KoColorSpaceRegistry::instance()->rgb8());
        return c;
    }

    static constexpr int defaultForegroundOpacity() { return 100; }

    static constexpr int defaultBackgroundOpacity() { return 100; }

    PkString colorModelId() const;
    PkString mode() const;
    PkString generatorId(const PkString &prefix) const;
    KisFilterConfigurationSP generatorConfiguration(const PkString &prefix) const;
    qreal hardness(const PkString &prefix) const;
    bool invert(const PkString &prefix) const;
    KoColor foregroundColor(const PkString &prefix) const;
    int foregroundOpacity(const PkString &prefix) const;
    KoColor backgroundColor(const PkString &prefix) const;
    int backgroundOpacity(const PkString &prefix) const;

    void setColorModelId(const PkString &newColorModelId);
    void setMode(const PkString &newMode);
    void setGeneratorId(const PkString &prefix, const PkString &id);
    void setGeneratorConfiguration(const PkString &prefix, KisFilterConfigurationSP config);
    void setHardness(const PkString &prefix, qreal newHardness);
    void setInvert(const PkString &prefix, bool newInvert);
    void setForegroundColor(const PkString &prefix, const KoColor &newForegroundColor);
    void setForegroundOpacity(const PkString &prefix, int newOpacity);
    void setBackgroundColor(const PkString &prefix, const KoColor &newBackgroundColor);
    void setBackgroundOpacity(const PkString &prefix, int newBackgroundOpacity);

    void setProperty(const PkString &name, const PkVariant &value) override;

private:
    mutable PkHash<PkString, KisFilterConfigurationSP> m_generatorConfigurationsCache;
};

#endif
