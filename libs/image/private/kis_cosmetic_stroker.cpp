/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is derived from the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 2.0 or (at your option) the GNU General Public
** license version 3 or any later version approved by the KDE Free Qt
** Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "kis_cosmetic_stroker_p.h"

#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkPoint.h>
#include <PkRect.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <new>
#include <utility>
#include <vector>

namespace KisPathRasterizer::Private {
namespace {

constexpr int toF26Dot6(qreal value)
{
    return int(value * 64.0);
}

int fixedDiv16(int x, int y)
{
    return int(int64_t(x) * (int64_t(1) << 16) / y);
}

uint8_t byteMul(uint8_t value, uint8_t alpha)
{
    unsigned product = unsigned(value) * unsigned(alpha);
    product = (product + (product >> 8) + 0x80u) >> 8;
    return uint8_t(product);
}

class CosmeticStroker;

class CosmeticStroker
{
public:
    struct Point {
        int x = std::numeric_limits<int>::min();
        int y = std::numeric_limits<int>::min();
    };
    struct PointF {
        qreal x = 0;
        qreal y = 0;
    };
    enum Caps { NoCaps = 0, CapBegin = 0x1, CapEnd = 0x2 };
    enum Direction {
        NoDirection = 0,
        TopToBottom = 0x1,
        BottomToTop = 0x2,
        LeftToRight = 0x4,
        RightToLeft = 0x8,
        VerticalMask = 0x3,
        HorizontalMask = 0xc
    };

    CosmeticStroker(const PkPen &strokePen,
                    const PkRect &strokeClip,
                    bool useAntialiasing)
        : pen(strokePen)
        , clip(strokeClip)
        , antialiased(useAntialiasing)
    {
        mask.bounds = clip;
        mask.stride = clip.width();
        const std::size_t width = std::size_t(clip.width());
        const std::size_t height = std::size_t(clip.height());
        if (height != 0 && width > std::numeric_limits<std::size_t>::max() / height) {
            return;
        }
        mask.alpha.assign(width * height, uint8_t(0));

        const auto dashPattern = pen.dashPattern();
        if (!dashPattern.empty() && dashPattern.size() <= 1024) {
            pattern.resize(std::size_t(dashPattern.size()));
            reversePattern.resize(std::size_t(dashPattern.size()));
            patternLength = 0;
            for (std::size_t i = 0; i < dashPattern.size(); ++i) {
                patternLength += int(std::clamp(dashPattern.at(i) * 64.0,
                                                qreal(1.0), qreal(65536.0)));
                pattern[std::size_t(i)] = patternLength;
            }
            patternLength = 0;
            for (std::size_t i = 0; i < dashPattern.size(); ++i) {
                patternLength += int(std::clamp(
                    dashPattern.at(dashPattern.size() - 1 - i) * 64.0,
                    qreal(1.0), qreal(65536.0)));
                reversePattern[std::size_t(i)] = patternLength;
            }
        }

        const qreal widthF = pen.widthF();
        opacity = widthF == 0.0 ? 256 : std::clamp(int(256.0 * widthF), 0, 256);
        color = uint8_t((255u * unsigned(opacity)) >> 8);
        drawCaps = pen.capStyle() != Qt::FlatCap;
        xmin = -1.0;
        ymin = -1.0;
        xmax = qreal(clip.width() - 1) + 2.0;
        ymax = qreal(clip.height() - 1) + 2.0;
    }

    bool ready() const
    {
        return mask.alpha.size()
            == std::size_t(clip.width()) * std::size_t(clip.height());
    }

    void drawPath(const PkPainterPath &path);
    bool clipLine(qreal &x1, qreal &y1, qreal &x2, qreal &y2);
    bool strokeLine(qreal x1, qreal y1, qreal x2, qreal y2, int caps);
    void calculateLastPoint(qreal x1, qreal y1, qreal x2, qreal y2);
    void renderCubic(const PointF &p1, const PointF &p2,
                     const PointF &p3, const PointF &p4, int caps);
    void renderCubicSubdivision(PointF *points, int level, int caps);

    void drawPixel(int x, int y, int coverage)
    {
        if (x < 0 || y < 0 || x >= clip.width() || y >= clip.height()) {
            return;
        }
        uint8_t source = antialiased ? byteMul(color, uint8_t(coverage)) : color;
        uint8_t &destination = mask.alpha[std::size_t(y) * std::size_t(mask.stride)
                                          + std::size_t(x)];
        destination = uint8_t(unsigned(source)
                              + unsigned(byteMul(destination, uint8_t(255 - source))));
    }

    PointF local(const PkPainterPath::Element &element) const
    {
        return {element.x - qreal(clip.x()), element.y - qreal(clip.y())};
    }

    const PkPen &pen;
    const PkRect clip;
    const bool antialiased;
    CoverageMask mask;
    std::vector<int> pattern;
    std::vector<int> reversePattern;
    int patternLength = 0;
    int patternOffset = 0;
    int opacity = 256;
    uint8_t color = 255;
    bool drawCaps = true;
    qreal xmin = 0;
    qreal xmax = 0;
    qreal ymin = 0;
    qreal ymax = 0;
    Direction lastDir = NoDirection;
    Point lastPixel;
    bool lastAxisAligned = false;
};

struct Dasher
{
    CosmeticStroker *stroker;
    const std::vector<int> *pattern;
    int offset;
    int dashIndex;
    int dashOn;

    Dasher(CosmeticStroker *s, bool reverse, int start, int stop)
        : stroker(s)
        , pattern(reverse ? &s->reversePattern : &s->pattern)
        , offset(0)
        , dashIndex(0)
        , dashOn(reverse ? 0 : 1)
    {
        const int delta = stop - start;
        if (reverse) {
            offset = s->patternLength - s->patternOffset - delta - ((start & 63) - 32);
        } else {
            offset = s->patternOffset - ((start & 63) - 32);
        }
        offset %= s->patternLength;
        if (offset < 0) {
            offset += s->patternLength;
        }
        while (dashIndex < int(pattern->size()) - 1
               && offset >= (*pattern)[std::size_t(dashIndex)]) {
            ++dashIndex;
        }
        s->patternOffset = (s->patternOffset + delta) % s->patternLength;
    }

    bool on() const { return (dashIndex + dashOn) & 1; }
    void adjust()
    {
        offset += 64;
        if (offset >= (*pattern)[std::size_t(dashIndex)]) {
            dashIndex = (dashIndex + 1) % int(pattern->size());
        }
        offset %= stroker->patternLength;
    }
};

struct NoDasher
{
    NoDasher(CosmeticStroker *, bool, int, int) {}
    bool on() const { return true; }
    void adjust() {}
};

int swapCaps(int caps)
{
    return ((caps & CosmeticStroker::CapBegin) << 1)
        | ((caps & CosmeticStroker::CapEnd) >> 1);
}

void capAdjust(int caps, int &first, int &last, int &minor, int increment)
{
    if (caps & CosmeticStroker::CapBegin) {
        first -= 32;
        minor -= increment >> 1;
    }
    if (caps & CosmeticStroker::CapEnd) {
        last += 32;
    }
}

template<class Dash>
bool drawLineAliased(CosmeticStroker *stroker,
                     qreal rx1, qreal ry1, qreal rx2, qreal ry2, int caps)
{
    bool didDraw = std::abs(rx2 - rx1) + std::abs(ry2 - ry1) >= 1.0;
    if (stroker->clipLine(rx1, ry1, rx2, ry2)) {
        return true;
    }
    int x1 = toF26Dot6(rx1);
    int y1 = toF26Dot6(ry1);
    int x2 = toF26Dot6(rx2);
    int y2 = toF26Dot6(ry2);
    const int dx = std::abs(x2 - x1);
    const int dy = std::abs(y2 - y1);
    CosmeticStroker::Point last = stroker->lastPixel;

    if (dx < dy) {
        auto dir = CosmeticStroker::TopToBottom;
        bool swapped = false;
        if (y1 > y2) {
            swapped = true;
            std::swap(y1, y2);
            std::swap(x1, x2);
            caps = swapCaps(caps);
            dir = CosmeticStroker::BottomToTop;
        }
        const int xinc = fixedDiv16(x2 - x1, y2 - y1);
        int x = x1 * (1 << 10);
        if ((stroker->lastDir ^ CosmeticStroker::VerticalMask) == dir) {
            caps |= swapped ? CosmeticStroker::CapEnd : CosmeticStroker::CapBegin;
        }
        capAdjust(caps, y1, y2, x, xinc);
        int y = (y1 + 32) >> 6;
        int ys = (y2 + 32) >> 6;
        const int round = xinc > 0 ? 32 : 0;
        if ((caps & CosmeticStroker::CapBegin) && stroker->lastPixel.y == y + 1) {
            ++y;
        }
        if (y != ys) {
            x += ((y * 64 + round - y1) * xinc) >> 6;
            CosmeticStroker::Point first{x >> 16, y};
            last = {(x + (ys - y - 1) * xinc) >> 16, ys - 1};
            if (swapped) {
                std::swap(first, last);
            }
            const bool axisAligned = std::abs(xinc) < (1 << 14);
            if (stroker->lastPixel.x > std::numeric_limits<int>::min()) {
                if (first.x == stroker->lastPixel.x && first.y == stroker->lastPixel.y) {
                    if (swapped) --ys;
                    else { ++y; x += xinc; }
                } else if (stroker->lastDir != dir
                           && (((axisAligned && stroker->lastAxisAligned)
                                && stroker->lastPixel.x != first.x
                                && stroker->lastPixel.y != first.y)
                               || std::abs(stroker->lastPixel.x - first.x) > 1
                               || std::abs(stroker->lastPixel.y - first.y) > 1)) {
                    if (swapped) ++ys;
                    else { --y; x -= xinc; }
                } else if (stroker->lastDir == dir
                           && std::abs(stroker->lastPixel.x - first.x) <= 1
                           && std::abs(stroker->lastPixel.y - first.y) > 1) {
                    x += xinc >> 1;
                    last.x = swapped ? (x >> 16)
                                     : ((x + (ys - y - 1) * xinc) >> 16);
                }
            }
            stroker->lastDir = dir;
            stroker->lastAxisAligned = axisAligned;
            Dash dasher(stroker, swapped, y * 64, ys * 64);
            do {
                if (dasher.on()) stroker->drawPixel(x >> 16, y, 255);
                dasher.adjust();
                x += xinc;
            } while (++y < ys);
            didDraw = true;
        }
    } else {
        if (!dx) {
            return true;
        }
        auto dir = CosmeticStroker::LeftToRight;
        bool swapped = false;
        if (x1 > x2) {
            swapped = true;
            std::swap(x1, x2);
            std::swap(y1, y2);
            caps = swapCaps(caps);
            dir = CosmeticStroker::RightToLeft;
        }
        const int yinc = fixedDiv16(y2 - y1, x2 - x1);
        int y = y1 * (1 << 10);
        if ((stroker->lastDir ^ CosmeticStroker::HorizontalMask) == dir) {
            caps |= swapped ? CosmeticStroker::CapEnd : CosmeticStroker::CapBegin;
        }
        capAdjust(caps, x1, x2, y, yinc);
        int x = (x1 + 32) >> 6;
        int xs = (x2 + 32) >> 6;
        const int round = yinc > 0 ? 32 : 0;
        if ((caps & CosmeticStroker::CapBegin) && stroker->lastPixel.x == x + 1) {
            ++x;
        }
        if (x != xs) {
            y += ((x * 64 + round - x1) * yinc) >> 6;
            CosmeticStroker::Point first{x, y >> 16};
            last = {xs - 1, (y + (xs - x - 1) * yinc) >> 16};
            if (swapped) {
                std::swap(first, last);
            }
            const bool axisAligned = std::abs(yinc) < (1 << 14);
            if (stroker->lastPixel.x > std::numeric_limits<int>::min()) {
                if (first.x == stroker->lastPixel.x && first.y == stroker->lastPixel.y) {
                    if (swapped) --xs;
                    else { ++x; y += yinc; }
                } else if (stroker->lastDir != dir
                           && (((axisAligned && stroker->lastAxisAligned)
                                && stroker->lastPixel.x != first.x
                                && stroker->lastPixel.y != first.y)
                               || std::abs(stroker->lastPixel.x - first.x) > 1
                               || std::abs(stroker->lastPixel.y - first.y) > 1)) {
                    if (swapped) ++xs;
                    else { --x; y -= yinc; }
                } else if (stroker->lastDir == dir
                           && std::abs(stroker->lastPixel.x - first.x) <= 1
                           && std::abs(stroker->lastPixel.y - first.y) > 1) {
                    y += yinc >> 1;
                    last.y = swapped ? (y >> 16)
                                     : ((y + (xs - x - 1) * yinc) >> 16);
                }
            }
            stroker->lastDir = dir;
            stroker->lastAxisAligned = axisAligned;
            Dash dasher(stroker, swapped, x * 64, xs * 64);
            do {
                if (dasher.on()) stroker->drawPixel(x, y >> 16, 255);
                dasher.adjust();
                y += yinc;
            } while (++x < xs);
            didDraw = true;
        }
    }
    stroker->lastPixel = last;
    return didDraw;
}

template<class Dash>
bool drawLineAntialiased(CosmeticStroker *stroker,
                         qreal rx1, qreal ry1, qreal rx2, qreal ry2, int caps)
{
    if (stroker->clipLine(rx1, ry1, rx2, ry2)) {
        return true;
    }
    int x1 = toF26Dot6(rx1);
    int y1 = toF26Dot6(ry1);
    int x2 = toF26Dot6(rx2);
    int y2 = toF26Dot6(ry2);
    const int dx = x2 - x1;
    const int dy = y2 - y1;

    if (std::abs(dx) < std::abs(dy)) {
        const int xinc = fixedDiv16(dx, dy);
        bool swapped = false;
        if (y1 > y2) {
            std::swap(y1, y2);
            std::swap(x1, x2);
            swapped = true;
            caps = swapCaps(caps);
        }
        int x = (x1 - 32) * (1 << 10);
        x -= (((y1 & 63) - 32) * xinc) >> 6;
        capAdjust(caps, y1, y2, x, xinc);
        Dash dasher(stroker, swapped, y1, y2);
        int y = y1 >> 6;
        const int ys = y2 >> 6;
        int alphaStart;
        int alphaEnd;
        if (y == ys) {
            alphaStart = y2 - y1;
            alphaEnd = 0;
        } else {
            alphaStart = 64 - (y1 & 63);
            alphaEnd = y2 & 63;
        }
        if (dasher.on()) {
            const unsigned alpha = uint8_t(x >> 8);
            stroker->drawPixel(x >> 16, y, int((255 - alpha) * alphaStart >> 6));
            stroker->drawPixel((x >> 16) + 1, y, int(alpha * alphaStart >> 6));
        }
        dasher.adjust();
        x += xinc;
        ++y;
        while (y < ys) {
            if (dasher.on()) {
                const unsigned alpha = uint8_t(x >> 8);
                stroker->drawPixel(x >> 16, y, int(255 - alpha));
                stroker->drawPixel((x >> 16) + 1, y, int(alpha));
            }
            dasher.adjust();
            x += xinc;
            ++y;
        }
        if (alphaEnd && dasher.on()) {
            const unsigned alpha = uint8_t(x >> 8);
            stroker->drawPixel(x >> 16, y, int((255 - alpha) * alphaEnd >> 6));
            stroker->drawPixel((x >> 16) + 1, y, int(alpha * alphaEnd >> 6));
        }
    } else {
        if (!dx) {
            return true;
        }
        const int yinc = fixedDiv16(dy, dx);
        bool swapped = false;
        if (x1 > x2) {
            std::swap(x1, x2);
            std::swap(y1, y2);
            swapped = true;
            caps = swapCaps(caps);
        }
        int y = (y1 - 32) * (1 << 10);
        y -= (((x1 & 63) - 32) * yinc) >> 6;
        capAdjust(caps, x1, x2, y, yinc);
        Dash dasher(stroker, swapped, x1, x2);
        int x = x1 >> 6;
        const int xs = x2 >> 6;
        int alphaStart;
        int alphaEnd;
        if (x == xs) {
            alphaStart = x2 - x1;
            alphaEnd = 0;
        } else {
            alphaStart = 64 - (x1 & 63);
            alphaEnd = x2 & 63;
        }
        if (dasher.on()) {
            const unsigned alpha = uint8_t(y >> 8);
            stroker->drawPixel(x, y >> 16, int((255 - alpha) * alphaStart >> 6));
            stroker->drawPixel(x, (y >> 16) + 1, int(alpha * alphaStart >> 6));
        }
        dasher.adjust();
        y += yinc;
        ++x;
        while (x < xs) {
            if (dasher.on()) {
                const unsigned alpha = uint8_t(y >> 8);
                stroker->drawPixel(x, y >> 16, int(255 - alpha));
                stroker->drawPixel(x, (y >> 16) + 1, int(alpha));
            }
            dasher.adjust();
            y += yinc;
            ++x;
        }
        if (alphaEnd && dasher.on()) {
            const unsigned alpha = uint8_t(y >> 8);
            stroker->drawPixel(x, y >> 16, int((255 - alpha) * alphaEnd >> 6));
            stroker->drawPixel(x, (y >> 16) + 1, int(alpha * alphaEnd >> 6));
        }
    }
    return true;
}

bool CosmeticStroker::strokeLine(qreal x1, qreal y1, qreal x2, qreal y2, int caps)
{
    if (antialiased) {
        return pattern.empty()
            ? drawLineAntialiased<NoDasher>(this, x1, y1, x2, y2, caps)
            : drawLineAntialiased<Dasher>(this, x1, y1, x2, y2, caps);
    }
    return pattern.empty()
        ? drawLineAliased<NoDasher>(this, x1, y1, x2, y2, caps)
        : drawLineAliased<Dasher>(this, x1, y1, x2, y2, caps);
}

bool CosmeticStroker::clipLine(qreal &x1, qreal &y1, qreal &x2, qreal &y2)
{
    if (!std::isfinite(x1) || !std::isfinite(y1)
        || !std::isfinite(x2) || !std::isfinite(y2)) {
        return true;
    }
    if (x1 < xmin) {
        if (x2 <= xmin) goto clipped;
        y1 += (y2 - y1) / (x2 - x1) * (xmin - x1);
        x1 = xmin;
    } else if (x1 > xmax) {
        if (x2 >= xmax) goto clipped;
        y1 += (y2 - y1) / (x2 - x1) * (xmax - x1);
        x1 = xmax;
    }
    if (x2 < xmin) {
        lastPixel.x = std::numeric_limits<int>::min();
        y2 += (y2 - y1) / (x2 - x1) * (xmin - x2);
        x2 = xmin;
    } else if (x2 > xmax) {
        lastPixel.x = std::numeric_limits<int>::min();
        y2 += (y2 - y1) / (x2 - x1) * (xmax - x2);
        x2 = xmax;
    }
    if (y1 < ymin) {
        if (y2 <= ymin) goto clipped;
        x1 += (x2 - x1) / (y2 - y1) * (ymin - y1);
        y1 = ymin;
    } else if (y1 > ymax) {
        if (y2 >= ymax) goto clipped;
        x1 += (x2 - x1) / (y2 - y1) * (ymax - y1);
        y1 = ymax;
    }
    if (y2 < ymin) {
        lastPixel.x = std::numeric_limits<int>::min();
        x2 += (x2 - x1) / (y2 - y1) * (ymin - y2);
        y2 = ymin;
    } else if (y2 > ymax) {
        lastPixel.x = std::numeric_limits<int>::min();
        x2 += (x2 - x1) / (y2 - y1) * (ymax - y2);
        y2 = ymax;
    }
    return false;

clipped:
    lastPixel.x = std::numeric_limits<int>::min();
    return true;
}

void CosmeticStroker::calculateLastPoint(qreal rx1, qreal ry1, qreal rx2, qreal ry2)
{
    lastPixel = {};
    if (clipLine(rx1, ry1, rx2, ry2)) {
        return;
    }
    int x1 = toF26Dot6(rx1);
    int y1 = toF26Dot6(ry1);
    int x2 = toF26Dot6(rx2);
    int y2 = toF26Dot6(ry2);
    const int dx = std::abs(x2 - x1);
    const int dy = std::abs(y2 - y1);
    if (dx < dy) {
        bool swapped = false;
        if (y1 > y2) {
            swapped = true;
            std::swap(y1, y2);
            std::swap(x1, x2);
        }
        const int xinc = fixedDiv16(x2 - x1, y2 - y1);
        int x = x1 * (1 << 10);
        const int y = (y1 + 32) >> 6;
        const int ys = (y2 + 32) >> 6;
        const int round = xinc > 0 ? 32 : 0;
        if (y != ys) {
            x += ((y * 64 + round - y1) * xinc) >> 6;
            if (swapped) {
                lastPixel = {x >> 16, y};
                lastDir = BottomToTop;
            } else {
                lastPixel = {(x + (ys - y - 1) * xinc) >> 16, ys - 1};
                lastDir = TopToBottom;
            }
            lastAxisAligned = std::abs(xinc) < (1 << 14);
        }
    } else {
        if (!dx) return;
        bool swapped = false;
        if (x1 > x2) {
            swapped = true;
            std::swap(x1, x2);
            std::swap(y1, y2);
        }
        const int yinc = fixedDiv16(y2 - y1, x2 - x1);
        int y = y1 * (1 << 10);
        const int x = (x1 + 32) >> 6;
        const int xs = (x2 + 32) >> 6;
        const int round = yinc > 0 ? 32 : 0;
        if (x != xs) {
            y += ((x * 64 + round - x1) * yinc) >> 6;
            if (swapped) {
                lastPixel = {x, y >> 16};
                lastDir = RightToLeft;
            } else {
                lastPixel = {xs - 1, (y + (xs - x - 1) * yinc) >> 16};
                lastDir = LeftToRight;
            }
            lastAxisAligned = std::abs(yinc) < (1 << 14);
        }
    }
}

void splitCubic(CosmeticStroker::PointF *points)
{
    constexpr qreal half = 0.5;
    qreal a;
    qreal b;
    qreal c;
    qreal d;
    points[6].x = points[3].x;
    c = points[1].x; d = points[2].x;
    points[1].x = a = (points[0].x + c) * half;
    points[5].x = b = (points[3].x + d) * half;
    c = (c + d) * half;
    points[2].x = a = (a + c) * half;
    points[4].x = b = (b + c) * half;
    points[3].x = (a + b) * half;
    points[6].y = points[3].y;
    c = points[1].y; d = points[2].y;
    points[1].y = a = (points[0].y + c) * half;
    points[5].y = b = (points[3].y + d) * half;
    c = (c + d) * half;
    points[2].y = a = (a + c) * half;
    points[4].y = b = (b + c) * half;
    points[3].y = (a + b) * half;
}

void CosmeticStroker::renderCubic(const PointF &p1, const PointF &p2,
                                  const PointF &p3, const PointF &p4, int caps)
{
    constexpr int maxSubDivisions = 6;
    PointF points[3 * maxSubDivisions + 4];
    points[3] = p1;
    points[2] = p2;
    points[1] = p3;
    points[0] = p4;
    renderCubicSubdivision(points, maxSubDivisions, caps);
}

void CosmeticStroker::renderCubicSubdivision(PointF *points, int level, int caps)
{
    if (level) {
        const qreal dx = points[3].x - points[0].x;
        const qreal dy = points[3].y - points[0].y;
        const qreal len = 0.25 * (std::abs(dx) + std::abs(dy));
        if (std::abs(dx * (points[0].y - points[2].y)
                     - dy * (points[0].x - points[2].x)) >= len
            || std::abs(dx * (points[0].y - points[1].y)
                        - dy * (points[0].x - points[1].x)) >= len) {
            splitCubic(points);
            --level;
            renderCubicSubdivision(points + 3, level, caps & CapBegin);
            renderCubicSubdivision(points, level, caps & CapEnd);
            return;
        }
    }
    strokeLine(points[3].x, points[3].y, points[0].x, points[0].y, caps);
}

void CosmeticStroker::drawPath(const PkPainterPath &path)
{
    int subpathStart = 0;
    while (subpathStart < path.elementCount()) {
        if (path.elementAt(subpathStart).type != PkPainterPath::MoveToElement) {
            return;
        }
        int subpathEnd = subpathStart + 1;
        while (subpathEnd < path.elementCount()
               && path.elementAt(subpathEnd).type != PkPainterPath::MoveToElement) {
            ++subpathEnd;
        }
        PointF point = local(path.elementAt(subpathStart));
        const PointF first = point;
        patternOffset = int(pen.dashOffset() * 64.0);
        lastPixel = {};
        const PointF last = local(path.elementAt(subpathEnd - 1));
        const bool closed = first.x == last.x && first.y == last.y;
        if (closed && subpathEnd - subpathStart > 1) {
            const PointF beforeLast = local(path.elementAt(subpathEnd - 2));
            calculateLastPoint(beforeLast.x, beforeLast.y, last.x, last.y);
        }
        int caps = !closed && drawCaps ? CapBegin : NoCaps;
        int i = subpathStart + 1;
        while (i < subpathEnd) {
            const auto element = path.elementAt(i);
            const PointF next = local(element);
            if (element.type == PkPainterPath::LineToElement) {
                if (!closed && drawCaps && i == subpathEnd - 1) caps |= CapEnd;
                strokeLine(point.x, point.y, next.x, next.y, caps);
                point = next;
                ++i;
            } else if (element.type == PkPainterPath::CurveToElement
                       && i + 2 < subpathEnd) {
                if (!closed && drawCaps && i == subpathEnd - 3) caps |= CapEnd;
                const PointF control2 = local(path.elementAt(i + 1));
                const PointF endpoint = local(path.elementAt(i + 2));
                renderCubic(point, next, control2, endpoint, caps);
                point = endpoint;
                i += 3;
            } else {
                return;
            }
            caps = NoCaps;
        }
        subpathStart = subpathEnd;
    }
}

} // namespace

CoverageMask rasterizeCosmeticStroke(const PkPainterPath &path,
                                      const PkPen &pen,
                                      const PkRect &clip,
                                      bool antialiased)
{
    try {
        CosmeticStroker stroker(pen, clip, antialiased);
        if (!stroker.ready()) {
            return {};
        }
        stroker.drawPath(path);
        return std::move(stroker.mask);
    } catch (const std::bad_alloc &) {
        return {};
    }
}

} // namespace KisPathRasterizer::Private
