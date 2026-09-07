/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PkImageRasterBackend.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

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

unsigned byteMultiply(unsigned value, unsigned factor)
{
    const unsigned product = value * factor;
    return (product + (product >> 8) + 0x80u) >> 8;
}

uint32_t premultiply(uint32_t pixel)
{
    const unsigned a = alpha(pixel);
    return argb(a,
                byteMultiply(red(pixel), a),
                byteMultiply(green(pixel), a),
                byteMultiply(blue(pixel), a));
}

unsigned unpremultiplyForArgb32(unsigned component, unsigned a)
{
    if (a == 0) {
        return 0;
    }
    if (a == 255) {
        return component;
    }

    // QPainter's ARGB32 raster store uses an 8-bit reciprocal. This differs
    // by one from qUnpremultiply() for some translucent pixels, so retain the
    // measured store rounding rather than routing through a generic formula.
    return std::min(255u, (component * 256u + a / 2u) / a);
}

uint32_t sourceOver(uint32_t destination, uint32_t source, unsigned opacity)
{
    const uint32_t sourcePremultiplied = premultiply(source);
    const uint32_t destinationPremultiplied = premultiply(destination);

    const unsigned sourceAlpha = byteMultiply(alpha(sourcePremultiplied), opacity);
    const unsigned inverseSourceAlpha = 255u - sourceAlpha;
    const unsigned resultAlpha = sourceAlpha +
        byteMultiply(alpha(destinationPremultiplied), inverseSourceAlpha);
    const unsigned resultRed =
        byteMultiply(red(sourcePremultiplied), opacity) +
        byteMultiply(red(destinationPremultiplied), inverseSourceAlpha);
    const unsigned resultGreen =
        byteMultiply(green(sourcePremultiplied), opacity) +
        byteMultiply(green(destinationPremultiplied), inverseSourceAlpha);
    const unsigned resultBlue =
        byteMultiply(blue(sourcePremultiplied), opacity) +
        byteMultiply(blue(destinationPremultiplied), inverseSourceAlpha);

    return argb(resultAlpha,
                unpremultiplyForArgb32(resultRed, resultAlpha),
                unpremultiplyForArgb32(resultGreen, resultAlpha),
                unpremultiplyForArgb32(resultBlue, resultAlpha));
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
    const unsigned opacity = static_cast<unsigned>(m_opacity * 255.0);

    for (int sourceY = 0; sourceY < command.image.height(); ++sourceY) {
        const long long destinationY = static_cast<long long>(targetY) + sourceY;
        if (destinationY < 0 || destinationY >= m_destination.height()) {
            continue;
        }
        for (int sourceX = 0; sourceX < command.image.width(); ++sourceX) {
            const long long destinationX = static_cast<long long>(targetX) + sourceX;
            if (destinationX < 0 || destinationX >= m_destination.width()) {
                continue;
            }

            const int x = static_cast<int>(destinationX);
            const int y = static_cast<int>(destinationY);
            m_destination.setPixel(
                x,
                y,
                sourceOver(m_destination.pixel(x, y),
                           command.image.pixel(sourceX, sourceY),
                           opacity));
        }
    }
}
