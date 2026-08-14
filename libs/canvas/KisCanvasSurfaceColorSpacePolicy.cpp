/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisCanvasSurfaceColorSpacePolicy.h"

namespace KisCanvasSurfaceColorSpacePolicy
{

KisCanvasSurfaceMode surfaceModeFromConfig(const QString &value)
{
    if (value == QStringLiteral("preferred")) return KisCanvasSurfaceMode::Preferred;
    if (value == QStringLiteral("rec709g22")) return KisCanvasSurfaceMode::Rec709g22;
    if (value == QStringLiteral("rec709g10")) return KisCanvasSurfaceMode::Rec709g10;
    if (value == QStringLiteral("rec2020pq")) return KisCanvasSurfaceMode::Rec2020pq;
    if (value == QStringLiteral("unmanaged")) return KisCanvasSurfaceMode::Unmanaged;
    return KisCanvasSurfaceMode::Preferred;
}

QString surfaceModeToConfig(KisCanvasSurfaceMode mode)
{
    switch (mode) {
    case KisCanvasSurfaceMode::Preferred: return QStringLiteral("preferred");
    case KisCanvasSurfaceMode::Rec709g22: return QStringLiteral("rec709g22");
    case KisCanvasSurfaceMode::Rec709g10: return QStringLiteral("rec709g10");
    case KisCanvasSurfaceMode::Rec2020pq: return QStringLiteral("rec2020pq");
    case KisCanvasSurfaceMode::Unmanaged: return QStringLiteral("unmanaged");
    }
    return QStringLiteral("preferred");
}

SelectionResult selectSurfaceDescription(
    KisCanvasSurfaceMode requestedSurfaceMode,
    const SurfaceDescription &compositorPreferred,
    const SupportsDescription &supportsDescription,
    const ProfileAvailable &profileAvailable)
{
    if (requestedSurfaceMode == KisCanvasSurfaceMode::Unmanaged) {
        return {std::nullopt, ProfileSource::BuiltInSrgb, {}};
    }

    SurfaceDescription requestedDescription;
    auto makeKritaRec2020PQLuminance = []() {
        Luminance luminance;
        luminance.minLuminance = 0;
        luminance.referenceLuminance = 80;
        luminance.maxLuminance = 10000;
        return luminance;
    };

    if (requestedSurfaceMode == KisCanvasSurfaceMode::Preferred) {
        if (compositorPreferred.colorSpace.isHDR()) {
            requestedDescription.colorSpace.primaries = NamedPrimaries::primaries_bt2020;
            requestedDescription.colorSpace.transferFunction = NamedTransferFunction::transfer_function_st2084_pq;
        } else {
            requestedDescription.colorSpace = compositorPreferred.colorSpace;
        }

        if (std::holds_alternative<NamedTransferFunction>(requestedDescription.colorSpace.transferFunction) &&
            std::get<NamedTransferFunction>(requestedDescription.colorSpace.transferFunction) ==
                NamedTransferFunction::transfer_function_st2084_pq) {
            requestedDescription.colorSpace.luminance = makeKritaRec2020PQLuminance();
        }
    } else if (requestedSurfaceMode == KisCanvasSurfaceMode::Rec2020pq) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::primaries_bt2020;
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::transfer_function_st2084_pq;
        requestedDescription.colorSpace.luminance = makeKritaRec2020PQLuminance();
    } else if (requestedSurfaceMode == KisCanvasSurfaceMode::Rec709g22) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::primaries_srgb;
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::transfer_function_gamma22;
        if (compositorPreferred.colorSpace.luminance) {
            requestedDescription.colorSpace.luminance =
                compositorPreferred.colorSpace.luminance->clipToSdr();
        }
    } else if (requestedSurfaceMode == KisCanvasSurfaceMode::Rec709g10) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::primaries_srgb;
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::transfer_function_ext_linear;
        if (compositorPreferred.colorSpace.luminance) {
            requestedDescription.colorSpace.luminance =
                compositorPreferred.colorSpace.luminance->clipToSdr();
        }
    }

    if (std::holds_alternative<NamedPrimaries>(requestedDescription.colorSpace.primaries) &&
        std::get<NamedPrimaries>(requestedDescription.colorSpace.primaries) == NamedPrimaries::primaries_unknown) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::primaries_srgb;
    }
    if (std::holds_alternative<NamedTransferFunction>(requestedDescription.colorSpace.transferFunction) &&
        std::get<NamedTransferFunction>(requestedDescription.colorSpace.transferFunction) ==
            NamedTransferFunction::transfer_function_unknown) {
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::transfer_function_gamma22;
    }

    if (!supportsDescription(requestedDescription)) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::primaries_srgb;
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::transfer_function_srgb;
        if (!supportsDescription(requestedDescription)) {
            requestedDescription.colorSpace.transferFunction = NamedTransferFunction::transfer_function_gamma22;
            if (!supportsDescription(requestedDescription)) {
                return {std::nullopt, ProfileSource::BuiltInSrgb,
                        QStringLiteral("failed to find a suitable surface format for the compositor")};
            }
        }
    }

    bool hasProfile = profileAvailable(requestedDescription);
    if (!hasProfile) {
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::transfer_function_gamma22;
        if (supportsDescription(requestedDescription)) {
            hasProfile = profileAvailable(requestedDescription);
        }
    }
    if (!hasProfile) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::primaries_srgb;
        if (supportsDescription(requestedDescription)) {
            hasProfile = profileAvailable(requestedDescription);
        }
    }
    if (!hasProfile) {
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::transfer_function_srgb;
        if (supportsDescription(requestedDescription)) {
            hasProfile = profileAvailable(requestedDescription);
        }
    }
    if (!hasProfile) {
        return {std::nullopt, ProfileSource::BuiltInSrgb,
                QStringLiteral("failed to create a profile for the compositor's preferred color space")};
    }

    return {requestedDescription, ProfileSource::Generated, {}};
}

RenderIntent calculateConfigIntent(const KisDisplayConfig::Options &options)
{
    switch (options.first) {
    case INTENT_RELATIVE_COLORIMETRIC:
        return options.second.testFlag(KoColorConversionTransformation::BlackpointCompensation)
            ? RenderIntent::render_intent_relative_bpc
            : RenderIntent::render_intent_relative;
    case INTENT_SATURATION:
        return RenderIntent::render_intent_saturation;
    case INTENT_ABSOLUTE_COLORIMETRIC:
        return RenderIntent::render_intent_absolute;
    case INTENT_PERCEPTUAL:
    default:
        return RenderIntent::render_intent_perceptual;
    }
}

NegotiationInput::NegotiationInput()
    : options(KisMultiSurfaceStateManager::legacyConfigDefaults().options)
{
}

NegotiationResult negotiate(const NegotiationInput &input,
                            const SupportsIntent &supportsIntent,
                            const SupportsDescription &supportsDescription,
                            const ProfileAvailable &profileAvailable)
{
    NegotiationResult result;
    if (!input.ready) {
        result.deferred = true;
        return result;
    }

    result.intent = calculateConfigIntent(input.options);
    if (!supportsIntent(result.intent)) {
        result.intent = RenderIntent::render_intent_perceptual;
        if (!supportsIntent(result.intent)) {
            result.deferred = true;
            result.errorMessage = QStringLiteral("perceptual rendering intent is unsupported");
            return result;
        }
    }

    SelectionResult selection;
    if (input.surfaceMode == KisCanvasSurfaceMode::Unmanaged) {
        selection = {std::nullopt, ProfileSource::BuiltInSrgb, {}};
    } else if (!input.compositorPreferred) {
        result.deferred = true;
        return result;
    } else {
        selection = selectSurfaceDescription(input.surfaceMode,
                                             *input.compositorPreferred,
                                             supportsDescription,
                                             profileAvailable);
    }

    result.requestedDescription = selection.requestedDescription;
    result.profileSource = selection.profileSource;
    result.errorMessage = selection.errorMessage;
    result.isCanvasHdr = result.requestedDescription &&
        result.requestedDescription->colorSpace.isHDR();

    if (input.currentDescription != result.requestedDescription ||
        input.currentIntent != std::optional<RenderIntent>(result.intent)) {
        result.command = result.requestedDescription ? ProtocolCommand::Set : ProtocolCommand::Unset;
    }
    return result;
}

} // namespace KisCanvasSurfaceColorSpacePolicy
