/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISCANVASSURFACECOLORSPACEPOLICY_H
#define KISCANVASSURFACECOLORSPACEPOLICY_H

#include <kritacanvas_export.h>

#include "KisMultiSurfaceStateManager.h"

#include <functional>
#include <optional>
#include <variant>

namespace KisCanvasSurfaceColorSpacePolicy
{

enum class NamedPrimaries {
    Unknown = 0,
    SRgb = 1,
    Bt2020 = 6,
    DciP3 = 8,
    DisplayP3 = 9,
    AdobeRgb = 10
};

enum class NamedTransferFunction {
    Unknown = 0,
    Bt1886 = 1,
    Gamma22 = 2,
    Gamma28 = 3,
    ExtendedLinear = 5,
    SRgb = 9,
    ExtendedSRgb = 10,
    St2084Pq = 11,
    St428 = 12
};

enum class RenderIntent {
    Perceptual = 0,
    Relative = 1,
    Saturation = 2,
    Absolute = 3,
    RelativeBpc = 4
};

struct KRITACANVAS_EXPORT Luminance
{
    quint32 minLuminance {2000};
    quint32 maxLuminance {80};
    quint32 referenceLuminance {80};

    Luminance() = default;
    Luminance(quint32 minValue, quint32 maxValue, quint32 referenceValue)
        : minLuminance(minValue)
        , maxLuminance(maxValue)
        , referenceLuminance(referenceValue)
    {
    }

    bool operator==(const Luminance &rhs) const;
    bool operator!=(const Luminance &rhs) const { return !(*this == rhs); }
    Luminance clipToSdr() const;
};

struct KRITACANVAS_EXPORT ColorSpace
{
    NamedPrimaries primaries {NamedPrimaries::Unknown};
    NamedTransferFunction transferFunction {NamedTransferFunction::Unknown};
    std::optional<Luminance> luminance;

    bool operator==(const ColorSpace &rhs) const;
    bool operator!=(const ColorSpace &rhs) const { return !(*this == rhs); }
    bool isHdr() const;
};

struct KRITACANVAS_EXPORT SurfaceDescription
{
    ColorSpace colorSpace;

    bool operator==(const SurfaceDescription &rhs) const;
    bool operator!=(const SurfaceDescription &rhs) const { return !(*this == rhs); }
};

using SupportsDescription = std::function<bool(const SurfaceDescription &)>;
using ProfileAvailable = std::function<bool(const SurfaceDescription &)>;
using SupportsIntent = std::function<bool(RenderIntent)>;

KRITACANVAS_EXPORT KisCanvasSurfaceMode surfaceModeFromConfig(const QString &value);
KRITACANVAS_EXPORT QString surfaceModeToConfig(KisCanvasSurfaceMode mode);

struct KRITACANVAS_EXPORT SelectionResult
{
    std::optional<SurfaceDescription> requestedDescription;
    bool hasProfile {false};
    QString errorMessage;
};

KRITACANVAS_EXPORT SelectionResult selectSurfaceDescription(
    KisCanvasSurfaceMode requestedSurfaceMode,
    const SurfaceDescription &compositorPreferred,
    const SupportsDescription &supportsDescription,
    const ProfileAvailable &profileAvailable);

KRITACANVAS_EXPORT RenderIntent calculateConfigIntent(
    const KisDisplayConfig::Options &options);

enum class ProtocolCommand {
    None,
    Set,
    Unset
};

struct KRITACANVAS_EXPORT NegotiationInput
{
    bool ready {false};
    KisCanvasSurfaceMode surfaceMode {KisCanvasSurfaceMode::Preferred};
    KisDisplayConfig::Options options;
    std::optional<SurfaceDescription> compositorPreferred;
    std::optional<SurfaceDescription> currentDescription;
    std::optional<RenderIntent> currentIntent;

    NegotiationInput();
};

struct KRITACANVAS_EXPORT NegotiationResult
{
    bool deferred {false};
    ProtocolCommand command {ProtocolCommand::None};
    std::optional<SurfaceDescription> requestedDescription;
    RenderIntent intent {RenderIntent::Perceptual};
    bool isCanvasHdr {false};
    bool hasProfile {false};
    QString errorMessage;
};

KRITACANVAS_EXPORT NegotiationResult negotiate(
    const NegotiationInput &input,
    const SupportsIntent &supportsIntent,
    const SupportsDescription &supportsDescription,
    const ProfileAvailable &profileAvailable);

} // namespace KisCanvasSurfaceColorSpacePolicy

#endif // KISCANVASSURFACECOLORSPACEPOLICY_H
