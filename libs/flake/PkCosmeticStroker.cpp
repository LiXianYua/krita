/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
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
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "PkCosmeticStroker.h"
#include <algorithm>
#include <cassert>
#include <climits>
#include <cmath>
#include <vector>

#define toF26Dot6(x) ((int)((x)*64.))
inline static int F16Dot16FixedDiv(int x, int y)
{
    if (pkAbs(x) > 0x7fff)
        return qlonglong(x) * (1<<16) / y;
    return x * (1<<16) / y;
}

typedef void (*DrawPixel)(PkCosmeticStroker *stroker, int x, int y, int coverage);

namespace {

struct Dasher {
    PkCosmeticStroker *stroker;
    int *pattern;
    int offset;
    int dashIndex;
    int dashOn;

    Dasher(PkCosmeticStroker *s, bool reverse, int start, int stop)
        : stroker(s)
    {
        int delta = stop - start;
        if (reverse) {
            pattern = stroker->reversePattern;
            offset = stroker->patternLength - stroker->patternOffset - delta - ((start & 63) - 32);
            dashOn = 0;
        } else {
            pattern = stroker->pattern;
            offset = stroker->patternOffset - ((start & 63) - 32);
            dashOn = 1;
        }
        offset %= stroker->patternLength;
        if (offset < 0)
            offset += stroker->patternLength;

        dashIndex = 0;
        while (dashIndex < stroker->patternSize - 1 && offset>= pattern[dashIndex])
            ++dashIndex;

//        qDebug() << "   dasher" << offset/64. << reverse << dashIndex;
        stroker->patternOffset += delta;
        stroker->patternOffset %= stroker->patternLength;
    }

    bool on() const {
        return (dashIndex + dashOn) & 1;
    }
    void adjust() {
        offset += 64;
        if (offset >= pattern[dashIndex]) {
            ++dashIndex;
            dashIndex %= stroker->patternSize;
        }
        offset %= stroker->patternLength;
//        qDebug() << "dasher.adjust" << offset/64. << dashIndex;
    }
};

struct NoDasher {
    NoDasher(PkCosmeticStroker *, bool, int, int) {}
    bool on() const { return true; }
    void adjust(int = 0) {}
};

};

/*
 * The return value is the result of the clipLine() call performed at the start
 * of each of the two functions, aka "false" means completely outside the devices
 * rect.
 */
template<DrawPixel drawPixel, class Dasher>
static bool drawLine(PkCosmeticStroker *stroker, qreal x1, qreal y1, qreal x2, qreal y2, int caps);
template<DrawPixel drawPixel, class Dasher>
static bool drawLineAA(PkCosmeticStroker *stroker, qreal x1, qreal y1, qreal x2, qreal y2, int caps);

inline void drawPixel(PkCosmeticStroker *stroker, int x, int y, int coverage)
{
    const PkRect &cl = stroker->clip;
    if (x < cl.x() || x > cl.right() || y < cl.y() || y > cl.bottom())
        return;

    if (stroker->current_span > 0) {
        const int lastx = stroker->spans[stroker->current_span-1].x + stroker->spans[stroker->current_span-1].len ;
        const int lasty = stroker->spans[stroker->current_span-1].y;

        if (stroker->current_span == PkCosmeticStroker::NSPANS || y < lasty || (y == lasty && x < lastx)) {
            stroker->blend(stroker->current_span, stroker->spans, stroker->context);
            stroker->current_span = 0;
        }
    }

    stroker->spans[stroker->current_span].x = ushort(x);
    stroker->spans[stroker->current_span].len = 1;
    stroker->spans[stroker->current_span].y = y;
    stroker->spans[stroker->current_span].coverage = coverage*stroker->opacity >> 8;
    ++stroker->current_span;
}

void PkCosmeticStroker::setup()
{
    const auto penPattern = lastPen.dashPattern();
    if (!penPattern.empty() && penPattern.size() <= 1024) {
        patternSize = penPattern.size();
        pattern = static_cast<int *>(malloc(patternSize * sizeof(int)));
        reversePattern = static_cast<int *>(malloc(patternSize * sizeof(int)));
        if (!pattern || !reversePattern) { free(pattern); free(reversePattern); throw std::bad_alloc(); }
        for (int i = 0; i < patternSize; ++i) {
            patternLength += int(pkBound(1., penPattern[i] * 64, 65536.));
            pattern[i] = patternLength;
        }
        patternLength = 0;
        for (int i = 0; i < patternSize; ++i) {
            patternLength += int(pkBound(1., penPattern[patternSize - 1 - i] * 64, 65536.));
            reversePattern[i] = patternLength;
        }
    }
    if (antialiased) stroke = patternSize ? &::drawLineAA<drawPixel, Dasher> : &::drawLineAA<drawPixel, NoDasher>;
    else stroke = patternSize ? &::drawLine<drawPixel, Dasher> : &::drawLine<drawPixel, NoDasher>;
    qreal width = lastPen.widthF();
    opacity = width == 0 ? 256 : int(256 * width * (lastPen.isCosmetic() ? 1 : txscale));
    opacity = pkBound(0, opacity, 256);
    drawCaps = lastPen.capStyle() != Pk::FlatCap;
    xmin = deviceRect.left() - 1; xmax = deviceRect.right() + 2;
    ymin = deviceRect.top() - 1; ymax = deviceRect.bottom() + 2;
    lastPixel.x = INT_MIN; lastPixel.y = INT_MIN;
}
// returns true if the whole line gets clipped away
bool PkCosmeticStroker::clipLine(qreal &x1, qreal &y1, qreal &x2, qreal &y2)
{
    if (!std::isfinite(x1) || !std::isfinite(y1) || !std::isfinite(x2) || !std::isfinite(y2))
        return true;
    // basic/rough clipping is done in floating point coordinates to avoid
    // integer overflow problems.
    if (x1 < xmin) {
        if (x2 <= xmin)
            goto clipped;
        y1 += (y2 - y1)/(x2 - x1) * (xmin - x1);
        x1 = xmin;
    } else if (x1 > xmax) {
        if (x2 >= xmax)
            goto clipped;
        y1 += (y2 - y1)/(x2 - x1) * (xmax - x1);
        x1 = xmax;
    }
    if (x2 < xmin) {
        lastPixel.x = INT_MIN;
        y2 += (y2 - y1)/(x2 - x1) * (xmin - x2);
        x2 = xmin;
    } else if (x2 > xmax) {
        lastPixel.x = INT_MIN;
        y2 += (y2 - y1)/(x2 - x1) * (xmax - x2);
        x2 = xmax;
    }

    if (y1 < ymin) {
        if (y2 <= ymin)
            goto clipped;
        x1 += (x2 - x1)/(y2 - y1) * (ymin - y1);
        y1 = ymin;
    } else if (y1 > ymax) {
        if (y2 >= ymax)
            goto clipped;
        x1 += (x2 - x1)/(y2 - y1) * (ymax - y1);
        y1 = ymax;
    }
    if (y2 < ymin) {
        lastPixel.x = INT_MIN;
        x2 += (x2 - x1)/(y2 - y1) * (ymin - y2);
        y2 = ymin;
    } else if (y2 > ymax) {
        lastPixel.x = INT_MIN;
        x2 += (x2 - x1)/(y2 - y1) * (ymax - y2);
        y2 = ymax;
    }

    return false;

  clipped:
    lastPixel.x = INT_MIN;
    return true;
}


void PkCosmeticStroker::drawLine(const PkPointF &p1, const PkPointF &p2)
{
    PkPointF start = p1 * matrix;
    PkPointF end = p2 * matrix;

    if (start == end) {
        drawPoints(&p1, 1);
        return;
    }

    patternOffset = lastPen.dashOffset()*64;
    lastPixel.x = INT_MIN;
    lastPixel.y = INT_MIN;

    stroke(this, start.x(), start.y(), end.x(), end.y(), drawCaps ? CapBegin|CapEnd : 0);

    blend(current_span, spans, context);
    current_span = 0;
}

void PkCosmeticStroker::drawPoints(const PkPoint *points, int num)
{
    const PkPoint *end = points + num;
    while (points < end) {
        PkPointF p = PkPointF(*points) * matrix;
        drawPixel(this, pkRound(p.x()), pkRound(p.y()), 255);
        ++points;
    }

    blend(current_span, spans, context);
    current_span = 0;
}

void PkCosmeticStroker::drawPoints(const PkPointF *points, int num)
{
    const PkPointF *end = points + num;
    while (points < end) {
        PkPointF p = (*points) * matrix;
        drawPixel(this, pkRound(p.x()), pkRound(p.y()), 255);
        ++points;
    }

    blend(current_span, spans, context);
    current_span = 0;
}

void PkCosmeticStroker::calculateLastPoint(qreal rx1, qreal ry1, qreal rx2, qreal ry2)
{
    // this is basically the same code as used in the aliased stroke method,
    // but it only determines the direction and last point of a line
    //
    // This is being used to have proper dropout control for closed contours
    // by calculating the direction and last pixel of the last segment in the contour.
    // the info is then used to perform dropout control when drawing the first line segment
    // of the contour
    lastPixel.x = INT_MIN;
    lastPixel.y = INT_MIN;

    if (clipLine(rx1, ry1, rx2, ry2))
        return;

    const int half = legacyRounding ? 31 : 0;
    int x1 = toF26Dot6(rx1) + half;
    int y1 = toF26Dot6(ry1) + half;
    int x2 = toF26Dot6(rx2) + half;
    int y2 = toF26Dot6(ry2) + half;

    int dx = pkAbs(x2 - x1);
    int dy = pkAbs(y2 - y1);

    if (dx < dy) {
        // vertical
        bool swapped = false;
        if (y1 > y2) {
            swapped = true;
            std::swap(y1, y2);
            std::swap(x1, x2);
        }
        int xinc = F16Dot16FixedDiv(x2 - x1, y2 - y1);
        int x = x1 * (1<<10);

        int y = (y1 + 32) >> 6;
        int ys = (y2 + 32) >> 6;

        int round = (xinc > 0) ? 32 : 0;
        if (y != ys) {
            x += ((y * (1<<6)) + round - y1) * xinc >> 6;

            if (swapped) {
                lastPixel.x = x >> 16;
                lastPixel.y = y;
                lastDir = PkCosmeticStroker::BottomToTop;
            } else {
                lastPixel.x = (x + (ys - y - 1)*xinc) >> 16;
                lastPixel.y = ys - 1;
                lastDir = PkCosmeticStroker::TopToBottom;
            }
            lastAxisAligned = pkAbs(xinc) < (1 << 14);
        }
    } else {
        // horizontal
        if (!dx)
            return;

        bool swapped = false;
        if (x1 > x2) {
            swapped = true;
            std::swap(x1, x2);
            std::swap(y1, y2);
        }
        int yinc = F16Dot16FixedDiv(y2 - y1, x2 - x1);
        int y = y1 * (1 << 10);

        int x = (x1 + 32) >> 6;
        int xs = (x2 + 32) >> 6;

        int round = (yinc > 0) ? 32 : 0;
        if (x != xs) {
            y += ((x * (1<<6)) + round - x1) * yinc >> 6;

            if (swapped) {
                lastPixel.x = x;
                lastPixel.y = y >> 16;
                lastDir = PkCosmeticStroker::RightToLeft;
            } else {
                lastPixel.x = xs - 1;
                lastPixel.y = (y + (xs - x - 1)*yinc) >> 16;
                lastDir = PkCosmeticStroker::LeftToRight;
            }
            lastAxisAligned = pkAbs(yinc) < (1 << 14);
        }
    }
//    qDebug() << "   moveTo: setting last pixel to x/y dir" << lastPixel.x << lastPixel.y << lastDir;
}

static inline const PkPainterPath::ElementType *subPath(const PkPainterPath::ElementType *t, const PkPainterPath::ElementType *end,
                                                 const qreal *points, bool *closed)
{
    const PkPainterPath::ElementType *start = t;
    ++t;

    // find out if the subpath is closed
    while (t < end) {
        if (*t == PkPainterPath::MoveToElement)
            break;
        ++t;
    }

    int offset = t - start - 1;
//    qDebug() << "subpath" << offset << points[0] << points[1] << points[2*offset] << points[2*offset+1];
    *closed = (points[0] == points[2*offset] && points[1] == points[2*offset + 1]);

    return t;
}

void PkCosmeticStroker::drawPath(const PkPainterPath &path)
{
//    qDebug() << ">>>> drawpath" << path.convertToPainterPath()
//             << "antialiasing:" << (bool)(state->renderHints & QPainter::Antialiasing) << " implicit close:" << path.hasImplicitClose();
    if (path.isEmpty())
        return;

    std::vector<qreal> coordinates;
    std::vector<PkPainterPath::ElementType> types;
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto e = path.elementAt(i);
        coordinates.push_back(e.x); coordinates.push_back(e.y); types.push_back(e.type);
    }
    const qreal *points = coordinates.data();
    const PkPainterPath::ElementType *type = types.data();

    if (type) {
        const PkPainterPath::ElementType *end = type + path.elementCount();

        while (type < end) {
            assert(type == types.data() || *type == PkPainterPath::MoveToElement);

            PkPointF p = PkPointF(points[0], points[1]) * matrix;
            patternOffset = lastPen.dashOffset()*64;
            lastPixel.x = INT_MIN;
            lastPixel.y = INT_MIN;

            bool closed;
            const PkPainterPath::ElementType *e = subPath(type, end, points, &closed);
            if (closed) {
                const qreal *p = points + 2*(e-type);
                PkPointF p1 = PkPointF(p[-4], p[-3]) * matrix;
                PkPointF p2 = PkPointF(p[-2], p[-1]) * matrix;
                calculateLastPoint(p1.x(), p1.y(), p2.x(), p2.y());
            }
            int caps = (!closed && drawCaps) ? CapBegin : NoCaps;
//            qDebug() << "closed =" << closed << capString(caps);

            points += 2;
            ++type;

            while (type < e) {
                PkPointF p2 = PkPointF(points[0], points[1]) * matrix;
                switch (*type) {
                case PkPainterPath::MoveToElement:
                    assert(!"Logic error");
                    break;

                case PkPainterPath::LineToElement:
                    if (!closed && drawCaps && type == e - 1)
                        caps |= CapEnd;
                    stroke(this, p.x(), p.y(), p2.x(), p2.y(), caps);
                    p = p2;
                    points += 2;
                    ++type;
                    break;

                case PkPainterPath::CurveToElement: {
                    if (!closed && drawCaps && type == e - 3)
                        caps |= CapEnd;
                    PkPointF p3 = PkPointF(points[2], points[3]) * matrix;
                    PkPointF p4 = PkPointF(points[4], points[5]) * matrix;
                    renderCubic(p, p2, p3, p4, caps);
                    p = p4;
                    type += 3;
                    points += 6;
                    break;
                }
                case PkPainterPath::CurveToDataElement:
                    assert(!"PkPainterPath::toSubpathPolygons(), bad element type");
                    break;
                }
                caps = NoCaps;
            }
        }
    }
    blend(current_span, spans, context);
    current_span = 0;
}

void PkCosmeticStroker::renderCubic(const PkPointF &p1, const PkPointF &p2, const PkPointF &p3, const PkPointF &p4, int caps)
{
//    qDebug() << ">>>> renderCubic" << p1 << p2 << p3 << p4 << capString(caps);
    const int maxSubDivisions = 6;
    PointF points[3*maxSubDivisions + 4];

    points[3].x = p1.x();
    points[3].y = p1.y();
    points[2].x = p2.x();
    points[2].y = p2.y();
    points[1].x = p3.x();
    points[1].y = p3.y();
    points[0].x = p4.x();
    points[0].y = p4.y();

    PointF *p = points;
    int level = maxSubDivisions;

    renderCubicSubdivision(p, level, caps);
}

static void splitCubic(PkCosmeticStroker::PointF *points)
{
    const qreal half = .5;
    qreal  a, b, c, d;

    points[6].x = points[3].x;
    c = points[1].x;
    d = points[2].x;
    points[1].x = a = ( points[0].x + c ) * half;
    points[5].x = b = ( points[3].x + d ) * half;
    c = ( c + d ) * half;
    points[2].x = a = ( a + c ) * half;
    points[4].x = b = ( b + c ) * half;
    points[3].x = ( a + b ) * half;

    points[6].y = points[3].y;
    c = points[1].y;
    d = points[2].y;
    points[1].y = a = ( points[0].y + c ) * half;
    points[5].y = b = ( points[3].y + d ) * half;
    c = ( c + d ) * half;
    points[2].y = a = ( a + c ) * half;
    points[4].y = b = ( b + c ) * half;
    points[3].y = ( a + b ) * half;
}

void PkCosmeticStroker::renderCubicSubdivision(PkCosmeticStroker::PointF *points, int level, int caps)
{
    if (level) {
        qreal dx = points[3].x - points[0].x;
        qreal dy = points[3].y - points[0].y;
        qreal len = ((qreal).25) * (pkAbs(dx) + pkAbs(dy));

        if (pkAbs(dx * (points[0].y - points[2].y) - dy * (points[0].x - points[2].x)) >= len ||
            pkAbs(dx * (points[0].y - points[1].y) - dy * (points[0].x - points[1].x)) >= len) {
            splitCubic(points);

            --level;
            renderCubicSubdivision(points + 3, level, caps & CapBegin);
            renderCubicSubdivision(points, level, caps & CapEnd);
            return;
        }
    }

    stroke(this, points[3].x, points[3].y, points[0].x, points[0].y, caps);
}

static inline int swapCaps(int caps)
{
    return ((caps & PkCosmeticStroker::CapBegin) << 1) |
           ((caps & PkCosmeticStroker::CapEnd) >> 1);
}

// adjust line by half a pixel
static inline void capAdjust(int caps, int &x1, int &x2, int &y, int yinc)
{
    if (caps & PkCosmeticStroker::CapBegin) {
        x1 -= 32;
        y -= yinc >> 1;
    }
    if (caps & PkCosmeticStroker::CapEnd) {
        x2 += 32;
    }
}

/*
  The hard part about this is dropout control and avoiding douple drawing of points when
  the drawing shifts from horizontal to vertical or back.
  */
template<DrawPixel drawPixel, class Dasher>
static bool drawLine(PkCosmeticStroker *stroker, qreal rx1, qreal ry1, qreal rx2, qreal ry2, int caps)
{
    bool didDraw = pkAbs(rx2 - rx1) + pkAbs(ry2 - ry1) >= 1.0;

    if (stroker->clipLine(rx1, ry1, rx2, ry2))
        return true;

    const int half = stroker->legacyRounding ? 31 : 0;
    int x1 = toF26Dot6(rx1) + half;
    int y1 = toF26Dot6(ry1) + half;
    int x2 = toF26Dot6(rx2) + half;
    int y2 = toF26Dot6(ry2) + half;

    int dx = pkAbs(x2 - x1);
    int dy = pkAbs(y2 - y1);

    PkCosmeticStroker::Point last = stroker->lastPixel;

//    qDebug() << "stroke" << x1/64. << y1/64. << x2/64. << y2/64.;

    if (dx < dy) {
        // vertical
        PkCosmeticStroker::Direction dir = PkCosmeticStroker::TopToBottom;

        bool swapped = false;
        if (y1 > y2) {
            swapped = true;
            std::swap(y1, y2);
            std::swap(x1, x2);
            caps = swapCaps(caps);
            dir = PkCosmeticStroker::BottomToTop;
        }
        int xinc = F16Dot16FixedDiv(x2 - x1, y2 - y1);
        int x = x1 * (1<<10);

        if ((stroker->lastDir ^ PkCosmeticStroker::VerticalMask) == dir)
            caps |= swapped ? PkCosmeticStroker::CapEnd : PkCosmeticStroker::CapBegin;

        capAdjust(caps, y1, y2, x, xinc);

        int y = (y1 + 32) >> 6;
        int ys = (y2 + 32) >> 6;
        int round = (xinc > 0) ? 32 : 0;

        // If capAdjust made us round away from what calculateLastPoint gave us,
        // round back the other way so we start and end on the right point.
        if ((caps & PkCosmeticStroker::CapBegin) && stroker->lastPixel.y == y + 1)
           y++;

        if (y != ys) {
            x += ((y * (1<<6)) + round - y1) * xinc >> 6;

            // calculate first and last pixel and perform dropout control
            PkCosmeticStroker::Point first;
            first.x = x >> 16;
            first.y = y;
            last.x = (x + (ys - y - 1)*xinc) >> 16;
            last.y = ys - 1;
            if (swapped)
                std::swap(first, last);

            bool axisAligned = pkAbs(xinc) < (1 << 14);
            if (stroker->lastPixel.x > INT_MIN) {
                if (first.x == stroker->lastPixel.x &&
                    first.y == stroker->lastPixel.y) {
                    // remove duplicated pixel
                    if (swapped) {
                        --ys;
                    } else {
                        ++y;
                        x += xinc;
                    }
                } else if (stroker->lastDir != dir &&
                           (((axisAligned && stroker->lastAxisAligned) &&
                             stroker->lastPixel.x != first.x && stroker->lastPixel.y != first.y) ||
                            (pkAbs(stroker->lastPixel.x - first.x) > 1 ||
                             pkAbs(stroker->lastPixel.y - first.y) > 1))) {
                    // have a missing pixel, insert it
                    if (swapped) {
                        ++ys;
                    } else {
                        --y;
                        x -= xinc;
                    }
                } else if (stroker->lastDir == dir &&
                           ((pkAbs(stroker->lastPixel.x - first.x) <= 1 &&
                             pkAbs(stroker->lastPixel.y - first.y) > 1))) {
                    x += xinc >> 1;
                    if (swapped)
                        last.x = (x >> 16);
                    else
                        last.x = (x + (ys - y - 1)*xinc) >> 16;
                }
            }
            stroker->lastDir = dir;
            stroker->lastAxisAligned = axisAligned;

            Dasher dasher(stroker, swapped, y * (1<<6), ys * (1<<6));

            do {
                if (dasher.on())
                    drawPixel(stroker, x >> 16, y, 255);
                dasher.adjust();
                x += xinc;
            } while (++y < ys);
            didDraw = true;
        }
    } else {
        // horizontal
        if (!dx)
            return true;

        PkCosmeticStroker::Direction dir = PkCosmeticStroker::LeftToRight;

        bool swapped = false;
        if (x1 > x2) {
            swapped = true;
            std::swap(x1, x2);
            std::swap(y1, y2);
            caps = swapCaps(caps);
            dir = PkCosmeticStroker::RightToLeft;
        }
        int yinc = F16Dot16FixedDiv(y2 - y1, x2 - x1);
        int y = y1 * (1<<10);

        if ((stroker->lastDir ^ PkCosmeticStroker::HorizontalMask) == dir)
            caps |= swapped ? PkCosmeticStroker::CapEnd : PkCosmeticStroker::CapBegin;

        capAdjust(caps, x1, x2, y, yinc);

        int x = (x1 + 32) >> 6;
        int xs = (x2 + 32) >> 6;
        int round = (yinc > 0) ? 32 : 0;

        // If capAdjust made us round away from what calculateLastPoint gave us,
        // round back the other way so we start and end on the right point.
        if ((caps & PkCosmeticStroker::CapBegin) && stroker->lastPixel.x == x + 1)
            x++;

        if (x != xs) {
            y += ((x * (1<<6)) + round - x1) * yinc >> 6;

            // calculate first and last pixel to perform dropout control
            PkCosmeticStroker::Point first;
            first.x = x;
            first.y = y >> 16;
            last.x = xs - 1;
            last.y = (y + (xs - x - 1)*yinc) >> 16;
            if (swapped)
                std::swap(first, last);

            bool axisAligned = pkAbs(yinc) < (1 << 14);
            if (stroker->lastPixel.x > INT_MIN) {
                if (first.x == stroker->lastPixel.x && first.y == stroker->lastPixel.y) {
                    // remove duplicated pixel
                    if (swapped) {
                        --xs;
                    } else {
                        ++x;
                        y += yinc;
                    }
                } else if (stroker->lastDir != dir &&
                           (((axisAligned && stroker->lastAxisAligned) &&
                             stroker->lastPixel.x != first.x && stroker->lastPixel.y != first.y) ||
                            (pkAbs(stroker->lastPixel.x - first.x) > 1 ||
                             pkAbs(stroker->lastPixel.y - first.y) > 1))) {
                    // have a missing pixel, insert it
                    if (swapped) {
                        ++xs;
                    } else {
                        --x;
                        y -= yinc;
                    }
                } else if (stroker->lastDir == dir &&
                           ((pkAbs(stroker->lastPixel.x - first.x) <= 1 &&
                             pkAbs(stroker->lastPixel.y - first.y) > 1))) {
                    y += yinc >> 1;
                    if (swapped)
                        last.y = (y >> 16);
                    else
                        last.y = (y + (xs - x - 1)*yinc) >> 16;
                }
            }
            stroker->lastDir = dir;
            stroker->lastAxisAligned = axisAligned;

            Dasher dasher(stroker, swapped, x * (1<<6), xs * (1<<6));

            do {
                if (dasher.on())
                    drawPixel(stroker, x, y >> 16, 255);
                dasher.adjust();
                y += yinc;
            } while (++x < xs);
            didDraw = true;
        }
    }
    stroker->lastPixel = last;
    return didDraw;
}


template<DrawPixel drawPixel, class Dasher>
static bool drawLineAA(PkCosmeticStroker *stroker, qreal rx1, qreal ry1, qreal rx2, qreal ry2, int caps)
{
    if (stroker->clipLine(rx1, ry1, rx2, ry2))
        return true;

    int x1 = toF26Dot6(rx1);
    int y1 = toF26Dot6(ry1);
    int x2 = toF26Dot6(rx2);
    int y2 = toF26Dot6(ry2);

    int dx = x2 - x1;
    int dy = y2 - y1;

    if (pkAbs(dx) < pkAbs(dy)) {
        // vertical

        int xinc = F16Dot16FixedDiv(dx, dy);

        bool swapped = false;
        if (y1 > y2) {
            std::swap(y1, y2);
            std::swap(x1, x2);
            swapped = true;
            caps = swapCaps(caps);
        }

        int x = (x1 - 32) * (1<<10);
        x -= ( ((y1 & 63) - 32)  * xinc ) >> 6;

        capAdjust(caps, y1, y2, x, xinc);

        Dasher dasher(stroker, swapped, y1, y2);

        int y = y1 >> 6;
        int ys = y2 >> 6;

        int alphaStart, alphaEnd;
        if (y == ys) {
            alphaStart = y2 - y1;
            assert(alphaStart >= 0 && alphaStart < 64);
            alphaEnd = 0;
        } else {
            alphaStart = 64 - (y1 & 63);
            alphaEnd = (y2 & 63);
        }
//        qDebug() << "vertical" << x1/64. << y1/64. << x2/64. << y2/64.;
//        qDebug() << "          x=" << x << "dx=" << dx << "xi=" << (x>>16) << "xsi=" << ((x+(ys-y)*dx)>>16) << "y=" << y << "ys=" << ys;

        // draw first pixel
        if (dasher.on()) {
            uint alpha = (quint8)(x >> 8);
            drawPixel(stroker, x>>16, y, (255-alpha) * alphaStart >> 6);
            drawPixel(stroker, (x>>16) + 1, y, alpha * alphaStart >> 6);
        }
        dasher.adjust();
        x += xinc;
        ++y;
        if (y < ys) {
            do {
                if (dasher.on()) {
                    uint alpha = (quint8)(x >> 8);
                    drawPixel(stroker, x>>16, y, (255-alpha));
                    drawPixel(stroker, (x>>16) + 1, y, alpha);
                }
                dasher.adjust();
                x += xinc;
            } while (++y < ys);
        }
        // draw last pixel
        if (alphaEnd && dasher.on()) {
            uint alpha = (quint8)(x >> 8);
            drawPixel(stroker, x>>16, y, (255-alpha) * alphaEnd >> 6);
            drawPixel(stroker, (x>>16) + 1, y, alpha * alphaEnd >> 6);
        }
    } else {
        // horizontal
        if (!dx)
            return true;

        int yinc = F16Dot16FixedDiv(dy, dx);

        bool swapped = false;
        if (x1 > x2) {
            std::swap(x1, x2);
            std::swap(y1, y2);
            swapped = true;
            caps = swapCaps(caps);
        }

        int y = (y1 - 32) * (1<<10);
        y -= ( ((x1 & 63) - 32)  * yinc ) >> 6;

        capAdjust(caps, x1, x2, y, yinc);

        Dasher dasher(stroker, swapped, x1, x2);

        int x = x1 >> 6;
        int xs = x2 >> 6;

//        qDebug() << "horizontal" << x1/64. << y1/64. << x2/64. << y2/64.;
//        qDebug() << "          y=" << y << "dy=" << dy << "x=" << x << "xs=" << xs << "yi=" << (y>>16) << "ysi=" << ((y+(xs-x)*dy)>>16);
        int alphaStart, alphaEnd;
        if (x == xs) {
            alphaStart = x2 - x1;
            assert(alphaStart >= 0 && alphaStart < 64);
            alphaEnd = 0;
        } else {
            alphaStart = 64 - (x1 & 63);
            alphaEnd = (x2 & 63);
        }

        // draw first pixel
        if (dasher.on()) {
            uint alpha = (quint8)(y >> 8);
            drawPixel(stroker, x, y>>16, (255-alpha) * alphaStart >> 6);
            drawPixel(stroker, x, (y>>16) + 1, alpha * alphaStart >> 6);
        }
        dasher.adjust();
        y += yinc;
        ++x;
        // draw line
        if (x < xs) {
            do {
                if (dasher.on()) {
                    uint alpha = (quint8)(y >> 8);
                    drawPixel(stroker, x, y>>16, (255-alpha));
                    drawPixel(stroker, x, (y>>16) + 1, alpha);
                }
                dasher.adjust();
                y += yinc;
            } while (++x < xs);
        }
        // draw last pixel
        if (alphaEnd && dasher.on()) {
            uint alpha = (quint8)(y >> 8);
            drawPixel(stroker, x, y>>16, (255-alpha) * alphaEnd >> 6);
            drawPixel(stroker, x, (y>>16) + 1, alpha * alphaEnd >> 6);
        }
    }
    return true;
}
