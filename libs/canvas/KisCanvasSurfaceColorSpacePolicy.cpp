/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisCanvasSurfaceColorSpacePolicy.h"

namespace KisCanvasSurfaceColorSpacePolicy
{

bool Luminance::operator==(const Luminance &rhs) const
{
    return minLuminance == rhs.minLuminance &&
        maxLuminance == rhs.maxLuminance &&
        referenceLuminance == rhs.referenceLuminance;
}

Luminance Luminance::clipToSdr() const
{
    return {minLuminance, referenceLuminance, referenceLuminance};
}

bool ColorSpace::operator==(const ColorSpace &rhs) const
{
    return primaries == rhs.primaries &&
        transferFunction == rhs.transferFunction &&
        luminance == rhs.luminance;
}

bool ColorSpace::isHdr() const
{
    return luminance && luminance->maxLuminance > luminance->referenceLuminance;
}

bool SurfaceDescription::operator==(const SurfaceDescription &rhs) const
{
    return colorSpace == rhs.colorSpace;
}

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
        return {std::nullopt, true, {}};
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
        if (compositorPreferred.colorSpace.isHdr()) {
            requestedDescription.colorSpace.primaries = NamedPrimaries::Bt2020;
            requestedDescription.colorSpace.transferFunction = NamedTransferFunction::St2084Pq;
        } else {
            requestedDescription.colorSpace = compositorPreferred.colorSpace;
        }

        if (requestedDescription.colorSpace.transferFunction == NamedTransferFunction::St2084Pq) {
            requestedDescription.colorSpace.luminance = makeKritaRec2020PQLuminance();
        }
    } else if (requestedSurfaceMode == KisCanvasSurfaceMode::Rec2020pq) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::Bt2020;
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::St2084Pq;
        requestedDescription.colorSpace.luminance = makeKritaRec2020PQLuminance();
    } else if (requestedSurfaceMode == KisCanvasSurfaceMode::Rec709g22) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::SRgb;
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::Gamma22;
        if (compositorPreferred.colorSpace.luminance) {
            requestedDescription.colorSpace.luminance =
                compositorPreferred.colorSpace.luminance->clipToSdr();
        }
    } else if (requestedSurfaceMode == KisCanvasSurfaceMode::Rec709g10) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::SRgb;
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::ExtendedLinear;
        if (compositorPreferred.colorSpace.luminance) {
            requestedDescription.colorSpace.luminance =
                compositorPreferred.colorSpace.luminance->clipToSdr();
        }
    }

    if (requestedDescription.colorSpace.primaries == NamedPrimaries::Unknown) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::SRgb;
    }
    if (requestedDescription.colorSpace.transferFunction == NamedTransferFunction::Unknown) {
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::Gamma22;
    }

    if (!supportsDescription(requestedDescription)) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::SRgb;
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::SRgb;
        if (!supportsDescription(requestedDescription)) {
            requestedDescription.colorSpace.transferFunction = NamedTransferFunction::Gamma22;
            if (!supportsDescription(requestedDescription)) {
                return {std::nullopt, false,
                        QStringLiteral("failed to find a suitable surface format for the compositor")};
            }
        }
    }

    bool hasProfile = profileAvailable(requestedDescription);
    if (!hasProfile) {
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::Gamma22;
        if (supportsDescription(requestedDescription)) {
            hasProfile = profileAvailable(requestedDescription);
        }
    }
    if (!hasProfile) {
        requestedDescription.colorSpace.primaries = NamedPrimaries::SRgb;
        if (supportsDescription(requestedDescription)) {
            hasProfile = profileAvailable(requestedDescription);
        }
    }
    if (!hasProfile) {
        requestedDescription.colorSpace.transferFunction = NamedTransferFunction::SRgb;
        if (supportsDescription(requestedDescription)) {
            hasProfile = profileAvailable(requestedDescription);
        }
    }
    if (!hasProfile) {
        return {std::nullopt, false,
                QStringLiteral("failed to create a profile for the compositor's preferred color space")};
    }

    return {requestedDescription, true, {}};
}

RenderIntent calculateConfigIntent(const KisDisplayConfig::Options &options)
{
    switch (options.first) {
    case INTENT_RELATIVE_COLORIMETRIC:
        return options.second.testFlag(KoColorConversionTransformation::BlackpointCompensation)
            ? RenderIntent::RelativeBpc
            : RenderIntent::Relative;
    case INTENT_SATURATION:
        return RenderIntent::Saturation;
    case INTENT_ABSOLUTE_COLORIMETRIC:
        return RenderIntent::Absolute;
    case INTENT_PERCEPTUAL:
    default:
        return RenderIntent::Perceptual;
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
        result.intent = RenderIntent::Perceptual;
        if (!supportsIntent(result.intent)) {
            result.deferred = true;
            result.errorMessage = QStringLiteral("perceptual rendering intent is unsupported");
            return result;
        }
    }

    SelectionResult selection;
    if (input.surfaceMode == KisCanvasSurfaceMode::Unmanaged) {
        selection = {std::nullopt, true, {}};
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
    result.hasProfile = selection.hasProfile;
    result.errorMessage = selection.errorMessage;
    result.isCanvasHdr = result.requestedDescription &&
        result.requestedDescription->colorSpace.isHdr();

    if (input.currentDescription != result.requestedDescription ||
        input.currentIntent != std::optional<RenderIntent>(result.intent)) {
        result.command = result.requestedDescription ? ProtocolCommand::Set : ProtocolCommand::Unset;
    }
    return result;
}

} // namespace KisCanvasSurfaceColorSpacePolicy
