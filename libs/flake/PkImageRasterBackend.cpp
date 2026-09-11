/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-FileCopyrightText: 2016 The Qt Company Ltd.
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PkImageRasterBackend.h"
#include "PkGrayRaster.h"
#include "PkAliasedRasterizer.h"
#include "PkFontRasterizer.h"
#include <PkStrokeOutline.h>
#include <PkPathClipper_p.h>
#include "PkCosmeticStroker.h"

#include <algorithm>
#include <array>
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

unsigned unpremultiplyTo8ScalarStore(unsigned component, unsigned a)
{
    if (a == 0) {
        return 0;
    }
    if (a == 65535u) {
        return to8Bit(component);
    }

    // The 1..3 pixel SSE4 store epilogue first unpremultiplies back to a
    // 16-bit component with a reciprocal scaled by 65535, then narrows that
    // component to eight bits. This has observably different rounding from
    // the four-pixel path's direct reciprocal scaled by 255.
#if defined(__SSE2__)
    const __m128 alphaVector = _mm_set_ss(static_cast<float>(a));
    __m128 inverseAlpha = _mm_rcp_ss(alphaVector);
    inverseAlpha = _mm_sub_ss(
        _mm_add_ss(inverseAlpha, inverseAlpha),
        _mm_mul_ss(inverseAlpha, _mm_mul_ss(inverseAlpha, alphaVector)));
    inverseAlpha = _mm_mul_ss(inverseAlpha, _mm_set_ss(65535.0f));
    const __m128 value = _mm_mul_ss(
        _mm_set_ss(static_cast<float>(component)), inverseAlpha);
    const int rounded = _mm_cvtss_si32(value);
    return to8Bit(std::min(65535u,
                           static_cast<unsigned>(std::max(0, rounded))));
#else
    const float value = static_cast<float>(component) * 65535.0f /
        static_cast<float>(a);
    const int rounded = static_cast<int>(std::nearbyint(value));
    return to8Bit(std::min(65535u,
                           static_cast<unsigned>(std::max(0, rounded))));
#endif
}

uint32_t compose(const Rgba64Pixel &destinationPremultiplied,
                    const Rgba64Pixel &sourcePremultiplied,
                    unsigned opacity,
                    bool scalarStore,
                    Pk::CompositionMode mode)
{
    Rgba64Pixel result;
    if (mode == Pk::CompositionMode_Plus) {
        // Plus saturates in premultiplied space BEFORE applying coverage /
        // painter opacity. Interpolating the saturated sum with destination
        // is observably different from adding an opacity-scaled source.
        const Rgba64Pixel sum {
            std::min(65535u, sourcePremultiplied.a + destinationPremultiplied.a),
            std::min(65535u, sourcePremultiplied.r + destinationPremultiplied.r),
            std::min(65535u, sourcePremultiplied.g + destinationPremultiplied.g),
            std::min(65535u, sourcePremultiplied.b + destinationPremultiplied.b)
        };
        const auto source = multiply64(sum, opacity * 257u);
        const auto destination = multiply64(destinationPremultiplied, (255u - opacity) * 257u);
        result = {source.a + destination.a, source.r + destination.r,
                  source.g + destination.g, source.b + destination.b};
    } else if (mode == Pk::CompositionMode_Source) {
        const auto source = multiply64(sourcePremultiplied, opacity * 257u);
        const auto destination = multiply64(destinationPremultiplied, (255u - opacity) * 257u);
        result = {source.a + destination.a, source.r + destination.r,
                  source.g + destination.g, source.b + destination.b};
    } else {
        const Rgba64Pixel scaledSource = multiply64(sourcePremultiplied, opacity * 257u);
        const Rgba64Pixel scaledDestination =
            multiply64(destinationPremultiplied, 65535u - scaledSource.a);
        result = {
            scaledSource.a + scaledDestination.a,
            scaledSource.r + scaledDestination.r,
            scaledSource.g + scaledDestination.g,
            scaledSource.b + scaledDestination.b
        };
    }

    const auto storeComponent = scalarStore ?
        unpremultiplyTo8ScalarStore : unpremultiplyTo8;
    return argb(to8Bit(result.a),
                storeComponent(result.r, result.a),
                storeComponent(result.g, result.a),
                storeComponent(result.b, result.a));
}

bool isIntegralCoordinate(qreal value)
{
    return std::isfinite(value) && std::trunc(value) == value &&
        value >= std::numeric_limits<int>::min() &&
        value <= std::numeric_limits<int>::max();
}

unsigned multiply8(unsigned value, unsigned factor)
{
    const unsigned product = value * factor + 128;
    return (product + (product >> 8)) >> 8;
}

bool uses32BitComposition(PkImage::Format format)
{
    // Qt's generic Grayscale8 destination dispatch uses ARGB32PM spans.
    return format==PkImage::Format_ARGB32_Premultiplied || format==PkImage::Format_Grayscale8;
}

void storeComposedPixel(PkImage &image, int x, int y, uint32_t pixel)
{
    // Qt destStore selects storeFromRGB32 for an opaque destination, including
    // Grayscale8. Its qGray store consumes the composed channels directly,
    // even when CompositionMode_Source produced a translucent PM pixel.
    image.setPixel(x,y,pixel);
}

uint32_t composeSolid(uint32_t destination, uint32_t premultipliedSource, unsigned coverage,
                      Pk::CompositionMode mode, bool premultipliedDestination = false, bool solid = true)
{
    const unsigned sa = alpha(premultipliedSource);
    const unsigned da = alpha(destination);
    unsigned s[] = {sa, red(premultipliedSource), green(premultipliedSource), blue(premultipliedSource)};
    unsigned d[] = {da, multiply8(red(destination), da), multiply8(green(destination), da), multiply8(blue(destination), da)};
    if (premultipliedDestination) {
        d[1] = red(destination); d[2] = green(destination); d[3] = blue(destination);
    }
    unsigned result[4];
    for (int i = 0; i < 4; ++i) {
        if (mode == Pk::CompositionMode_Plus) {
            if (premultipliedDestination) {
                const unsigned value = std::min(255u, s[i] + d[i]) * coverage + d[i] * (255 - coverage);
                result[i] = (value + (value >> 8) + 128) >> 8;
                continue;
            }
            result[i] = multiply8(std::min(255u, s[i] + d[i]), coverage) +
                multiply8(d[i], 255 - coverage);
        } else if (mode == Pk::CompositionMode_Source) {
            if (premultipliedDestination && solid) {
                result[i] = multiply8(s[i], coverage) + multiply8(d[i], 255 - coverage);
                continue;
            }
            // Qt's 32-bit interpolation rounds the combined numerator once.
            const unsigned value = s[i] * coverage + d[i] * (255 - coverage);
            result[i] = (value + (value >> 8) + 128) >> 8;
        } else {
            result[i] = multiply8(s[i], coverage) + multiply8(d[i], 255 - multiply8(sa, coverage));
        }
    }
    if (premultipliedDestination) return argb(result[0], result[1], result[2], result[3]);
    if (result[0] == 0) return 0;
    return argb(result[0], unpremultiplyTo8(result[1], result[0]),
                unpremultiplyTo8(result[2], result[0]), unpremultiplyTo8(result[3], result[0]));
}

std::array<Rgba64Pixel, 1024> gradientTable(const PkGradient &gradient, unsigned opacity)
{
    const auto stops = gradient.stops();
    const bool component = gradient.interpolationMode() == PkGradient::ComponentInterpolation;
    const auto premultiply = [](Rgba64Pixel value) {
        return Rgba64Pixel {value.a, divideBy65535(value.r * value.a),
            divideBy65535(value.g * value.a), divideBy65535(value.b * value.a)};
    };
    const auto output = [component, premultiply](Rgba64Pixel value) {
        return component ? premultiply(value) : value;
    };
    const auto color = [opacity, component, premultiply](const PkColor &value) {
        const unsigned a = (unsigned(std::round(value.alphaF() * 65535)) * opacity) >> 8;
        const Rgba64Pixel result {a, unsigned(std::round(value.redF() * 65535)),
            unsigned(std::round(value.greenF() * 65535)), unsigned(std::round(value.blueF() * 65535))};
        return component ? result : premultiply(result);
    };
    std::array<Rgba64Pixel, 1024> table;
    if (stops.size() == 1) {
        table.fill(output(color(stops[0].color)));
        return table;
    }
    if (stops.size() > 2) {
        const double increment = 1.0 / 1024;
        double position = 1.5 * increment;
        int index = 0;
        table[index++] = output(color(stops[0].color));
        while (position <= stops[0].offset && index < 1024) {
            table[index] = table[index - 1];
            ++index;
            position += increment;
        }
        int segment = 0;
        double t = 0, delta = 0;
        bool newSegment = true;
        while (position < stops.last().offset && index < 1024) {
            while (position > stops[segment + 1].offset) { ++segment; newSegment = true; }
            if (newSegment) {
                const double distance = stops[segment + 1].offset - stops[segment].offset;
                const double factor = distance == 0 ? 0 : 256 / distance;
                t = (position - stops[segment].offset) * factor;
                delta = increment * factor;
                newSegment = false;
            }
            const auto first = color(stops[segment].color), last = color(stops[segment + 1].color);
            const int amount = pkRound(t), inverse = 256 - amount;
            const auto mix = [amount, inverse](unsigned a, unsigned b) {
                return ((a * inverse) >> 8) + ((b * amount) >> 8);
            };
            table[index++] = output({mix(first.a, last.a), mix(first.r, last.r),
                              mix(first.g, last.g), mix(first.b, last.b)});
            position += increment;
            t += delta;
        }
        const auto last = output(color(stops.last().color));
        while (index < 1024) table[index++] = last;
        return table;
    }
    const auto first = color(stops[0].color), last = color(stops[1].color);
    const int firstIndex = pkRound(stops[0].offset * 1023);
    const int lastIndex = pkRound(stops[1].offset * 1023);
    int index = 0;
    for (; index <= firstIndex && index < 1024; ++index) table[index] = output(first);
    if (index < lastIndex) {
        const double reciprocal = 1.0 / (lastIndex - firstIndex);
        uint32_t channels[] = {first.a << 16, first.r << 16, first.g << 16, first.b << 16};
        const unsigned ends[] = {last.a, last.r, last.g, last.b};
        int delta[4];
        for (int j = 0; j < 4; ++j) {
            delta[j] = pkRound((double(uint32_t(ends[j] << 16)) - channels[j]) * reciprocal);
            channels[j] += 1 << 15;
        }
        for (; index < lastIndex && index < 1024; ++index) {
            for (int j = 0; j < 4; ++j) channels[j] += delta[j];
            table[index] = output({channels[0] >> 16, channels[1] >> 16, channels[2] >> 16, channels[3] >> 16});
        }
    }
    for (; index < 1024; ++index) table[index] = output(last);
    return table;
}

int gradientIndex(int index, PkGradient::Spread spread)
{
    if (spread == PkGradient::RepeatSpread) {
        index %= 1024;
        if (index < 0) index += 1024;
    } else if (spread == PkGradient::ReflectSpread) {
        index %= 2048;
        if (index < 0) index += 2048;
        if (index >= 1024) index = 2047 - index;
    } else {
        index = std::clamp(index, 0, 1023);
    }
    return index;
}

bool scaleForTransform(const PkTransform &transform, qreal *scale)
{
    const auto type = transform.type();
    if (type <= PkTransform::TxTranslate) { *scale = 1; return true; }
    if (type == PkTransform::TxScale) {
        const auto x = std::abs(transform.m11()), y = std::abs(transform.m22());
        *scale = std::max(x, y);
        return pkQtFuzzyCompare(x, y);
    }
    const double x1 = transform.m11() * transform.m11() + transform.m21() * transform.m21();
    const double y1 = transform.m12() * transform.m12() + transform.m22() * transform.m22();
    const double x2 = transform.m11() * transform.m11() + transform.m12() * transform.m12();
    const double y2 = transform.m21() * transform.m21() + transform.m22() * transform.m22();
    if (std::abs(x1 - y1) > std::abs(x2 - y2)) {
        *scale = std::sqrt(std::max(x1, y1));
        return type == PkTransform::TxRotate && pkQtFuzzyCompare(x1, y1);
    }
    *scale = std::sqrt(std::max(x2, y2));
    return type == PkTransform::TxRotate && pkQtFuzzyCompare(x2, y2);
}

} // namespace

PkImageRasterBackend::PkImageRasterBackend(PkImage &destination)
    : m_destination(destination)
{
}

qreal PkImageRasterBackend::devicePixelRatio() const
{
    return m_destination.devicePixelRatio();
}

void PkImageRasterBackend::submit(const PkPaintCommand &command)
{
    // Like an inactive painter on a null image, an empty mask buffer accepts
    // drawing/state commands without validating a nonexistent pixel format.
    if (m_destination.isNull()) return;
    if (const auto *pen = std::get_if<PkSetPenCommand>(&command)) {
        m_state.pen = pen->pen;
        return;
    }
    if (const auto *brush = std::get_if<PkSetBrushCommand>(&command)) {
        m_state.brush = brush->brush;
        return;
    }
    if (const auto *font = std::get_if<PkSetFontCommand>(&command)) {
        // Pure state: qfont.h's setFont has no rasterisation of its own, and
        // the facade's save()/restore() rolls the font back, so the backend
        // must track it to keep the state contract identical.
        m_state.font = font->font;
        return;
    }
    if (const auto *path = std::get_if<PkDrawPathCommand>(&command)) {
        fillPath(path->path, m_state.brush);
        strokePath(path->path, m_state.pen);
        return;
    }
    if (const auto *rect = std::get_if<PkDrawRectCommand>(&command)) {
        PkPainterPath path;
        path.addRect(rect->rect);
        fillMask(path, m_state.brush, rectangleCoverage(rect->rect));
        strokePath(path, m_state.pen);
        return;
    }
    if (const auto *ellipse = std::get_if<PkDrawEllipseCommand>(&command)) {
        // QPainter::drawEllipse is pixel-identical to filling and stroking an
        // addEllipse path (measured against Qt 5.15.7: integer, non-integer,
        // out-of-bounds, negative-size and degenerate rects all agree).
        PkPainterPath path;
        path.addEllipse(ellipse->rect);
        fillPath(path, m_state.brush);
        strokePath(path, m_state.pen);
        return;
    }
    if (const auto *polygon = std::get_if<PkDrawPolygonCommand>(&command)) {
        // QPainter::drawPolygon closes the subpath before stroking; addPolygon
        // produces an open one. Dropping the closeSubpath() below changes 74 / 76
        // / 106 pixels respectively for the 4-vertex, self-intersecting and
        // explicitly-closed quads (metric and cases: pk/render/oracle/
        // shape_primitive_cases.h, 32x32 ARGB32, per-pixel packed-value compare).
        // Filling is unaffected either way (a fill implicitly closes the contour).
        PkPainterPath path;
        path.addPolygon(polygon->polygon);
        path.closeSubpath();
        fillPath(path, m_state.brush);
        strokePath(path, m_state.pen);
        return;
    }
    if (const auto *line = std::get_if<PkDrawLineCommand>(&command)) {
        PkPainterPath path(line->line.p1());
        path.lineTo(line->line.p2());
        strokePath(path, m_state.pen);
        return;
    }
    if (const auto *point = std::get_if<PkDrawPointCommand>(&command)) {
        strokePath(PkPainterPath(point->point), m_state.pen, true);
        return;
    }
    if (const auto *stroke = std::get_if<PkStrokePathCommand>(&command)) {
        strokePath(stroke->path, stroke->pen);
        return;
    }
    if (const auto *hint = std::get_if<PkSetRenderHintCommand>(&command)) {
        if (hint->enabled) m_state.hints |= hint->hint;
        else m_state.hints &= ~hint->hint;
        return;
    }
    if (const auto *transform = std::get_if<PkSetTransformCommand>(&command)) {
        m_state.transform = transform->combine ?
            transform->transform * m_state.transform : transform->transform;
        return;
    }
    if (const auto *clip = std::get_if<PkSetClipPathCommand>(&command)) {
        setClip(clip->path, clip->operation);
        return;
    }
    if (const auto *clip = std::get_if<PkSetClipRectCommand>(&command)) {
        PkPainterPath path;
        if (m_state.transform.type() <= PkTransform::TxScale) {
            // The raster engine optimizes vector rectangle clips to their
            // aligned integer bounds, even when antialiasing is enabled.
            path.addRect(PkRectF(m_state.transform.mapRect(clip->rect).toAlignedRect()));
            const auto savedTransform = m_state.transform;
            m_state.transform = PkTransform();
            setClip(path, clip->operation);
            m_state.transform = savedTransform;
            return;
        }
        path.addRect(clip->rect);
        setClip(path, clip->operation);
        return;
    }
    if (const auto *fill = std::get_if<PkFillPathCommand>(&command)) {
        fillPath(fill->path, fill->brush);
        return;
    }
    if (const auto *fill = std::get_if<PkFillTexturePathCommand>(&command)) {
        PkTransform placement = fill->transform;
        placement.scale(1.0 / fill->image.devicePixelRatio(), 1.0 / fill->image.devicePixelRatio());
        renderImage(fill->image, coverage(fill->path), placement, PkRectF(fill->image.rect()), true);
        return;
    }
    if (const auto *fill = std::get_if<PkFillRectCommand>(&command)) {
        PkPainterPath path;
        path.addRect(fill->rect);
        fillPath(path, fill->brush, true);
        return;
    }
    if (std::holds_alternative<PkSaveCommand>(command)) {
        m_stack.push_back(m_state);
        return;
    }
    if (std::holds_alternative<PkRestoreCommand>(command)) {
        if (!m_stack.empty()) {
            m_state = m_stack.back();
            m_stack.pop_back();
        }
        return;
    }
    if (const auto *composition = std::get_if<PkSetCompositionModeCommand>(&command)) {
        if (composition->mode != Pk::CompositionMode_SourceOver &&
            composition->mode != Pk::CompositionMode_Source &&
            composition->mode != Pk::CompositionMode_Plus) {
            throw std::logic_error("PkImageRasterBackend unsupported composition mode");
        }
        m_state.mode = composition->mode;
        return;
    }
    if (const auto *opacity = std::get_if<PkSetOpacityCommand>(&command)) {
        if (!std::isfinite(opacity->opacity)) {
            throw std::invalid_argument("PkImageRasterBackend opacity must be finite");
        }
        m_state.opacity = std::clamp(opacity->opacity, qreal(0.0), qreal(1.0));
        return;
    }
    if (const auto *image = std::get_if<PkDrawImageCommand>(&command)) {
        drawImage(*image);
        return;
    }
    if (const auto *image = std::get_if<PkDrawPixmapCommand>(&command)) {
        drawTransformedImage({image->target, image->image}, image->source);
        return;
    }
    if (const auto *image = std::get_if<PkDrawTiledPixmapCommand>(&command)) {
        if (image->image.isNull()) return;
        // QPainter rounds the requested tile phase to whole source pixels
        // before passing it to the raster engine (also for transformed tiles).
        const auto phase = [](qreal value, int period) {
            return value < 0 ? period - int(std::round(-value)) % period : int(std::round(value)) % period;
        };
        drawTransformedImage({image->rect, image->image},
                             PkRectF(PkPointF(phase(image->offset.x(), image->image.width()),
                                              phase(image->offset.y(), image->image.height())),
                                     PkSizeF(image->rect.width(), image->rect.height())), true);
        return;
    }

    if (const auto *text = std::get_if<PkDrawTextAtPointCommand>(&command)) {
        drawText(*text);
        return;
    }
    if (std::holds_alternative<PkDrawTextInRectCommand>(command)) {
        // Deliberately left unimplemented. The command carries only
        // { PkRectF rect; PkString text; } with no alignment / word-wrap flags,
        // so it cannot express Qt's three-argument rect + flags overload; it
        // also has zero live call sites. Registered as an accepted deviation in
        // R-55 plan §1.3 / §6 item 2, alongside R-51's fillRule-less drawPolygon.
        throw std::logic_error("PkImageRasterBackend does not support this paint command");
    }

    throw std::logic_error("PkImageRasterBackend does not support this paint command");
}

std::vector<unsigned char> PkImageRasterBackend::coverage(const PkPainterPath &path) const
{
    const int width = m_destination.width();
    const int height = m_destination.height();
    if (width > 32767 || height > 32767) {
        throw std::invalid_argument("PkImageRasterBackend raster dimensions exceed span range");
    }
    std::vector<unsigned char> pixels(static_cast<std::size_t>(width) * height, 0);
    if (path.isEmpty() || width <= 0 || height <= 0) return pixels;
    // QOutlineMapper flattens cubics in logical coordinates, at 0.25 / scale,
    // before the 26.6 transform. Passing raw cubics to the gray raster has a
    // different subdivision and produces observably different coverage.
    PkPainterPath flattened;
    flattened.setFillRule(path.fillRule());
    double scale;
    scaleForTransform(m_state.transform, &scale);
    const double threshold = scale == 0 ? 0.25 : 0.25 / scale;
    const auto flatten = [&](auto &&self, PkPointF a, PkPointF b, PkPointF c, PkPointF d, int level) -> void {
        const double dx = d.x() - a.x(), dy = d.y() - a.y();
        double length = std::abs(dx) + std::abs(dy);
        double distance;
        if (length > 1) {
            distance = std::abs(dx * (a.y() - b.y()) - dy * (a.x() - b.x())) +
                std::abs(dx * (a.y() - c.y()) - dy * (a.x() - c.x()));
        } else {
            distance = std::abs(a.x() - b.x()) + std::abs(a.y() - b.y()) +
                std::abs(a.x() - c.x()) + std::abs(a.y() - c.y());
            length = 1;
        }
        if (distance < threshold * length || level == 0) {
            flattened.lineTo(d);
            return;
        }
        const auto ab = (a + b) * 0.5, bc = (b + c) * 0.5, cd = (c + d) * 0.5;
        const auto abc = (ab + bc) * 0.5, bcd = (bc + cd) * 0.5;
        const auto middle = (abc + bcd) * 0.5;
        self(self, a, ab, abc, middle, level - 1);
        self(self, middle, bcd, cd, d, level - 1);
    };
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto element = path.elementAt(i);
        if (element.isMoveTo()) {
            if (!flattened.isEmpty()) flattened.closeSubpath();
            flattened.moveTo(element);
        } else if (element.isLineTo()) {
            flattened.lineTo(element);
        } else if (element.isCurveTo()) {
            flatten(flatten, flattened.currentPosition(), element,
                    path.elementAt(i + 1), path.elementAt(i + 2), 9);
            i += 2;
        }
    }
    flattened.closeSubpath();
    PkPainterPath mapped = m_state.transform.map(flattened);
    bool needsDeviceClip = false;
    for (int i = 0; i < mapped.elementCount(); ++i) {
        const auto element = mapped.elementAt(i);
        if (!std::isfinite(element.x) || !std::isfinite(element.y)) {
            throw std::invalid_argument("PkImageRasterBackend path contains a non-finite coordinate");
        }
        needsDeviceClip = needsDeviceClip
            || std::abs(element.x) > 32767
            || std::abs(element.y) > 32767;
    }
    if (needsDeviceClip) {
        // The span rasterizer clips in device space, but its input uses 26.6
        // fixed point. Keep representable subpaths byte-for-byte unchanged,
        // discard distant ones, and restrict only crossing subpaths to the
        // destination plus one AA guard pixel before converting them.
        const PkRectF deviceClip(-1, -1, width + 2, height + 2);
        PkPainterPath restricted;
        restricted.setFillRule(mapped.fillRule());
        PkPainterPath subpath;
        const auto appendRestricted = [&]() {
            if (subpath.isEmpty())
                return;
            bool subpathNeedsClip = false;
            for (int i = 0; i < subpath.elementCount(); ++i) {
                const auto element = subpath.elementAt(i);
                subpathNeedsClip = subpathNeedsClip
                    || std::abs(element.x) > 32767
                    || std::abs(element.y) > 32767;
            }
            if (!subpathNeedsClip) {
                restricted.addPath(subpath);
            } else if (subpath.controlPointRect().intersects(deviceClip)) {
                restricted.addPath(PkPathClipper::intersect(subpath, deviceClip));
            }
        };
        for (int i = 0; i < mapped.elementCount(); ++i) {
            const auto element = mapped.elementAt(i);
            if (element.isMoveTo()) {
                appendRestricted();
                subpath = PkPainterPath();
                subpath.setFillRule(mapped.fillRule());
                subpath.moveTo(element);
            } else {
                subpath.lineTo(element);
            }
        }
        appendRestricted();
        mapped = std::move(restricted);
    }
    std::vector<PK_FT_Vector> points;
    std::vector<char> tags;
    std::vector<int> contours;
    for (int i = 0; i < mapped.elementCount(); ++i) {
        const auto element = mapped.elementAt(i);
        if (std::abs(element.x) > 32767 || std::abs(element.y) > 32767) {
            throw std::invalid_argument("PkImageRasterBackend path exceeds fixed-point range");
        }
        if (element.isMoveTo() && !points.empty()) contours.push_back(points.size() - 1);
        // QOutlineMapper rounds transformed positions to 26.6 fixed point.
        points.push_back({pkRound(element.x * 64), pkRound(element.y * 64)});
        if (element.isCurveTo()) {
            tags.push_back(PK_FT_CURVE_TAG_CUBIC);
            for (int j = 0; j < 2; ++j) {
                const auto next = mapped.elementAt(++i);
                points.push_back({pkRound(next.x * 64), pkRound(next.y * 64)});
                tags.push_back(j == 0 ? PK_FT_CURVE_TAG_CUBIC : PK_FT_CURVE_TAG_ON);
            }
        } else {
            tags.push_back(PK_FT_CURVE_TAG_ON);
        }
    }
    contours.push_back(points.size() - 1);
    PK_FT_Outline outline {static_cast<int>(contours.size()), static_cast<int>(points.size()),
        points.data(), tags.data(), contours.data(),
        path.fillRule() == Pk::OddEvenFill ? PK_FT_OUTLINE_EVEN_ODD_FILL : 0};
    struct Target { int width; unsigned char *pixels; } target {width, pixels.data()};
    PK_FT_Raster_Params params {};
    params.source = &outline;
    params.flags = PK_FT_RASTER_FLAG_AA | PK_FT_RASTER_FLAG_DIRECT | PK_FT_RASTER_FLAG_CLIP;
    params.clip_box = {0, 0, width, height};
    params.user = &target;
    params.gray_spans = [](int count, const PK_FT_Span *spans, void *context) {
        const auto &target = *static_cast<Target *>(context);
        for (int i = 0; i < count; ++i) {
            const auto &span = spans[i];
            std::fill_n(target.pixels + static_cast<std::size_t>(span.y) * target.width + span.x,
                        span.len, span.coverage);
        }
    };
    if (!(m_state.hints & 1u)) {
        PkAliasedRasterizer raster;
        raster.setClipRect(PkRect(0, 0, width, height));
        raster.initialize(params.gray_spans, params.user);
        raster.rasterize(&outline, path.fillRule());
        return pixels;
    }
    // Each call owns its raster pool, so independent image painters are safe.
    std::vector<std::max_align_t> pool(65536 / sizeof(std::max_align_t));
    PK_FT_Raster raster {};
    if (pk_ft_grays_raster.raster_new(&raster) != 0) throw std::bad_alloc();
    pk_ft_grays_raster.raster_reset(raster, reinterpret_cast<unsigned char *>(pool.data()),
                                   pool.size() * sizeof(std::max_align_t));
    const int error = pk_ft_grays_raster.raster_render(raster, &params);
    pk_ft_grays_raster.raster_done(raster);
    if (error) throw std::runtime_error("PkImageRasterBackend path rasterization failed");
    return pixels;
}

void PkImageRasterBackend::setClip(const PkPainterPath &path, Pk::ClipOperation operation)
{
    if (operation == Pk::NoClip) {
        m_state.hasClip = false;
        m_state.clip.clear();
        return;
    }
    auto mask = coverage(path);
    if (operation == Pk::IntersectClip && m_state.hasClip) {
        for (std::size_t i = 0; i < mask.size(); ++i) {
            mask[i] = (unsigned(mask[i]) * m_state.clip[i] + 127) / 255;
        }
    }
    m_state.clip = std::move(mask);
    m_state.hasClip = true;
}

std::vector<unsigned char> PkImageRasterBackend::rectangleCoverage(const PkRectF &rect) const
{
    double scale;
    if (!scaleForTransform(m_state.transform, &scale)) {
        PkPainterPath path; path.addRect(rect); return coverage(path);
    }
    std::vector<unsigned char> pixels(static_cast<std::size_t>(m_destination.width()) * m_destination.height());
    if (rect.isEmpty() || m_destination.isNull()) return pixels;
    struct Target { int width; unsigned char *pixels; } target {m_destination.width(), pixels.data()};
    PkAliasedRasterizer raster;
    raster.setAntialiased(m_state.hints & 1u);
    PkRect rasterClip=m_destination.rect();
    if (m_state.hasClip) {
        int left=m_destination.width(),top=m_destination.height(),right=-1,bottom=-1;
        for (int y=0;y<m_destination.height();++y) for (int x=0;x<m_destination.width();++x) {
            if (!m_state.clip[static_cast<std::size_t>(y)*m_destination.width()+x]) continue;
            left=std::min(left,x); top=std::min(top,y); right=std::max(right,x); bottom=std::max(bottom,y);
        }
        if (right<left) return pixels;
        rasterClip=PkRect(left,top,right-left+1,bottom-top+1);
    }
    // initializeRasterizer in Qt uses the clip's span bounds. Clipping the
    // line earlier changes its fixed-point stepping at partially covered edges.
    raster.setClipRect(rasterClip);
    raster.initialize([](int count, const PK_FT_Span *spans, void *data) {
        const auto &target = *static_cast<Target *>(data);
        for (int i = 0; i < count; ++i) {
            const auto &span = spans[i];
            std::fill_n(target.pixels + static_cast<std::size_t>(span.y) * target.width + span.x,
                        span.len, span.coverage);
        }
    }, &target);
    const auto bounds = rect.normalized();
    const auto a = m_state.transform.map((bounds.topLeft() + bounds.bottomLeft()) * 0.5);
    const auto b = m_state.transform.map((bounds.topRight() + bounds.bottomRight()) * 0.5);
    raster.rasterizeLine(a, b, bounds.height() / bounds.width());
    return pixels;
}

void PkImageRasterBackend::fillPath(const PkPainterPath &path, const PkBrush &brush, bool rectangle)
{
    if (brush.style() == Pk::NoBrush) return;
    if (m_destination.format() != PkImage::Format_ARGB32 &&
        !uses32BitComposition(m_destination.format())) {
        throw std::invalid_argument("PkImageRasterBackend requires ARGB32 or Grayscale8 destination");
    }
    auto mask = coverage(path);
    if (rectangle && m_state.transform.type() <= PkTransform::TxScale) {
        const auto bounds = m_state.transform.mapRect(path.boundingRect()).normalized();
        for (int y = 0; y < m_destination.height(); ++y) {
            const auto height = std::max(0.0, std::min(y + 1.0, bounds.bottom()) - std::max(double(y), bounds.top()));
            for (int x = 0; x < m_destination.width(); ++x) {
                const auto width = std::max(0.0, std::min(x + 1.0, bounds.right()) - std::max(double(x), bounds.left()));
                const auto horizontal = static_cast<long long>(width * 65536) * 255;
                const auto vertical = static_cast<long long>(height * 65536);
                unsigned amount = (horizontal * vertical) >> 32;
                if (!(m_state.hints & 1u)) {
                    amount = x >= pkRound(bounds.left()) && x < pkRound(bounds.right()) &&
                        y >= pkRound(bounds.top()) && y < pkRound(bounds.bottom()) ? 255 : 0;
                }
                mask[static_cast<std::size_t>(y) * m_destination.width() + x] = amount;
            }
        }
    }
    fillMask(path, brush, mask);
}

void PkImageRasterBackend::fillMask(const PkPainterPath &path, const PkBrush &brush,
                                   const std::vector<unsigned char> &mask)
{
    if (brush.style() == Pk::NoBrush) return;
    const PkGradient *gradient = brush.gradient();
    if (brush.style() >= Pk::Dense1Pattern && brush.style() <= Pk::DiagCrossPattern) {
        // Qt 5.15 qbrush.cpp::qt_patternForBrush(invert=true), MonoLSB.
        static constexpr unsigned char patterns[][8] = {
            {0xff,0xbb,0xff,0xff,0xff,0xbb,0xff,0xff},
            {0x77,0xff,0xdd,0xff,0x77,0xff,0xdd,0xff},
            {0x55,0xbb,0x55,0xee,0x55,0xbb,0x55,0xee},
            {0xaa,0x55,0xaa,0x55,0xaa,0x55,0xaa,0x55},
            {0xaa,0x44,0xaa,0x11,0xaa,0x44,0xaa,0x11},
            {0x88,0x00,0x22,0x00,0x88,0x00,0x22,0x00},
            {0x00,0x44,0x00,0x00,0x00,0x44,0x00,0x00},
            {0x00,0x00,0x00,0xff,0x00,0x00,0x00,0x00},
            {0x10,0x10,0x10,0x10,0x10,0x10,0x10,0x10},
            {0x10,0x10,0x10,0xff,0x10,0x10,0x10,0x10},
            {0x80,0x40,0x20,0x10,0x08,0x04,0x02,0x01},
            {0x01,0x02,0x04,0x08,0x10,0x20,0x40,0x80},
            {0x81,0x42,0x24,0x18,0x18,0x24,0x42,0x81}
        };
        const auto color=brush.color().rgba();
        const auto a=alpha(color);
        const auto foreground=argb(a,multiply8(red(color),a),multiply8(green(color),a),multiply8(blue(color),a));
        PkImage tile(8,8,PkImage::Format_ARGB32_Premultiplied);
        const auto &rows=patterns[brush.style()-Pk::Dense1Pattern];
        for (int y=0;y<8;++y) for (int x=0;x<8;++x)
            tile.setPixel(x,y,(rows[y]&(1u<<x))?foreground:0);
        renderImage(tile,mask,brush.transform(),PkRectF(0,0,8,8),true);
        return;
    }
    if (brush.style() != Pk::SolidPattern && !gradient)
        throw std::logic_error("PkImageRasterBackend unsupported brush");
    const uint32_t color = brush.color().rgba();
    const unsigned fixedOpacity = static_cast<unsigned>(m_state.opacity * 256.0);
    std::array<Rgba64Pixel, 1024> ramp {};
    PkTransform inverse;
    double gradientDx = 0, gradientDy = 0, gradientOffset = 0;
    if (gradient) {
        ramp = gradientTable(*gradient, fixedOpacity);
        auto transform = brush.transform();
        if (gradient->coordinateMode() == PkGradient::ObjectBoundingMode) {
            const auto bounds = path.boundingRect();
            PkTransform object;
            object.translate(bounds.x(), bounds.y());
            object.scale(bounds.width(), bounds.height());
            transform = transform * object;
        }
        // QSpanData biases the source sampling origin by one 16.16 unit
        // before inversion so gradient table boundary rounding is stable.
        inverse = (PkTransform::fromTranslate(1.0 / 65536, 1.0 / 65536) *
                   transform * m_state.transform).inverted();
        gradientDx = gradient->finalStop().x() - gradient->start().x();
        gradientDy = gradient->finalStop().y() - gradient->start().y();
        const double length = gradientDx * gradientDx + gradientDy * gradientDy;
        if (length != 0) {
            gradientDx /= length;
            gradientDy /= length;
            gradientOffset = -gradientDx * gradient->start().x() - gradientDy * gradient->start().y();
        }
    }
    const unsigned sourceAlpha = (alpha(color) * 257u * fixedOpacity) >> 8;
    const uint32_t source = argb(to8Bit(sourceAlpha),
        to8Bit(divideBy65535(red(color) * 257u * sourceAlpha)),
        to8Bit(divideBy65535(green(color) * 257u * sourceAlpha)),
        to8Bit(divideBy65535(blue(color) * 257u * sourceAlpha)));
    for (int y = 0; y < m_destination.height(); ++y) {
        int x = 0;
        while (x < m_destination.width()) {
            const auto index = static_cast<std::size_t>(y) * m_destination.width() + x;
            unsigned amount = mask[index];
            if (m_state.hasClip) amount = (amount * m_state.clip[index] + 127) / 255;
            const int start = x++;
            while (x < m_destination.width()) {
                const auto next = static_cast<std::size_t>(y) * m_destination.width() + x;
                unsigned nextAmount = mask[next];
                if (m_state.hasClip) nextAmount = (nextAmount * m_state.clip[next] + 127) / 255;
                if (gradient ? (!amount || !nextAmount) : nextAmount != amount) break;
                ++x;
            }
            if (!amount) continue;
            const int count = x - start;
            if (gradient) {
                const auto positions = inverse.map(PkPointF(start + 0.5, y + 0.5));
                const double t = (gradientDx * positions.x() + gradientDy * positions.y() + gradientOffset) * 1023;
                const double increment = (gradientDx * inverse.m11() + gradientDy * inverse.m12()) * 1023;
                int fixed = int(t * 256);
                const int step = int(increment * 256);
                const auto destinations = premultiplySpan(m_destination, y, start, count);
                for (int i = 0; i < count; ++i, fixed += step) {
                    const auto maskIndex=static_cast<std::size_t>(y)*m_destination.width()+start+i;
                    unsigned sampleAmount=mask[maskIndex];
                    if (m_state.hasClip) sampleAmount=(sampleAmount*m_state.clip[maskIndex]+127)/255;
                    int index = gradientIndex((fixed + 128) >> 8, gradient->spread());
                    bool valid = true;
                    if (gradient->type() == PkGradient::ConicalGradient) {
                        const auto p = inverse.map(PkPointF(start + i + 0.5, y + 0.5)) - gradient->center();
                        const double angle = std::atan2(p.y(), p.x()) + gradient->angle() * (M_PI / 180);
                        index = gradientIndex(int((1 - angle / (2 * M_PI)) * 1023 + 0.5), PkGradient::RepeatSpread);
                    } else if (gradient->type() == PkGradient::RadialGradient) {
                        const auto p = inverse.map(PkPointF(start + i + 0.5, y + 0.5)) - gradient->focalPoint();
                        const auto delta = gradient->center() - gradient->focalPoint();
                        const double dr = gradient->radius() - gradient->focalRadius();
                        const double a = dr * dr - delta.x() * delta.x() - delta.y() * delta.y();
                        const double b = 2 * (dr * gradient->focalRadius() + p.x() * delta.x() + p.y() * delta.y());
                        const double c = gradient->focalRadius() * gradient->focalRadius() - p.x() * p.x() - p.y() * p.y();
                        const double determinant = b * b - 4 * a * c;
                        valid = !pkQtFuzzyIsNull(a) && determinant >= 0;
                        if (valid) {
                            const double root = std::sqrt(determinant);
                            const double position = std::max((-b + root) / (2 * a), (-b - root) / (2 * a));
                            valid = gradient->focalRadius() + dr * position >= 0;
                            index = gradientIndex(int(position * 1023 + 0.5), gradient->spread());
                        }
                    }
                    const auto sample = valid ? ramp[index] : Rgba64Pixel{};
                    if (uses32BitComposition(m_destination.format())) {
                        const uint32_t source = argb(to8Bit(sample.a),to8Bit(sample.r),to8Bit(sample.g),to8Bit(sample.b));
                        storeComposedPixel(m_destination,start+i,y,composeSolid(m_destination.pixel(start+i,y),source,sampleAmount,m_state.mode,true,false));
                    } else {
                        m_destination.setPixel(start + i, y,
                            compose(destinations[i], sample, sampleAmount, i >= count - count % 4, m_state.mode));
                    }
                }
                continue;
            }
            for (int i = 0; i < count; ++i) {
                storeComposedPixel(m_destination,start + i, y,
                    composeSolid(m_destination.pixel(start + i, y), source, amount, m_state.mode,
                                 uses32BitComposition(m_destination.format())));
            }
        }
    }
}

void PkImageRasterBackend::strokePath(const PkPainterPath &path, const PkPen &pen, bool point)
{
    if (pen.style() == Pk::NoPen) return;
    double scale;
    const bool noShear = scaleForTransform(m_state.transform, &scale);
    const double width = pen.widthF() * (pen.isCosmetic() ? 1 : scale);
    if (width <= 1 && (pen.isCosmetic() || noShear || !(m_state.hints & 1u))) {
        if (m_destination.format() != PkImage::Format_ARGB32 && !uses32BitComposition(m_destination.format())) {
            throw std::logic_error("PkImageRasterBackend cosmetic brush/image format unsupported");
        }
        if (pen.brush().style() != Pk::SolidPattern) {
            const PkBrush brush=pen.brush();
            // Retain the cosmetic stroker's exact coverage, and use the same
            // brush sampler as fills. Flush overlapping spans in order rather
            // than unioning them (SourceOver/Plus must see every contribution).
            struct Context {
                PkImageRasterBackend *backend;
                const PkPainterPath *path;
                const PkBrush *brush;
                std::vector<unsigned char> mask;
            } context {this,&path,&brush,std::vector<unsigned char>(
                static_cast<std::size_t>(m_destination.width())*m_destination.height(),0)};
            const auto blend=[](int count,const PK_FT_Span *spans,void *data) {
                auto &c=*static_cast<Context*>(data);
                const auto flush=[&] {
                    c.backend->fillMask(*c.path,*c.brush,c.mask);
                    std::fill(c.mask.begin(),c.mask.end(),0);
                };
                for (int i=0;i<count;++i) {
                    const auto &s=spans[i];
                    auto first=c.mask.begin()+static_cast<std::size_t>(s.y)*c.backend->m_destination.width()+s.x;
                    if (std::any_of(first,first+s.len,[](unsigned char v){return v!=0;})) flush();
                    std::fill_n(first,s.len,s.coverage);
                }
                if (count) flush();
            };
            PkCosmeticStroker stroker(pen,m_state.transform,m_state.hints&1u,scale,m_destination.rect(),blend,&context);
            if (point) { const auto position=path.currentPosition(); stroker.drawPoints(&position,1); }
            else stroker.drawPath(path);
            return;
        }
        const uint32_t color = pen.color().rgba();
        const unsigned opacity = static_cast<unsigned>(m_state.opacity * 256);
        const unsigned a = (alpha(color) * 257u * opacity) >> 8;
        const uint32_t source = argb(to8Bit(a),
            to8Bit(divideBy65535(red(color) * 257u * a)),
            to8Bit(divideBy65535(green(color) * 257u * a)),
            to8Bit(divideBy65535(blue(color) * 257u * a)));
        struct Context { PkImageRasterBackend *backend; uint32_t source; } context {this, source};
        const auto blend = [](int count, const PK_FT_Span *spans, void *data) {
            const auto &context = *static_cast<Context *>(data);
            auto &backend = *context.backend;
            for (int i = 0; i < count; ++i) {
                const auto &span = spans[i];
                for (int x = span.x; x < span.x + span.len; ++x) {
                    unsigned amount = span.coverage;
                    if (backend.m_state.hasClip) {
                        const auto index = static_cast<std::size_t>(span.y) * backend.m_destination.width() + x;
                        amount = (amount * backend.m_state.clip[index] + 127) / 255;
                    }
                    if (amount) storeComposedPixel(backend.m_destination,x, span.y,
                        composeSolid(backend.m_destination.pixel(x, span.y), context.source,
                                     amount, backend.m_state.mode,
                                     uses32BitComposition(backend.m_destination.format())));
                }
            }
        };
        PkCosmeticStroker stroker(pen, m_state.transform, m_state.hints & 1u, scale,
                                  m_destination.rect(), blend, &context);
        if (point) {
            const auto position = path.currentPosition();
            stroker.drawPoints(&position, 1);
        } else {
            stroker.drawPath(path);
        }
        return;
    }
    if (point) throw std::logic_error("PkImageRasterBackend wide point rasterization unsupported");
    if (pen.isCosmetic()) {
        // Cosmetic pen width is measured after the world transform.
        const auto transform = m_state.transform;
        const auto outline = PkRender::createStrokeOutline(transform.map(path), pen);
        m_state.transform = PkTransform();
        try { fillPath(outline, pen.brush()); }
        catch (...) { m_state.transform = transform; throw; }
        m_state.transform = transform;
    } else {
        fillPath(PkRender::createStrokeOutline(path, pen), pen.brush());
    }
}

void PkImageRasterBackend::drawImage(const PkDrawImageCommand &command)
{
    if (m_destination.format() != PkImage::Format_ARGB32 || command.image.format() != PkImage::Format_ARGB32 ||
        !m_state.transform.isIdentity() || m_state.hasClip ||
        !isIntegralCoordinate(command.target.x()) || !isIntegralCoordinate(command.target.y()) ||
        command.target.width() != command.image.width() || command.target.height() != command.image.height()) {
        drawTransformedImage(command);
        return;
    }
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
    const unsigned fixedOpacity = static_cast<unsigned>(m_state.opacity * 256.0);
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
        const int scalarStoreBegin = count - count % 4;
        for (int i = 0; i < count; ++i) {
            m_destination.setPixel(
                destinationX + i,
                y,
                compose(destinationPixels[static_cast<std::size_t>(i)],
                           sourcePixels[static_cast<std::size_t>(i)],
                           opacity,
                           i >= scalarStoreBegin,
                           m_state.mode));
        }
    }
}

void PkImageRasterBackend::drawText(const PkDrawTextAtPointCommand &command)
{
    // Text is painted with the pen colour only: neither the pen width nor the
    // brush influences it (measured against Qt 5.15.7, R-55 plan §2 P2). So the
    // fill always composes a solid colour, through the shared fillMask() path,
    // which already applies painter opacity, composition mode and clip.
    PkBrush brush(m_state.pen.color());
    if (brush.style() == Pk::NoBrush) return;
    if (m_destination.format() != PkImage::Format_ARGB32 &&
        !uses32BitComposition(m_destination.format())) {
        throw std::invalid_argument("PkImageRasterBackend requires ARGB32 or Grayscale8 destination");
    }
    // Glyphs are rasterised in device space: Qt passes the brush transform down
    // to the font engine per glyph rather than stroking a transformed outline
    // (R-55 plan §2 P5/P7). The translation is already baked into devicePoint.
    const PkPointF devicePoint = m_state.transform.map(command.position);
    // `coverage()` returns the mask plus offsets relative to the snapped baseline
    // origin, in the layout of whichever branch it took (identity or transformed);
    // the mapped baseline is added back when the mask is placed below, so both
    // branches land on the same device pixel grid (PkFontRasterizer.h TextCoverage).
    const auto cov = PkFontRasterizer::coverage(command.text, m_state.font, devicePoint,
                                               m_state.transform);
    if (cov.isEmpty()) return;
    // Anchor the mask on the device pixel grid exactly as the glyph blit does:
    // cell (col, row) covers
    //   (floor(devicePoint.x()) + offsetX + col,
    //    round(devicePoint.y()) + offsetY + row)
    // (PkFontRasterizer.h TextCoverage; qpaintengine_raster.cpp:2892-2893).
    const int originX = static_cast<int>(std::floor(devicePoint.x())) + cov.offsetX;
    const int originY = static_cast<int>(std::lround(devicePoint.y())) + cov.offsetY;
    const int width = m_destination.width();
    const int height = m_destination.height();
    std::vector<unsigned char> mask(static_cast<std::size_t>(width) * height, 0);
    for (int row = 0; row < cov.height; ++row) {
        const int y = originY + row;
        if (y < 0 || y >= height) continue;
        for (int col = 0; col < cov.width; ++col) {
            const int x = originX + col;
            if (x < 0 || x >= width) continue;
            mask[static_cast<std::size_t>(y) * width + x] =
                cov.mask[static_cast<std::size_t>(row) * cov.width + col];
        }
    }
    // Solid-colour brushes never read `path` (only the gradient / pattern
    // branches do), but the contract is a device-space ink rectangle.
    PkPainterPath inkBox;
    inkBox.addRect(PkRectF(originX, originY, cov.width, cov.height));
    fillMask(inkBox, brush, mask);
}

void PkImageRasterBackend::drawTransformedImage(const PkDrawImageCommand &command,
                                               const PkRectF &sourceRect, bool tiled)
{
    if (command.image.isNull() || command.target.isEmpty()) return;
    const PkRectF source = sourceRect.isNull() ? PkRectF(0, 0, command.image.width(), command.image.height()) : sourceRect;
    PkTransform target;
    target.translate(command.target.x(), command.target.y());
    if (!tiled) target.scale(command.target.width() / source.width(), command.target.height() / source.height());
    target.translate(-source.x(), -source.y());
    if (tiled) target.scale(1.0 / command.image.devicePixelRatio(), 1.0 / command.image.devicePixelRatio());
    renderImage(command.image, rectangleCoverage(command.target), target, source, tiled);
}

void PkImageRasterBackend::renderImage(const PkImage &image, const std::vector<unsigned char> &mask,
                                      const PkTransform &placement, const PkRectF &source, bool tiled)
{
    if (image.isNull()) return;
    if (m_destination.format() != PkImage::Format_ARGB32 && !uses32BitComposition(m_destination.format())) {
        throw std::invalid_argument("PkImageRasterBackend requires ARGB32 or Grayscale8 destination");
    }
    switch (image.format()) {
    case PkImage::Format_RGB32:
    case PkImage::Format_RGBX64:
    case PkImage::Format_RGBA64:
    case PkImage::Format_RGBA64_Premultiplied:
    case PkImage::Format_ARGB32:
    case PkImage::Format_ARGB32_Premultiplied:
    case PkImage::Format_Grayscale8:
    case PkImage::Format_Grayscale16:
    case PkImage::Format_Mono:
        break;
    default:
        throw std::invalid_argument("PkImageRasterBackend unsupported image source format");
    }
    if (!(placement * m_state.transform).isAffine()) throw std::logic_error("PkImageRasterBackend projective image unsupported");
    const auto inverse = (PkTransform::fromTranslate(1.0 / 65536, 1.0 / 65536) *
                          placement * m_state.transform).inverted();
    const bool smooth = m_state.hints & 4u;
    const double f1 = inverse.m11() * inverse.m11() + inverse.m21() * inverse.m21();
    const double f2 = inverse.m12() * inverse.m12() + inverse.m22() * inverse.m22();
    const bool fastMatrix = f1 < 1e4 && f2 < 1e4 && f1 > 1.0 / 65536 && f2 > 1.0 / 65536 &&
        std::abs(inverse.dx()) < 1e4 && std::abs(inverse.dy()) < 1e4;
    const int stepX = fastMatrix ? int(inverse.m11() * 65536) : 0;
    const int stepY = fastMatrix ? int(inverse.m12() * 65536) : 0;
    const unsigned opacity = unsigned(m_state.opacity * 256);
    const int left = tiled ? 0 : std::max(0, int(std::floor(source.left())));
    const int top = tiled ? 0 : std::max(0, int(std::floor(source.top())));
    const int right = tiled ? image.width() - 1 : std::min(image.width() - 1, int(std::ceil(source.right())) - 1);
    const int bottom = tiled ? image.height() - 1 : std::min(image.height() - 1, int(std::ceil(source.bottom())) - 1);
    if (right < left || bottom < top) return;
    const auto fetch = [&](int x, int y) {
        if (tiled) {
            x %= image.width(); if (x < 0) x += image.width();
            y %= image.height(); if (y < 0) y += image.height();
        } else {
            x = std::clamp(x, left, right);
            y = std::clamp(y, top, bottom);
        }
        if (image.format()==PkImage::Format_Grayscale16) {
            unsigned gray=reinterpret_cast<const std::uint16_t*>(image.constScanLine(y))[x];
            // Qt fetches full gray16 into RGBA64, but rounds through div_257
            // before interpolation when the destination uses the 32-bit path.
            if (uses32BitComposition(m_destination.format()))
                gray=to8Bit(gray)*257u;
            return Rgba64Pixel {65535,gray,gray,gray};
        }
        if (image.depth()==64) {
            const auto *rgba=reinterpret_cast<const std::uint16_t*>(image.constScanLine(y))+4*x;
            const unsigned a=image.format()==PkImage::Format_RGBX64?65535:rgba[3];
            if (image.format()==PkImage::Format_RGBA64 && uses32BitComposition(m_destination.format()) &&
                ((!smooth && (placement*m_state.transform).type()>PkTransform::TxTranslate) ||
                 (smooth && (stepY!=0 || std::abs(stepX)>131072)))) {
                const unsigned alpha8=to8Bit(a);
                return Rgba64Pixel {alpha8*257u,multiply8(to8Bit(rgba[0]),alpha8)*257u,
                    multiply8(to8Bit(rgba[1]),alpha8)*257u,multiply8(to8Bit(rgba[2]),alpha8)*257u};
            }
            if (image.format()==PkImage::Format_RGBA64_Premultiplied)
                return Rgba64Pixel {a,rgba[0],rgba[1],rgba[2]};
            return Rgba64Pixel {a,divideBy65535(rgba[0]*a),divideBy65535(rgba[1]*a),divideBy65535(rgba[2]*a)};
        }
        const auto pixel = image.pixel(x, y);
        // Premultiplied image bytes are already associated with alpha. Qt's
        // RGBA64 fetch expands them directly; a second multiplication loses
        // colored-glyph energy, especially along translucent edges.
        if (image.format() == PkImage::Format_ARGB32_Premultiplied) {
            return Rgba64Pixel {alpha(pixel) * 257u, red(pixel) * 257u,
                                green(pixel) * 257u, blue(pixel) * 257u};
        }
        if (uses32BitComposition(m_destination.format())) {
            const unsigned a=alpha(pixel);
            return Rgba64Pixel {a*257u,multiply8(red(pixel),a)*257u,multiply8(green(pixel),a)*257u,multiply8(blue(pixel),a)*257u};
        }
        return premultiply64(pixel);
    };
    const auto interpolate = [&](const Rgba64Pixel &a, const Rgba64Pixel &b, unsigned amount) {
        if (!amount) return a;
        const auto mix = [&](unsigned x, unsigned y) {
            if (uses32BitComposition(m_destination.format())) {
                // The 32-bit fetcher truncates each separable pass to eight
                // bits, with an eight-bit sample fraction (not RGBA64 lerp).
                const unsigned fraction=amount>>8;
                return ((to8Bit(x)*(256-fraction)+to8Bit(y)*fraction)>>8)*257u;
            }
            return ((x * (65536 - amount)) >> 16) + ((y * amount) >> 16);
        };
        return Rgba64Pixel {mix(a.a, b.a), mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b)};
    };
    for (int y = 0; y < m_destination.height(); ++y) {
        int x = 0;
        const auto amountAt = [&](int x) {
            const auto index = static_cast<std::size_t>(y) * m_destination.width() + x;
            unsigned amount = mask[index];
            if (m_state.hasClip) amount = (amount * m_state.clip[index] + 127) / 255;
            return amount;
        };
        while (x < m_destination.width()) {
            const unsigned amount = amountAt(x);
            const int start = x++;
            if (!amount) continue;
            // Qt batches adjacent spans on one scanline even when coverage
            // changes. Sampling fixed-point phase and SIMD store tails refer
            // to that whole batch, not to each coverage run.
            while (x < m_destination.width() && amountAt(x) != 0) ++x;
            const int count = x - start;
            const auto p = inverse.map(PkPointF(start + 0.5, y + 0.5));
            bool fastSpan = fastMatrix;
            for (double coordinate : {p.x() * 65536, p.y() * 65536,
                    p.x() * 65536 + double(stepX) * count, p.y() * 65536 + double(stepY) * count})
                if (!std::isfinite(coordinate) || coordinate < std::numeric_limits<int>::min() ||
                    coordinate > std::numeric_limits<int>::max()) fastSpan = false;
            std::int64_t fx = fastSpan ? std::int64_t(p.x() * 65536) - (smooth ? 32768 : 0) : 0;
            std::int64_t fy = fastSpan ? std::int64_t(p.y() * 65536) - (smooth ? 32768 : 0) : 0;
            double floatingX = p.x(), floatingY = p.y();
            int lowPrecisionStart=0,lowPrecisionEnd=0;
            if (fastSpan && smooth && !tiled && uses32BitComposition(m_destination.format()) &&
                (image.format()==PkImage::Format_ARGB32_Premultiplied || image.format()==PkImage::Format_RGB32) &&
                stepY!=0 && std::abs(inverse.m11())>=.125 && std::abs(inverse.m22())>=.125) {
                // Qt's PM fast-rotation SIMD interior uses rounded four-bit
                // fractions; bounded edges and scalar tails retain eight bits.
                std::int64_t tx=fx,ty=fy;
                while (lowPrecisionStart<count &&
                    ((tx>>16)<left || (tx>>16)>=right || (ty>>16)<top || (ty>>16)>=bottom)) {
                    ++lowPrecisionStart; tx+=stepX; ty+=stepY;
                }
                int length=count-lowPrecisionStart;
                if (stepX>0) length=std::min(length,int((std::int64_t(right)*65536-tx)/stepX));
                else if (stepX<0) length=std::min(length,int((std::int64_t(left)*65536-tx)/stepX));
                if (stepY>0) length=std::min(length,int((std::int64_t(bottom)*65536-ty)/stepY));
                else if (stepY<0) length=std::min(length,int((std::int64_t(top)*65536-ty)/stepY));
                int lanes=1;
#if defined(__SSE2__)
                lanes=__builtin_cpu_supports("avx2")?8:4;
#elif defined(__ARM_NEON__)
                lanes=4;
#endif
                lowPrecisionEnd=lowPrecisionStart+std::max(0,length)/lanes*lanes;
            }
            const auto destinations = premultiplySpan(m_destination, y, start, count);
            for (int i = 0; i < count; ++i, fx += stepX, fy += stepY) {
                int sx = int(fx >> 16), sy = int(fy >> 16);
                unsigned fractionX = fx & 65535, fractionY = fy & 65535;
                if (!fastSpan) {
                    double px = floatingX - (smooth ? 0.5 : 0);
                    double py = floatingY - (smooth ? 0.5 : 0);
                    if (!std::isfinite(px) || !std::isfinite(py)) throw std::invalid_argument("non-finite image sampling coordinate");
                    if (tiled) { px = std::fmod(px, image.width()); py = std::fmod(py, image.height()); }
                    sx = int(std::clamp(std::floor(px), -1.0, double(right)));
                    sy = int(std::clamp(std::floor(py), -1.0, double(bottom)));
                    fractionX = unsigned((px - std::floor(px)) * 65536);
                    fractionY = unsigned((py - std::floor(py)) * 65536);
                    floatingX += inverse.m11();
                    floatingY += inverse.m12();
                }
                Rgba64Pixel source = fetch(sx, sy);
                if (smooth) {
                    const auto right = fetch(sx + 1, sy);
                    const auto bottom = fetch(sx, sy + 1);
                    const auto bottomRight = fetch(sx + 1, sy + 1);
                    if (i>=lowPrecisionStart && i<lowPrecisionEnd) {
                        const unsigned dx=((fx&65535)+2048)>>12,dy=((fy&65535)+2048)>>12;
                        const auto mix=[&](unsigned tl,unsigned tr,unsigned bl,unsigned br) {
                            return ((to8Bit(tl)*(16-dx)*(16-dy)+to8Bit(tr)*dx*(16-dy)+
                                to8Bit(bl)*(16-dx)*dy+to8Bit(br)*dx*dy)>>8)*257u;
                        };
                        source={mix(source.a,right.a,bottom.a,bottomRight.a),mix(source.r,right.r,bottom.r,bottomRight.r),
                            mix(source.g,right.g,bottom.g,bottomRight.g),mix(source.b,right.b,bottom.b,bottomRight.b)};
                    } else {
                        source = interpolate(interpolate(source, bottom, fractionY),
                                             interpolate(right, bottomRight, fractionY), fractionX);
                    }
                }
                if (uses32BitComposition(m_destination.format())) {
                    const uint32_t pixel=argb(to8Bit(source.a),to8Bit(source.r),to8Bit(source.g),to8Bit(source.b));
                    const auto mode=m_state.mode==Pk::CompositionMode_SourceOver &&
                        image.format()==PkImage::Format_RGB32 && !smooth && !m_state.hasClip &&
                        !(m_state.hints&1u) && (placement*m_state.transform).type()<=PkTransform::TxScale?
                        Pk::CompositionMode_Source:m_state.mode;
                    storeComposedPixel(m_destination,start+i,y,composeSolid(m_destination.pixel(start+i,y),pixel,
                        (amountAt(start+i)*opacity)>>8,mode,true,false));
                } else {
                    const auto mode = m_state.mode == Pk::CompositionMode_SourceOver &&
                        image.format() == PkImage::Format_RGB32 ? Pk::CompositionMode_Source : m_state.mode;
                    m_destination.setPixel(start + i, y,
                        compose(destinations[i], source, (amountAt(start + i) * opacity) >> 8,
                                i >= count - count % 4, mode));
                }
            }
        }
    }
}
