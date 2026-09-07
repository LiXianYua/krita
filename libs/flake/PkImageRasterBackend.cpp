/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PkImageRasterBackend.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <vector>

#if defined(__SSE2__)
#include <emmintrin.h>
#include <xmmintrin.h>
#endif

namespace
{

constexpr unsigned alpha(uint32_t pixel)
{
    return pixel >> 24;
}

constexpr unsigned red(uint32_t pixel)
{
    return (pixel >> 16) & 0xffu;
}

constexpr unsigned green(uint32_t pixel)
{
    return (pixel >> 8) & 0xffu;
}

constexpr unsigned blue(uint32_t pixel)
{
    return pixel & 0xffu;
}

constexpr uint32_t argb(unsigned a, unsigned r, unsigned g, unsigned b)
{
    return (a << 24) | (r << 16) | (g << 8) | b;
}

unsigned divideBy65535(uint32_t value)
{
    return (value + (value >> 16) + 0x8000u) >> 16;
}

unsigned premultiply16(unsigned component, unsigned a)
{
    unsigned result = (component * a) >> 16;
    return result + (result >> 15);
}

struct Rgba64Pixel
{
    unsigned a;
    unsigned r;
    unsigned g;
    unsigned b;
};

Rgba64Pixel premultiply64(uint32_t pixel, bool forcePremultiply = false)
{
    Rgba64Pixel result {
        alpha(pixel) * 257u,
        red(pixel) * 257u,
        green(pixel) * 257u,
        blue(pixel) * 257u
    };
    if (forcePremultiply || result.a != 65535u) {
        // Qt's SSE4 ARGB32 -> RGBA64PM conversion uses unsigned multiply-high,
        // then maps 0xfffe back to 0xffff.
        result.r = premultiply16(result.r, result.a);
        result.g = premultiply16(result.g, result.a);
        result.b = premultiply16(result.b, result.a);
    }
    return result;
}

std::vector<Rgba64Pixel> premultiplySpan(const PkImage &image,
                                         int y,
                                         int x,
                                         int count)
{
    std::vector<Rgba64Pixel> result(static_cast<std::size_t>(count));
    // Qt's AVX2 ARGB32 fetch converts eight pixels together. If any valid
    // pixel in a group is not opaque, every pixel in that group follows the
    // multiply-high path, including opaque neighbors.
    for (int groupBegin = 0; groupBegin < count; groupBegin += 8) {
        const int groupEnd = std::min(count, groupBegin + 8);
        bool allTransparent = true;
        bool allOpaque = true;
        for (int i = groupBegin; i < groupEnd; ++i) {
            const unsigned pixelAlpha = alpha(image.pixel(x + i, y));
            allTransparent = allTransparent && pixelAlpha == 0;
            allOpaque = allOpaque && pixelAlpha == 255;
        }

        if (allTransparent) {
            continue;
        }
        for (int i = groupBegin; i < groupEnd; ++i) {
            result[static_cast<std::size_t>(i)] =
                premultiply64(image.pixel(x + i, y), !allOpaque);
        }
    }
    return result;
}

Rgba64Pixel multiply64(const Rgba64Pixel &pixel, unsigned factor)
{
    return {
        divideBy65535(pixel.a * factor),
        divideBy65535(pixel.r * factor),
        divideBy65535(pixel.g * factor),
        divideBy65535(pixel.b * factor)
    };
}

unsigned to8Bit(unsigned component)
{
    component += 128u;
    component -= component >> 8;
    return component >> 8;
}

unsigned unpremultiplyTo8(unsigned component, unsigned a)
{
    if (a == 0) {
        return 0;
    }
    if (a == 65535u) {
        return to8Bit(component);
    }

    // Qt's SSE4 RGBA64PM -> ARGB32 store converts directly to eight bits with
    // round-to-nearest-even. Avoiding an intermediate 16-bit rounding is
    // observable at low alpha and channel-boundary values.
#if defined(__SSE2__)
    const __m128 alphaVector = _mm_set1_ps(static_cast<float>(a));
    __m128 inverseAlpha = _mm_rcp_ps(alphaVector);
    inverseAlpha = _mm_sub_ps(
        _mm_add_ps(inverseAlpha, inverseAlpha),
        _mm_mul_ps(inverseAlpha, _mm_mul_ps(inverseAlpha, alphaVector)));
    inverseAlpha = _mm_mul_ps(inverseAlpha, _mm_set1_ps(255.0f));
    const __m128 value = _mm_mul_ps(
        _mm_set1_ps(static_cast<float>(component)), inverseAlpha);
    const int rounded = _mm_cvtsi128_si32(_mm_cvtps_epi32(value));
    return std::min(255u, static_cast<unsigned>(std::max(0, rounded)));
#else
    const float value = static_cast<float>(component) * 255.0f /
        static_cast<float>(a);
    return std::min(255u, static_cast<unsigned>(std::nearbyint(value)));
#endif
}

uint32_t sourceOver(const Rgba64Pixel &destinationPremultiplied,
                    const Rgba64Pixel &sourcePremultiplied,
                    unsigned opacity)
{
    const Rgba64Pixel scaledSource = multiply64(sourcePremultiplied, opacity * 257u);
    const Rgba64Pixel scaledDestination =
        multiply64(destinationPremultiplied, 65535u - scaledSource.a);
    const Rgba64Pixel result {
        scaledSource.a + scaledDestination.a,
        scaledSource.r + scaledDestination.r,
        scaledSource.g + scaledDestination.g,
        scaledSource.b + scaledDestination.b
    };

    return argb(to8Bit(result.a),
                unpremultiplyTo8(result.r, result.a),
                unpremultiplyTo8(result.g, result.a),
                unpremultiplyTo8(result.b, result.a));
}

bool isIntegralCoordinate(qreal value)
{
    return std::isfinite(value) && std::trunc(value) == value &&
        value >= std::numeric_limits<int>::min() &&
        value <= std::numeric_limits<int>::max();
}

} // namespace

PkImageRasterBackend::PkImageRasterBackend(PkImage &destination)
    : m_destination(destination)
{
}

void PkImageRasterBackend::submit(const PkPaintCommand &command)
{
    if (const auto *opacity = std::get_if<PkSetOpacityCommand>(&command)) {
        if (!std::isfinite(opacity->opacity)) {
            throw std::invalid_argument("PkImageRasterBackend opacity must be finite");
        }
        m_opacity = std::clamp(opacity->opacity, qreal(0.0), qreal(1.0));
        return;
    }
    if (const auto *image = std::get_if<PkDrawImageCommand>(&command)) {
        drawImage(*image);
        return;
    }

    throw std::logic_error("PkImageRasterBackend does not support this paint command");
}

void PkImageRasterBackend::drawImage(const PkDrawImageCommand &command)
{
    if (m_destination.format() != PkImage::Format_ARGB32 ||
        command.image.format() != PkImage::Format_ARGB32) {
        throw std::invalid_argument("PkImageRasterBackend requires ARGB32 images");
    }
    if (!isIntegralCoordinate(command.target.x()) ||
        !isIntegralCoordinate(command.target.y()) ||
        command.target.width() != command.image.width() ||
        command.target.height() != command.image.height()) {
        throw std::invalid_argument(
            "PkImageRasterBackend supports only integer-aligned 1:1 image draws");
    }

    const int targetX = static_cast<int>(command.target.x());
    const int targetY = static_cast<int>(command.target.y());
    // QRasterPaintEngine first truncates opacity to an 8.8 fixed-point value,
    // then combines it with a fully covered span to obtain its 0..255 alpha.
    const unsigned fixedOpacity = static_cast<unsigned>(m_opacity * 256.0);
    const unsigned opacity = (fixedOpacity * 255u) >> 8;

    for (int sourceY = 0; sourceY < command.image.height(); ++sourceY) {
        const long long destinationY = static_cast<long long>(targetY) + sourceY;
        if (destinationY < 0 || destinationY >= m_destination.height()) {
            continue;
        }

        const long long clippedSourceBegin = std::max(
            0LL, -static_cast<long long>(targetX));
        const long long clippedSourceEnd = std::min(
            static_cast<long long>(command.image.width()),
            static_cast<long long>(m_destination.width()) - targetX);
        if (clippedSourceBegin >= clippedSourceEnd) {
            continue;
        }

        const int sourceBegin = static_cast<int>(clippedSourceBegin);
        const int sourceEnd = static_cast<int>(clippedSourceEnd);
        const int destinationX = targetX + sourceBegin;
        const int y = static_cast<int>(destinationY);
        const int count = sourceEnd - sourceBegin;
        const auto sourcePixels =
            premultiplySpan(command.image, sourceY, sourceBegin, count);
        const auto destinationPixels =
            premultiplySpan(m_destination, y, destinationX, count);
        for (int i = 0; i < count; ++i) {
            m_destination.setPixel(
                destinationX + i,
                y,
                sourceOver(destinationPixels[static_cast<std::size_t>(i)],
                           sourcePixels[static_cast<std::size_t>(i)],
                           opacity));
        }
    }
}
