/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISCANVASSURFACECOLORSPACEPOLICY_H
#define KISCANVASSURFACECOLORSPACEPOLICY_H

#include <kritacanvas_export.h>

#include "KisMultiSurfaceStateManager.h"

#include <optional>
#include <variant>

#include <surfacecolormanagement/KisSurfaceColorimetry.h>

#include <functional>

namespace KisCanvasSurfaceColorSpacePolicy
{

using NamedPrimaries = KisSurfaceColorimetry::NamedPrimaries;
using NamedTransferFunction = KisSurfaceColorimetry::NamedTransferFunction;
using RenderIntent = KisSurfaceColorimetry::RenderIntent;
using Luminance = KisSurfaceColorimetry::Luminance;
using MasteringLuminance = KisSurfaceColorimetry::MasteringLuminance;
using ColorSpace = KisSurfaceColorimetry::ColorSpace;
using MasteringInfo = KisSurfaceColorimetry::MasteringInfo;
using SurfaceDescription = KisSurfaceColorimetry::SurfaceDescription;

using SupportsDescription = std::function<bool(const SurfaceDescription &)>;
using ProfileAvailable = std::function<bool(const SurfaceDescription &)>;
using SupportsIntent = std::function<bool(RenderIntent)>;

KRITACANVAS_EXPORT KisCanvasSurfaceMode surfaceModeFromConfig(const QString &value);
KRITACANVAS_EXPORT QString surfaceModeToConfig(KisCanvasSurfaceMode mode);

enum class ProfileSource {
    None,
    Generated,
    BuiltInSrgb
};

struct KRITACANVAS_EXPORT SelectionResult
{
    std::optional<SurfaceDescription> requestedDescription;
    ProfileSource profileSource {ProfileSource::None};
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
    RenderIntent intent {RenderIntent::render_intent_perceptual};
    bool isCanvasHdr {false};
    ProfileSource profileSource {ProfileSource::None};
    QString errorMessage;
};

KRITACANVAS_EXPORT NegotiationResult negotiate(
    const NegotiationInput &input,
    const SupportsIntent &supportsIntent,
    const SupportsDescription &supportsDescription,
    const ProfileAvailable &profileAvailable);

} // namespace KisCanvasSurfaceColorSpacePolicy

#endif // KISCANVASSURFACECOLORSPACEPOLICY_H
