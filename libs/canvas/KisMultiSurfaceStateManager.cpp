/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisMultiSurfaceStateManager.h"

KisMultiSurfaceStateManager::InitializationContext::InitializationContext()
    : configuration(KisMultiSurfaceStateManager::legacyConfigDefaults())
{
}

bool KisMultiSurfaceStateManager::State::operator==(const State &other) const
{
    return isCanvasOpenGL == other.isCanvasOpenGL &&
        proofingConfig == other.proofingConfig &&
        surfaceMode == other.surfaceMode &&
        optionsFromConfig == other.optionsFromConfig &&
        multiConfig == other.multiConfig;
}

KisMultiSurfaceStateManager::Configuration KisMultiSurfaceStateManager::legacyConfigDefaults()
{
    return {
        KisCanvasSurfaceMode::Preferred,
        {KoColorConversionTransformation::IntentPerceptual,
         KoColorConversionTransformation::HighQuality |
             KoColorConversionTransformation::BlackpointCompensation}
    };
}

KisMultiSurfaceStateManager::State KisMultiSurfaceStateManager::createInitializingConfig(
    const InitializationContext &context) const
{
    State state;
    state.isCanvasOpenGL = context.isCanvasOpenGL;
    state.optionsFromConfig = context.configuration.options;
    state.surfaceMode = context.configuration.surfaceMode;
    state.proofingConfig = context.proofingConfig;

    if (context.legacyHdrMode) {
        state.multiConfig.canvasProfile = state.isCanvasOpenGL
            ? context.legacyHdrProfile
            : context.srgbProfile;
        state.multiConfig.uiProfile = context.srgbProfile;
    } else if (context.surfaceManagedByOs) {
        state.multiConfig.canvasProfile = context.rootSurfaceProfile;
        state.multiConfig.uiProfile = context.rootSurfaceProfile;
    } else {
        const KoColorProfile *profile = context.displayProfile
            ? context.displayProfile
            : context.srgbProfile;
        state.multiConfig.canvasProfile = profile;
        state.multiConfig.uiProfile = profile;
    }

    state.multiConfig.setOptions(
        overriddenWithProofingConfig(state.optionsFromConfig, state.proofingConfig));
    state.multiConfig.isCanvasHDR = false;
    return state;
}

KisMultiSurfaceStateManager::State KisMultiSurfaceStateManager::onCanvasSurfaceFormatChanged(
    const State &oldState,
    const KisDisplayConfig &canvasConfig,
    bool surfaceManagedByOs,
    bool legacyHdrMode) const
{
    if (!surfaceManagedByOs || legacyHdrMode || !oldState.isCanvasOpenGL) {
        return oldState;
    }

    State newState = oldState;
    newState.multiConfig.canvasProfile = canvasConfig.profile;
    newState.multiConfig.setOptions(canvasConfig.options());
    newState.multiConfig.isCanvasHDR = canvasConfig.isHDR;
    return newState;
}

KisMultiSurfaceStateManager::State KisMultiSurfaceStateManager::onGuiSurfaceFormatChanged(
    const State &oldState,
    const KoColorProfile *uiProfile,
    bool surfaceManagedByOs,
    bool legacyHdrMode) const
{
    if (!surfaceManagedByOs || legacyHdrMode) {
        return oldState;
    }

    State newState = oldState;
    newState.multiConfig.uiProfile = uiProfile;
    return newState;
}

KisMultiSurfaceStateManager::State KisMultiSurfaceStateManager::onProofingChanged(
    const State &oldState,
    KisProofingConfigurationSP proofingConfig) const
{
    State newState = oldState;
    newState.proofingConfig = proofingConfig;
    newState.multiConfig.setOptions(
        overriddenWithProofingConfig(newState.optionsFromConfig, newState.proofingConfig));
    return newState;
}

KisMultiSurfaceStateManager::State KisMultiSurfaceStateManager::onConfigChanged(
    const State &oldState,
    KisCanvasSurfaceMode surfaceMode,
    const KisDisplayConfig::Options &options,
    const KoColorProfile *displayProfile,
    const KoColorProfile *srgbProfile,
    bool legacyHdrMode,
    bool surfaceManagedByOs) const
{
    State newState = oldState;
    newState.surfaceMode = surfaceMode;
    newState.optionsFromConfig = options;
    newState.multiConfig.setOptions(
        overriddenWithProofingConfig(newState.optionsFromConfig, newState.proofingConfig));

    if (!legacyHdrMode && !surfaceManagedByOs) {
        const KoColorProfile *profile = displayProfile ? displayProfile : srgbProfile;
        newState.multiConfig.canvasProfile = profile;
        newState.multiConfig.uiProfile = profile;
    }
    return newState;
}

KisMultiSurfaceStateManager::State KisMultiSurfaceStateManager::onScreenChanged(
    const State &oldState,
    const KoColorProfile *displayProfile,
    const KoColorProfile *srgbProfile,
    bool legacyHdrMode,
    bool surfaceManagedByOs) const
{
    if (legacyHdrMode || surfaceManagedByOs) {
        return oldState;
    }
    return onConfigChanged(oldState,
                           oldState.surfaceMode,
                           oldState.optionsFromConfig,
                           displayProfile,
                           srgbProfile,
                           legacyHdrMode,
                           surfaceManagedByOs);
}

KisDisplayConfig::Options KisMultiSurfaceStateManager::overriddenWithProofingConfig(
    const KisDisplayConfig::Options &options,
    KisProofingConfigurationSP proofingConfig) const
{
    if (proofingConfig &&
        proofingConfig->displayFlags.testFlag(KoColorConversionTransformation::SoftProofing)) {
        return {proofingConfig->determineDisplayIntent(options.first),
                proofingConfig->determineDisplayFlags(options.second)};
    }
    return options;
}
