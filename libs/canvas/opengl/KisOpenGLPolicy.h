/*
 *  SPDX-FileCopyrightText: 2007 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISOPENGLPOLICY_H
#define KISOPENGLPOLICY_H

#include <kritacanvas_export.h>

#include <QString>
#include <QVector>

namespace KisOpenGLPolicy
{

enum class Renderer {
    None = 0x00,
    Auto = 0x01,
    DesktopGL = 0x02,
    OpenGLES = 0x04,
    Software = 0x08
};

enum class AngleRenderer {
    Default = 0x0000,
    D3d11 = 0x0002,
    D3d9 = 0x0004,
    D3d11Warp = 0x0008
};

enum class Platform {
    Other,
    Windows,
    MacOS
};

enum class Profile {
    None,
    Compatibility,
    Core
};

enum class ColorSpace {
    Default,
    SRgb,
    ScRgb,
    Bt2020Pq
};

struct KRITACANVAS_EXPORT SurfaceRequest
{
    int majorVersion {0};
    int minorVersion {0};
    Profile profile {Profile::None};
    AngleRenderer angleRenderer {AngleRenderer::Default};
    int depthBufferBits {24};
    int stencilBufferBits {8};
    int swapInterval {0};
    bool doubleBuffered {true};
    bool deprecatedFunctions {false};
    bool debugContext {false};
};

KRITACANVAS_EXPORT QString rendererToConfig(Renderer renderer);
KRITACANVAS_EXPORT Renderer rendererFromConfig(const QString &renderer);

KRITACANVAS_EXPORT SurfaceRequest surfaceRequest(Renderer renderer,
                                                 Platform platform,
                                                 bool inhibitCompatibilityProfile,
                                                 bool debugContext,
                                                 bool repaintDebugging);

struct KRITACANVAS_EXPORT ProbeRequest
{
    Renderer renderer {Renderer::Auto};
    bool inhibitCompatibilityProfile {false};

    bool operator==(const ProbeRequest &rhs) const
    {
        return renderer == rhs.renderer &&
            inhibitCompatibilityProfile == rhs.inhibitCompatibilityProfile;
    }
};

KRITACANVAS_EXPORT QVector<ProbeRequest> defaultProbeSequence(Platform platform);
KRITACANVAS_EXPORT QVector<Renderer> rendererCandidates(bool isAndroid, bool isWindows);

enum class IntelWarning {
    None,
    KnownBadDriver,
    UnknownDriverFormat
};

struct KRITACANVAS_EXPORT IntelDriverPolicy
{
    bool blacklisted {false};
    IntelWarning warning {IntelWarning::None};
    int driverBuild {-1};
};

KRITACANVAS_EXPORT IntelDriverPolicy intelDriverPolicy(const QString &rendererString,
                                                       const QString &driverVersionString,
                                                       bool isWindows);

struct KRITACANVAS_EXPORT FormatCandidate
{
    Renderer renderer {Renderer::Auto};
    ColorSpace colorSpace {ColorSpace::Default};
    int redBufferBits {8};
};

struct KRITACANVAS_EXPORT SelectionPreferences
{
    ColorSpace preferredColorSpace {ColorSpace::Default};
    Renderer preferredRendererByQt {Renderer::DesktopGL};
    Renderer preferredRendererByUser {Renderer::Auto};
    Renderer preferredRendererByHdr {Renderer::Auto};
    bool desktopBlacklisted {false};
    bool openGlesBlacklisted {false};
    int userPreferredBitDepth {8};
};

KRITACANVAS_EXPORT bool isPreferred(const FormatCandidate &lhs,
                                    const FormatCandidate &rhs,
                                    const SelectionPreferences &preferences);

KRITACANVAS_EXPORT bool needsFenceWorkaround(bool isOnX11,
                                             const QString &rendererString,
                                             bool forceWorkaround);
KRITACANVAS_EXPORT bool shouldUseTextureBuffers(bool forceDisabled, bool userPreference);
KRITACANVAS_EXPORT bool shouldInvalidateBuffers(bool configured, bool driverSupportsInvalidation);
KRITACANVAS_EXPORT bool rejectAngleD3d9(bool isWindows,
                                       bool isUsingAngle,
                                       const QString &rendererString);

} // namespace KisOpenGLPolicy

#endif // KISOPENGLPOLICY_H
