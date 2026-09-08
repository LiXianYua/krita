/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PkImageRasterBackend.h"
#include "PkGrayRaster.h"

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

uint32_t composeSolid(uint32_t destination, uint32_t premultipliedSource, unsigned coverage,
                      Pk::CompositionMode mode)
{
    const unsigned sa = alpha(premultipliedSource);
    const unsigned da = alpha(destination);
    unsigned s[] = {sa, red(premultipliedSource), green(premultipliedSource), blue(premultipliedSource)};
    unsigned d[] = {da, multiply8(red(destination), da), multiply8(green(destination), da), multiply8(blue(destination), da)};
    unsigned result[4];
    for (int i = 0; i < 4; ++i) {
        if (mode == Pk::CompositionMode_Plus) {
            result[i] = multiply8(std::min(255u, s[i] + d[i]), coverage) +
                multiply8(d[i], 255 - coverage);
        } else if (mode == Pk::CompositionMode_Source) {
            // Qt's 32-bit interpolation rounds the combined numerator once.
            const unsigned value = s[i] * coverage + d[i] * (255 - coverage) + 128;
            result[i] = (value + (value >> 8)) >> 8;
        } else {
            result[i] = multiply8(s[i], coverage) + multiply8(d[i], 255 - multiply8(sa, coverage));
        }
    }
    if (result[0] == 0) return 0;
    return argb(result[0], unpremultiplyTo8(result[1], result[0]),
                unpremultiplyTo8(result[2], result[0]), unpremultiplyTo8(result[3], result[0]));
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
    const double scale = std::max(std::hypot(m_state.transform.m11(), m_state.transform.m12()),
                                  std::hypot(m_state.transform.m21(), m_state.transform.m22()));
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
    const PkPainterPath mapped = m_state.transform.map(flattened);
    std::vector<PK_FT_Vector> points;
    std::vector<char> tags;
    std::vector<int> contours;
    for (int i = 0; i < mapped.elementCount(); ++i) {
        const auto element = mapped.elementAt(i);
        if (!std::isfinite(element.x) || !std::isfinite(element.y) ||
            std::abs(element.x) > 32767 || std::abs(element.y) > 32767) {
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

void PkImageRasterBackend::fillPath(const PkPainterPath &path, const PkBrush &brush, bool rectangle)
{
    if (brush.style() == Pk::NoBrush) return;
    if (!(m_state.hints & 1u)) {
        throw std::logic_error("PkImageRasterBackend aliased path rasterization is not implemented");
    }
    if (brush.style() != Pk::SolidPattern) {
        throw std::logic_error("PkImageRasterBackend unsupported brush");
    }
    if (m_destination.format() != PkImage::Format_ARGB32) {
        throw std::invalid_argument("PkImageRasterBackend requires ARGB32 destination");
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
                mask[static_cast<std::size_t>(y) * m_destination.width() + x] = (horizontal * vertical) >> 32;
            }
        }
    }
    const uint32_t color = brush.color().rgba();
    const unsigned fixedOpacity = static_cast<unsigned>(m_state.opacity * 256.0);
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
                if (nextAmount != amount) break;
                ++x;
            }
            if (!amount) continue;
            const int count = x - start;
            for (int i = 0; i < count; ++i) {
                m_destination.setPixel(start + i, y,
                    composeSolid(m_destination.pixel(start + i, y), source, amount, m_state.mode));
            }
        }
    }
}

void PkImageRasterBackend::drawImage(const PkDrawImageCommand &command)
{
    if (!m_state.transform.isIdentity() || m_state.hasClip) {
        throw std::logic_error("PkImageRasterBackend transformed/clipped image rasterization is not implemented");
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
