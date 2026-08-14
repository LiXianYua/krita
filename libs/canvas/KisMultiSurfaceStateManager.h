/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISMULTISURFACESTATEMANAGER_H
#define KISMULTISURFACESTATEMANAGER_H

#include <kritacanvas_export.h>

#include <KisDisplayConfig.h>
#include <KisProofingConfiguration.h>
#include <kis_types.h>

class KoColorProfile;

enum class KisCanvasSurfaceMode {
    Preferred = 0,
    Rec709g22,
    Rec709g10,
    Rec2020pq,
    Unmanaged
};

class KRITACANVAS_EXPORT KisMultiSurfaceStateManager
{
public:
    struct Configuration {
        KisCanvasSurfaceMode surfaceMode {KisCanvasSurfaceMode::Preferred};
        KisDisplayConfig::Options options;
    };

    struct InitializationContext {
        bool isCanvasOpenGL {false};
        bool legacyHdrMode {false};
        bool surfaceManagedByOs {false};
        KisProofingConfigurationSP proofingConfig;
        Configuration configuration;
        const KoColorProfile *srgbProfile {nullptr};
        const KoColorProfile *legacyHdrProfile {nullptr};
        const KoColorProfile *rootSurfaceProfile {nullptr};
        const KoColorProfile *displayProfile {nullptr};

        InitializationContext();
    };

    struct State {
        bool isCanvasOpenGL {false};
        KisProofingConfigurationSP proofingConfig;
        KisCanvasSurfaceMode surfaceMode {KisCanvasSurfaceMode::Preferred};
        KisDisplayConfig::Options optionsFromConfig;
        KisMultiSurfaceDisplayConfig multiConfig;

        bool operator==(const State &other) const;
        bool operator!=(const State &other) const { return !(*this == other); }
    };

    static Configuration legacyConfigDefaults();

    State createInitializingConfig(const InitializationContext &context) const;
    State onCanvasSurfaceFormatChanged(const State &oldState,
                                       const KisDisplayConfig &canvasConfig,
                                       bool surfaceManagedByOs,
                                       bool legacyHdrMode) const;
    State onGuiSurfaceFormatChanged(const State &oldState,
                                    const KoColorProfile *uiProfile,
                                    bool surfaceManagedByOs,
                                    bool legacyHdrMode) const;
    State onProofingChanged(const State &oldState,
                            KisProofingConfigurationSP proofingConfig) const;
    State onConfigChanged(const State &oldState,
                          KisCanvasSurfaceMode surfaceMode,
                          const KisDisplayConfig::Options &options,
                          const KoColorProfile *displayProfile,
                          const KoColorProfile *srgbProfile,
                          bool legacyHdrMode,
                          bool surfaceManagedByOs) const;
    State onScreenChanged(const State &oldState,
                          const KoColorProfile *displayProfile,
                          const KoColorProfile *srgbProfile,
                          bool legacyHdrMode,
                          bool surfaceManagedByOs) const;

private:
    KisDisplayConfig::Options overriddenWithProofingConfig(
        const KisDisplayConfig::Options &options,
        KisProofingConfigurationSP proofingConfig) const;
};

#endif // KISMULTISURFACESTATEMANAGER_H
