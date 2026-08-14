/*
 *  SPDX-FileCopyrightText: 2007 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisOpenGLPolicy.h"

#include <QRegularExpression>

namespace KisOpenGLPolicy
{

QString rendererToConfig(Renderer renderer)
{
    switch (renderer) {
    case Renderer::None:
        return QStringLiteral("none");
    case Renderer::Software:
        return QStringLiteral("software");
    case Renderer::DesktopGL:
        return QStringLiteral("desktop");
    case Renderer::OpenGLES:
        return QStringLiteral("angle");
    default:
        return QStringLiteral("auto");
    }
}

Renderer rendererFromConfig(const QString &renderer)
{
    if (renderer == QStringLiteral("desktop")) {
        return Renderer::DesktopGL;
    } else if (renderer == QStringLiteral("angle")) {
        return Renderer::OpenGLES;
    } else if (renderer == QStringLiteral("software")) {
        return Renderer::Software;
    } else if (renderer == QStringLiteral("none")) {
        return Renderer::None;
    }
    return Renderer::Auto;
}

SurfaceRequest surfaceRequest(Renderer renderer,
                              Platform platform,
                              bool inhibitCompatibilityProfile,
                              bool debugContext,
                              bool repaintDebugging)
{
    SurfaceRequest request;
    request.debugContext = debugContext;
    request.swapInterval = repaintDebugging ? 1 : 0;

    switch (renderer) {
    case Renderer::OpenGLES:
        request.angleRenderer = AngleRenderer::D3d11;
        request.majorVersion = 3;
        request.minorVersion = 0;
        request.profile = Profile::None;
        break;
    case Renderer::Software:
        request.angleRenderer = AngleRenderer::D3d11Warp;
        request.majorVersion = 3;
        request.minorVersion = 0;
        request.profile = Profile::None;
        break;
    case Renderer::DesktopGL:
        request.angleRenderer = AngleRenderer::Default;
        if (platform == Platform::MacOS) {
            request.majorVersion = 4;
            request.minorVersion = 1;
            request.profile = Profile::Core;
        } else {
            request.majorVersion = 3;
            request.minorVersion = 3;
            request.profile = inhibitCompatibilityProfile ? Profile::None : Profile::Compatibility;
            request.deprecatedFunctions = platform == Platform::Windows;
        }
        break;
    case Renderer::None:
        request.angleRenderer = AngleRenderer::Default;
        break;
    case Renderer::Auto:
        request.angleRenderer = AngleRenderer::D3d11;
        break;
    }

    return request;
}

QVector<ProbeRequest> defaultProbeSequence(Platform platform)
{
    QVector<ProbeRequest> sequence {{Renderer::Auto, false}};

    if (platform != Platform::MacOS) {
        sequence.append({Renderer::DesktopGL, false});
        sequence.append({Renderer::DesktopGL, true});
        sequence.append({Renderer::OpenGLES, true});
    }

    if (platform == Platform::Windows) {
        sequence.append({Renderer::Software, false});
    }

    return sequence;
}

QVector<Renderer> rendererCandidates(bool isAndroid, bool isWindows)
{
    QVector<Renderer> renderers;
    if (!isAndroid) {
        renderers.append(Renderer::DesktopGL);
    }
    renderers.append(Renderer::OpenGLES);
    if (isWindows) {
        renderers.append(Renderer::Software);
    }
    return renderers;
}

IntelDriverPolicy intelDriverPolicy(const QString &rendererString,
                                    const QString &driverVersionString,
                                    bool isWindows)
{
    IntelDriverPolicy result;
    if (!isWindows || !rendererString.startsWith(QStringLiteral("Intel"))) {
        return result;
    }

    const QRegularExpression regex(QStringLiteral("\\b\\d{1,2}\\.\\d{1,2}\\.(\\d{1,3})\\.(\\d{4})\\b"));
    const QRegularExpressionMatch match = regex.match(driverVersionString);
    if (!match.hasMatch()) {
        result.blacklisted = true;
        result.warning = IntelWarning::UnknownDriverFormat;
        return result;
    }

    const int thirdPart = match.captured(1).toInt();
    const int fourthPart = match.captured(2).toInt();
    result.driverBuild = thirdPart >= 100 ? thirdPart * 10000 + fourthPart : fourthPart;

    if ((result.driverBuild > 4636 && result.driverBuild < 4729) || result.driverBuild == 4358) {
        result.blacklisted = true;
        result.warning = IntelWarning::KnownBadDriver;
    }
    return result;
}

namespace
{
bool isHdr(ColorSpace colorSpace)
{
    return colorSpace == ColorSpace::Bt2020Pq || colorSpace == ColorSpace::ScRgb;
}

bool isFallbackOnly(Renderer renderer)
{
    return renderer == Renderer::Software;
}

bool isBlacklisted(Renderer renderer, const SelectionPreferences &preferences)
{
    return (renderer == Renderer::DesktopGL && preferences.desktopBlacklisted) ||
        ((renderer == Renderer::OpenGLES || renderer == Renderer::Software) &&
         preferences.openGlesBlacklisted);
}

bool orderBy(bool lhs, bool rhs, bool &decided)
{
    if (lhs != rhs) {
        decided = true;
        return lhs;
    }
    return false;
}
}

bool isPreferred(const FormatCandidate &lhs,
                 const FormatCandidate &rhs,
                 const SelectionPreferences &preferences)
{
    bool decided = false;
    if (preferences.preferredRendererByUser != Renderer::Software) {
        const bool result = orderBy(!isFallbackOnly(lhs.renderer), !isFallbackOnly(rhs.renderer), decided);
        if (decided) return result;
    }

    {
        const bool result = orderBy(lhs.colorSpace == preferences.preferredColorSpace,
                                    rhs.colorSpace == preferences.preferredColorSpace,
                                    decided);
        if (decided) return result;
    }

    const bool preferHdr = isHdr(preferences.preferredColorSpace);
    {
        const bool result = orderBy(preferHdr ? isHdr(lhs.colorSpace) : !isHdr(lhs.colorSpace),
                                    preferHdr ? isHdr(rhs.colorSpace) : !isHdr(rhs.colorSpace),
                                    decided);
        if (decided) return result;
    }

    if (preferences.preferredRendererByUser != Renderer::Auto) {
        const bool result = orderBy(lhs.renderer == preferences.preferredRendererByUser,
                                    rhs.renderer == preferences.preferredRendererByUser,
                                    decided);
        if (decided) return result;
    }

    {
        const bool result = orderBy(!isBlacklisted(lhs.renderer, preferences),
                                    !isBlacklisted(rhs.renderer, preferences),
                                    decided);
        if (decided) return result;
    }

    if (preferHdr && preferences.preferredRendererByHdr != Renderer::Auto) {
        const bool result = orderBy(lhs.renderer == preferences.preferredRendererByHdr,
                                    rhs.renderer == preferences.preferredRendererByHdr,
                                    decided);
        if (decided) return result;
    }

    {
        const bool result = orderBy(lhs.renderer == preferences.preferredRendererByQt,
                                    rhs.renderer == preferences.preferredRendererByQt,
                                    decided);
        if (decided) return result;
    }

    {
        const bool result = orderBy(lhs.redBufferBits == preferences.userPreferredBitDepth,
                                    rhs.redBufferBits == preferences.userPreferredBitDepth,
                                    decided);
        if (decided) return result;
    }

    return false;
}

bool needsFenceWorkaround(bool isOnX11, const QString &rendererString, bool forceWorkaround)
{
    return (isOnX11 && rendererString.startsWith(QStringLiteral("AMD"))) || forceWorkaround;
}

bool shouldUseTextureBuffers(bool forceDisabled, bool userPreference)
{
    return !forceDisabled && userPreference;
}

bool shouldInvalidateBuffers(bool configured, bool driverSupportsInvalidation)
{
    return configured && driverSupportsInvalidation;
}

bool rejectAngleD3d9(bool isWindows, bool isUsingAngle, const QString &rendererString)
{
    return isWindows && isUsingAngle &&
        rendererString.contains(QStringLiteral("Direct3D9"), Qt::CaseInsensitive);
}

} // namespace KisOpenGLPolicy
