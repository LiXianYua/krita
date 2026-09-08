/*
 *  SPDX-FileCopyrightText: 2007 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <pk/global/PkGlobal.h>
#include "KisOpenGLPolicy.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>
#include <string>

namespace KisOpenGLPolicy
{

PkString rendererToConfig(Renderer renderer)
{
    switch (renderer) {
    case Renderer::None:
        return PkString("none");
    case Renderer::Software:
        return PkString("software");
    case Renderer::DesktopGL:
        return PkString("desktop");
    case Renderer::OpenGLES:
        return PkString("angle");
    default:
        return PkString("auto");
    }
}

Renderer rendererFromConfig(const PkString &renderer)
{
    if (renderer == PkString("desktop")) {
        return Renderer::DesktopGL;
    } else if (renderer == PkString("angle")) {
        return Renderer::OpenGLES;
    } else if (renderer == PkString("software")) {
        return Renderer::Software;
    } else if (renderer == PkString("none")) {
        return Renderer::None;
    }
    return Renderer::Auto;
}

SurfaceRequest surfaceRequest(Renderer renderer,
                              Platform platform,
                              bool inhibitCompatibilityProfile,
                              bool debugContext,
                              bool repaintDebugging,
                              Renderer defaultRenderer)
{
    SurfaceRequest request;
    request.debugContext = debugContext;
    request.swapInterval = repaintDebugging ? 1 : 0;

    if (renderer == Renderer::Auto) {
        renderer = defaultRenderer == Renderer::Auto ? Renderer::DesktopGL : defaultRenderer;
    }

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
        std::abort();
    }

    return request;
}

PkVector<ProbeRequest> defaultProbeSequence(Platform platform)
{
    PkVector<ProbeRequest> sequence {{Renderer::Auto, false}};

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

PkVector<Renderer> rendererCandidates(bool isAndroid, bool isWindows)
{
    PkVector<Renderer> renderers;
    if (!isAndroid) {
        renderers.append(Renderer::DesktopGL);
    }
    renderers.append(Renderer::OpenGLES);
    if (isWindows) {
        renderers.append(Renderer::Software);
    }
    return renderers;
}

IntelDriverPolicy intelDriverPolicy(const PkString &rendererString,
                                    const PkString &driverVersionString,
                                    bool isWindows)
{
    IntelDriverPolicy result;
    if (!isWindows || !rendererString.startsWith(PkString("Intel"))) {
        return result;
    }

    static const std::regex regex("\\b\\d{1,2}\\.\\d{1,2}\\.(\\d{1,3})\\.(\\d{4})\\b");
    std::smatch match;
    const std::string version = driverVersionString.PkToUtf8();
    if (!std::regex_search(version, match, regex)) {
        result.blacklisted = true;
        result.warning = IntelWarning::UnknownDriverFormat;
        return result;
    }

    const int thirdPart = std::stoi(match[1].str());
    const int fourthPart = std::stoi(match[2].str());
    result.driverBuild = thirdPart >= 100 ? thirdPart * 10000 + fourthPart : fourthPart;

    if ((result.driverBuild > 4636 && result.driverBuild < 4729) || result.driverBuild == 4358) {
        result.blacklisted = true;
        result.warning = IntelWarning::KnownBadDriver;
    }
    return result;
}

namespace
{
bool containsAsciiCaseInsensitive(const PkString &text, const char *needle)
{
    std::string haystack = text.PkToUtf8();
    std::string wanted(needle);
    const auto lower = [](unsigned char ch) { return static_cast<char>(std::tolower(ch)); };
    std::transform(haystack.begin(), haystack.end(), haystack.begin(), lower);
    std::transform(wanted.begin(), wanted.end(), wanted.begin(), lower);
    return haystack.find(wanted) != std::string::npos;
}

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

bool needsFenceWorkaround(bool isOnX11, const PkString &rendererString, bool forceWorkaround)
{
    return (isOnX11 && rendererString.startsWith(PkString("AMD"))) || forceWorkaround;
}

bool shouldUseTextureBuffers(bool forceDisabled, bool userPreference)
{
    return !forceDisabled && userPreference;
}

bool forceDisableTextureBuffers(Platform platform,
                                const PkString &rendererString,
                                bool unlockTextureBuffersEnvironmentSet)
{
    return platform == Platform::Windows &&
        !unlockTextureBuffersEnvironmentSet &&
        containsAsciiCaseInsensitive(rendererString, "ANGLE");
}

bool shouldInvalidateBuffers(bool configured, bool driverSupportsInvalidation)
{
    return configured && driverSupportsInvalidation;
}

int assistantPixmapCacheLimitKiB(int width, int height)
{
    const int minimumCacheSize = 20 * 1024;
    const int cacheSize = 2048 + 5 * 4 * width * height / 1024;
    return pkMax(minimumCacheSize, cacheSize);
}

bool rejectAngleD3d9(bool isWindows, bool isUsingAngle, const PkString &rendererString)
{
    return isWindows && isUsingAngle &&
        containsAsciiCaseInsensitive(rendererString, "Direct3D9");
}

} // namespace KisOpenGLPolicy
