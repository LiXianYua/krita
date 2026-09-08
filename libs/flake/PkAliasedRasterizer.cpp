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

#include "PkAliasedRasterizer.h"
#include <PkVector.h>
#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <climits>
#include <new>

template<class T> static T *pkRasterCheckPtr(T *p) { if (!p) throw std::bad_alloc(); return p; }

typedef int Pk16Dot16;
#define Pk16Dot16ToFloat(i) ((i)/65536.)
#define FloatToPk16Dot16(i) (int)((i) * 65536.)
#define IntToPk16Dot16(i) ((i) * (1 << 16))
#define Pk16Dot16ToInt(i) ((i) >> 16)
#define Pk16Dot16Factor 65536

#define Pk16Dot16Multiply(x, y) (int)((qlonglong(x) * qlonglong(y)) >> 16)
#define Pk16Dot16FastMultiply(x, y) (((x) * (y)) >> 16)

#define SPAN_BUFFER_SIZE 256

#define COORD_ROUNDING 1 // 0: round up, 1: round down
#define COORD_OFFSET 32 // 26.6, 32 is half a pixel

static inline PK_FT_Vector PointToVector(const PkPointF &p)
{
    PK_FT_Vector result = { PK_FT_Pos(p.x() * 64), PK_FT_Pos(p.y() * 64) };
    return result;
}

class PkSpanBuffer {
public:
    PkSpanBuffer(ProcessSpans blend, void *data, const PkRect &clipRect)
        : m_spanCount(0)
        , m_blend(blend)
        , m_data(data)
        , m_clipRect(clipRect)
    {
    }

    ~PkSpanBuffer()
    {
        flushSpans();
    }

    void addSpan(int x, unsigned int len, int y, unsigned char coverage)
    {
        if (!coverage || !len)
            return;

        assert(y >= m_clipRect.top());
        assert(y <= m_clipRect.bottom());
        assert(x >= m_clipRect.left());
        assert(x + int(len) - 1 <= m_clipRect.right());

        m_spans[m_spanCount].x = x;
        m_spans[m_spanCount].len = len;
        m_spans[m_spanCount].y = y;
        m_spans[m_spanCount].coverage = coverage;

        if (++m_spanCount == SPAN_BUFFER_SIZE)
            flushSpans();
    }

private:
    void flushSpans()
    {
        m_blend(m_spanCount, m_spans, m_data);
        m_spanCount = 0;
    }

    PK_FT_Span m_spans[SPAN_BUFFER_SIZE];
    int m_spanCount;

    ProcessSpans m_blend;
    void *m_data;

    PkRect m_clipRect;
};

#define CHUNK_SIZE 64
class PkScanConverter
{
public:
    PkScanConverter();
    ~PkScanConverter();

    void begin(int top, int bottom, int left, int right,
               Pk::FillRule fillRule, bool legacyRounding, PkSpanBuffer *spanBuffer);
    void end();

    void mergeCurve(const PK_FT_Vector &a, const PK_FT_Vector &b,
                    const PK_FT_Vector &c, const PK_FT_Vector &d);
    void mergeLine(PK_FT_Vector a, PK_FT_Vector b);

    struct Line
    {
        Pk16Dot16 x;
        Pk16Dot16 delta;

        int top, bottom;

        int winding;
    };

private:
    struct Intersection
    {
        int x;
        int winding;

        int left, right;
    };

    inline bool clip(Pk16Dot16 &xFP, int &iTop, int &iBottom, Pk16Dot16 slopeFP, Pk16Dot16 edgeFP, int winding);
    inline void mergeIntersection(Intersection *head, const Intersection &isect);

    void prepareChunk();

    void emitNode(const Intersection *node);
    void emitSpans(int chunk);

    inline void allocate(int size);

    PkVector<Line> m_lines;

    int m_alloc;
    int m_size;

    int m_top;
    int m_bottom;

    Pk16Dot16 m_leftFP;
    Pk16Dot16 m_rightFP;

    int m_fillRuleMask;
    bool m_legacyRounding;

    int m_x;
    int m_y;
    int m_winding;

    Intersection *m_intersections;

    PkSpanBuffer *m_spanBuffer;

    PkVector<Line *> m_active;

    template <typename T>
    friend void pkScanConvert(PkScanConverter &d, T allVertical);
};

class PkAliasedRasterizerPrivate
{
public:
    bool antialiased;
    bool legacyRounding;
    ProcessSpans blend;
    void *data;
    PkRect clipRect;

    PkScanConverter scanConverter;
};

PkScanConverter::PkScanConverter()
   : m_lines(0)
   , m_alloc(0)
   , m_size(0)
   , m_intersections(nullptr)
   , m_active(0)
{
}

PkScanConverter::~PkScanConverter()
{
    if (m_intersections)
        free(m_intersections);
}

void PkScanConverter::begin(int top, int bottom, int left, int right,
                           Pk::FillRule fillRule, bool legacyRounding,
                           PkSpanBuffer *spanBuffer)
{
    m_top = top;
    m_bottom = bottom;
    m_leftFP = IntToPk16Dot16(left);
    m_rightFP = IntToPk16Dot16(right + 1);

    m_lines.clear();

    m_fillRuleMask = fillRule == Pk::WindingFill ? ~0x0 : 0x1;
    m_legacyRounding = legacyRounding;
    m_spanBuffer = spanBuffer;
}

void PkScanConverter::prepareChunk()
{
    m_size = CHUNK_SIZE;

    allocate(CHUNK_SIZE);
    memset(m_intersections, 0, CHUNK_SIZE * sizeof(Intersection));
}

void PkScanConverter::emitNode(const Intersection *node)
{
tail_call:
    if (node->left)
        emitNode(node + node->left);

    if (m_winding & m_fillRuleMask)
        m_spanBuffer->addSpan(m_x, node->x - m_x, m_y, 0xff);

    m_x = node->x;
    m_winding += node->winding;

    if (node->right) {
        node += node->right;
        goto tail_call;
    }
}

void PkScanConverter::emitSpans(int chunk)
{
    for (int dy = 0; dy < CHUNK_SIZE; ++dy) {
        m_x = 0;
        m_y = chunk + dy;
        m_winding = 0;

        emitNode(&m_intersections[dy]);
    }
}

// split control points b[0] ... b[3] into
// left (b[0] ... b[3]) and right (b[3] ... b[6])
static void split(PK_FT_Vector *b)
{
    b[6] = b[3];

    {
        const PK_FT_Pos temp = (b[1].x + b[2].x)/2;

        b[1].x = (b[0].x + b[1].x)/2;
        b[5].x = (b[2].x + b[3].x)/2;
        b[2].x = (b[1].x + temp)/2;
        b[4].x = (b[5].x + temp)/2;
        b[3].x = (b[2].x + b[4].x)/2;
    }
    {
        const PK_FT_Pos temp = (b[1].y + b[2].y)/2;

        b[1].y = (b[0].y + b[1].y)/2;
        b[5].y = (b[2].y + b[3].y)/2;
        b[2].y = (b[1].y + temp)/2;
        b[4].y = (b[5].y + temp)/2;
        b[3].y = (b[2].y + b[4].y)/2;
    }
}

static inline bool topOrder(const PkScanConverter::Line &a, const PkScanConverter::Line &b)
{
    return a.top < b.top;
}

static inline bool xOrder(const PkScanConverter::Line *a, const PkScanConverter::Line *b)
{
    return a->x < b->x;
}

template <bool B>
struct PkBoolToType
{
    inline bool operator()() const
    {
        return B;
    }
};

// should be a member function but VC6 doesn't support member template functions
template <typename T>
void pkScanConvert(PkScanConverter &d, T allVertical)
{
    if (!d.m_lines.size()) {
        d.m_active.clear();
        return;
    }
    std::sort(d.m_lines.data(), d.m_lines.data() + d.m_lines.size(), topOrder);
    int line = 0;
    for (int y = d.m_lines.first().top; y <= d.m_bottom; ++y) {
        for (; line < d.m_lines.size() && d.m_lines.operator[](line).top == y; ++line) {
            // add node to active list
            if (allVertical()) {
                PkScanConverter::Line *l = &d.m_lines.operator[](line);
                d.m_active.resize(d.m_active.size() + 1);
                int j;
                for (j = d.m_active.size() - 2; j >= 0 && xOrder(l, d.m_active.operator[](j)); --j)
                    d.m_active.operator[](j+1) = d.m_active.operator[](j);
                d.m_active.operator[](j+1) = l;
            } else {
                d.m_active << &d.m_lines.operator[](line);
            }
        }

        int numActive = d.m_active.size();
        if (!allVertical()) {
        // use insertion sort instead of qSort, as the active edge list is quite small
        // and in the average case already sorted
            for (int i = 1; i < numActive; ++i) {
                PkScanConverter::Line *l = d.m_active.operator[](i);
                int j;
                for (j = i-1; j >= 0 && xOrder(l, d.m_active.operator[](j)); --j)
                    d.m_active.operator[](j+1) = d.m_active.operator[](j);
                d.m_active.operator[](j+1) = l;
            }
        }

        int x = 0;
        int winding = 0;
        for (int i = 0; i < numActive; ++i) {
            PkScanConverter::Line *node = d.m_active.operator[](i);

            const int current = Pk16Dot16ToInt(node->x);
            if (winding & d.m_fillRuleMask)
                d.m_spanBuffer->addSpan(x, current - x, y, 0xff);

            x = current;
            winding += node->winding;

            if (node->bottom == y) {
                // remove node from active list
                for (int j = i; j < numActive - 1; ++j)
                    d.m_active.operator[](j) = d.m_active.operator[](j+1);

                d.m_active.resize(--numActive);
                --i;
            } else if (!allVertical())
                node->x += node->delta;
        }
    }
    d.m_active.clear();
}

void PkScanConverter::end()
{
    if (m_lines.isEmpty())
        return;

    if (m_lines.size() <= 32) {
        bool allVertical = true;
        for (int i = 0; i < m_lines.size(); ++i) {
            if (m_lines.operator[](i).delta) {
                allVertical = false;
                break;
            }
        }
        if (allVertical)
            pkScanConvert(*this, PkBoolToType<true>());
        else
            pkScanConvert(*this, PkBoolToType<false>());
    } else {
        for (int chunkTop = m_top; chunkTop <= m_bottom; chunkTop += CHUNK_SIZE) {
            prepareChunk();

            Intersection isect = { 0, 0, 0, 0 };

            const int chunkBottom = chunkTop + CHUNK_SIZE;
            for (int i = 0; i < m_lines.size(); ++i) {
                Line &line = m_lines.operator[](i);

                if ((line.bottom < chunkTop) || (line.top > chunkBottom))
                    continue;

                const int top = pkMax(0, line.top - chunkTop);
                const int bottom = pkMin(CHUNK_SIZE, line.bottom + 1 - chunkTop);
                allocate(m_size + bottom - top);

                isect.winding = line.winding;

                Intersection *it = m_intersections + top;
                Intersection *end = m_intersections + bottom;

                if (line.delta) {
                    for (; it != end; ++it) {
                        isect.x = Pk16Dot16ToInt(line.x);
                        line.x += line.delta;
                        mergeIntersection(it, isect);
                    }
                } else {
                    isect.x = Pk16Dot16ToInt(line.x);
                    for (; it != end; ++it)
                        mergeIntersection(it, isect);
                }
            }

            emitSpans(chunkTop);
        }
    }

    if (m_alloc > 1024) {
        free(m_intersections);
        m_alloc = 0;
        m_size = 0;
        m_intersections = nullptr;
    }

    if (m_lines.size() > 1024)
        m_lines.resize(1024);
}

inline void PkScanConverter::allocate(int size)
{
    if (m_alloc < size) {
        int newAlloc = pkMax(size, 2 * m_alloc);
        m_intersections = pkRasterCheckPtr((Intersection *)realloc(m_intersections, newAlloc * sizeof(Intersection)));
        m_alloc = newAlloc;
    }
}

inline void PkScanConverter::mergeIntersection(Intersection *it, const Intersection &isect)
{
    Intersection *current = it;

    while (isect.x != current->x) {
        int &next = isect.x < current->x ? current->left : current->right;
        if (next)
            current += next;
        else {
            Intersection *last = m_intersections + m_size;
            next = last - current;
            *last = isect;
            ++m_size;
            return;
        }
    }

    current->winding += isect.winding;
}

void PkScanConverter::mergeCurve(const PK_FT_Vector &pa, const PK_FT_Vector &pb,
                                const PK_FT_Vector &pc, const PK_FT_Vector &pd)
{
    // make room for 32 splits
    PK_FT_Vector beziers[4 + 3 * 32];

    PK_FT_Vector *b = beziers;

    b[0] = pa;
    b[1] = pb;
    b[2] = pc;
    b[3] = pd;

    const PK_FT_Pos flatness = 16;

    while (b >= beziers) {
        PK_FT_Vector delta = { b[3].x - b[0].x, b[3].y - b[0].y };
        PK_FT_Pos l = pkAbs(delta.x) + pkAbs(delta.y);

        bool belowThreshold;
        if (l > 64) {
            qlonglong d2 = pkAbs(qlonglong(b[1].x-b[0].x) * qlonglong(delta.y) -
                                qlonglong(b[1].y-b[0].y) * qlonglong(delta.x));
            qlonglong d3 = pkAbs(qlonglong(b[2].x-b[0].x) * qlonglong(delta.y) -
                                qlonglong(b[2].y-b[0].y) * qlonglong(delta.x));

            qlonglong d = d2 + d3;

            belowThreshold = (d <= qlonglong(flatness) * qlonglong(l));
        } else {
            PK_FT_Pos d = pkAbs(b[0].x-b[1].x) + pkAbs(b[0].y-b[1].y) +
                          pkAbs(b[0].x-b[2].x) + pkAbs(b[0].y-b[2].y);

            belowThreshold = (d <= flatness);
        }

        if (belowThreshold || b == beziers + 3 * 32) {
            mergeLine(b[0], b[3]);
            b -= 3;
            continue;
        }

        split(b);
        b += 3;
    }
}

inline bool PkScanConverter::clip(Pk16Dot16 &xFP, int &iTop, int &iBottom, Pk16Dot16 slopeFP, Pk16Dot16 edgeFP, int winding)
{
    bool right = edgeFP == m_rightFP;

    if (xFP == edgeFP) {
        if ((slopeFP > 0) ^ right)
            return false;
        else {
            Line line = { edgeFP, 0, iTop, iBottom, winding };
            m_lines.append(line);
            return true;
        }
    }

    Pk16Dot16 lastFP = xFP + slopeFP * (iBottom - iTop);

    if (lastFP == edgeFP) {
        if ((slopeFP < 0) ^ right)
            return false;
        else {
            Line line = { edgeFP, 0, iTop, iBottom, winding };
            m_lines.append(line);
            return true;
        }
    }

    // does line cross edge?
    if ((lastFP < edgeFP) ^ (xFP < edgeFP)) {
        Pk16Dot16 deltaY = Pk16Dot16((edgeFP - xFP) / Pk16Dot16ToFloat(slopeFP));

        if ((xFP < edgeFP) ^ right) {
            // top segment needs to be clipped
            int iHeight = Pk16Dot16ToInt(deltaY + 1);
            int iMiddle = iTop + iHeight;

            Line line = { edgeFP, 0, iTop, iMiddle, winding };
            m_lines.append(line);

            if (iMiddle != iBottom) {
                xFP += slopeFP * (iHeight + 1);
                iTop = iMiddle + 1;
            } else
                return true;
        } else {
            // bottom segment needs to be clipped
            int iHeight = Pk16Dot16ToInt(deltaY);
            int iMiddle = iTop + iHeight;

            if (iMiddle != iBottom) {
                Line line = { edgeFP, 0, iMiddle + 1, iBottom, winding };
                m_lines.append(line);

                iBottom = iMiddle;
            }
        }
        return false;
    } else if ((xFP < edgeFP) ^ right) {
        Line line = { edgeFP, 0, iTop, iBottom, winding };
        m_lines.append(line);
        return true;
    }

    return false;
}

void PkScanConverter::mergeLine(PK_FT_Vector a, PK_FT_Vector b)
{
    int winding = 1;

    if (a.y > b.y) {
        std::swap(a, b);
        winding = -1;
    }

    if (m_legacyRounding) {
        a.x += COORD_OFFSET;
        a.y += COORD_OFFSET;
        b.x += COORD_OFFSET;
        b.y += COORD_OFFSET;
    }

    int rounding = m_legacyRounding ? COORD_ROUNDING : 0;

    int iTop = pkMax(m_top, int((a.y + 32 - rounding) >> 6));
    int iBottom = pkMin(m_bottom, int((b.y - 32 - rounding) >> 6));

    if (iTop <= iBottom) {
        Pk16Dot16 aFP = Pk16Dot16Factor/2 + (a.x * (1 << 10)) - rounding;

        if (b.x == a.x) {
            Line line = { pkBound(m_leftFP, aFP, m_rightFP), 0, iTop, iBottom, winding };
            m_lines.append(line);
        } else {
            const qreal slope = (b.x - a.x) / qreal(b.y - a.y);

            const Pk16Dot16 slopeFP = FloatToPk16Dot16(slope);

            Pk16Dot16 xFP = aFP + Pk16Dot16Multiply(slopeFP,
                                                  IntToPk16Dot16(iTop)
                                                  + Pk16Dot16Factor/2 - (a.y * (1 << 10)));

            if (clip(xFP, iTop, iBottom, slopeFP, m_leftFP, winding))
                return;

            if (clip(xFP, iTop, iBottom, slopeFP, m_rightFP, winding))
                return;

            assert(xFP >= m_leftFP);

            Line line = { xFP, slopeFP, iTop, iBottom, winding };
            m_lines.append(line);
        }
    }
}

PkAliasedRasterizer::PkAliasedRasterizer()
    : d(new PkAliasedRasterizerPrivate)
{
    d->legacyRounding = false;
}

PkAliasedRasterizer::~PkAliasedRasterizer()
{
    delete d;
}

void PkAliasedRasterizer::setAntialiased(bool antialiased)
{
    d->antialiased = antialiased;
}

void PkAliasedRasterizer::initialize(ProcessSpans blend, void *data)
{
    d->blend = blend;
    d->data = data;
}

void PkAliasedRasterizer::setClipRect(const PkRect &clipRect)
{
    d->clipRect = clipRect;
}

void PkAliasedRasterizer::setLegacyRoundingEnabled(bool legacyRoundingEnabled)
{
    d->legacyRounding = legacyRoundingEnabled;
}

void PkAliasedRasterizer::rasterize(const PK_FT_Outline *outline, Pk::FillRule fillRule)
{
    if (outline->n_points < 3 || outline->n_contours == 0)
        return;

    const PK_FT_Vector *points = outline->points;

    PkSpanBuffer buffer(d->blend, d->data, d->clipRect);

    // ### PK_FT_Outline already has a bounding rect which is
    // ### precomputed at this point, so we should probably just be
    // ### using that instead...
    PK_FT_Pos min_y = points[0].y, max_y = points[0].y;
    for (int i = 1; i < outline->n_points; ++i) {
        const PK_FT_Vector &p = points[i];
        min_y = pkMin(p.y, min_y);
        max_y = pkMax(p.y, max_y);
    }

    int rounding = d->legacyRounding ? COORD_OFFSET - COORD_ROUNDING : 0;

    int iTopBound = pkMax(d->clipRect.top(), int((min_y + 32 + rounding) >> 6));
    int iBottomBound = pkMin(d->clipRect.bottom(), int((max_y - 32 + rounding) >> 6));

    if (iTopBound > iBottomBound)
        return;

    d->scanConverter.begin(iTopBound, iBottomBound, d->clipRect.left(), d->clipRect.right(), fillRule, d->legacyRounding, &buffer);

    int first = 0;
    for (int i = 0; i < outline->n_contours; ++i) {
        const int last = outline->contours[i];
        for (int j = first; j < last; ++j) {
            if (outline->tags[j+1] == PK_FT_CURVE_TAG_CUBIC) {
                assert(outline->tags[j+2] == PK_FT_CURVE_TAG_CUBIC);
                d->scanConverter.mergeCurve(points[j], points[j+1], points[j+2], points[j+3]);
                j += 2;
            } else {
                d->scanConverter.mergeLine(points[j], points[j+1]);
            }
        }

        first = last + 1;
    }

    d->scanConverter.end();
}
static Pk16Dot16 intersectPixelFP(int x, Pk16Dot16 top, Pk16Dot16 bottom, Pk16Dot16 leftIntersectX, Pk16Dot16 rightIntersectX, Pk16Dot16 slope, Pk16Dot16 invSlope)
{
    Pk16Dot16 leftX = IntToPk16Dot16(x);
    Pk16Dot16 rightX = IntToPk16Dot16(x) + Pk16Dot16Factor;

    Pk16Dot16 leftIntersectY, rightIntersectY;
    if (slope > 0) {
        leftIntersectY = top + Pk16Dot16Multiply(leftX - leftIntersectX, invSlope);
        rightIntersectY = leftIntersectY + invSlope;
    } else {
        leftIntersectY = top + Pk16Dot16Multiply(leftX - rightIntersectX, invSlope);
        rightIntersectY = leftIntersectY + invSlope;
    }

    if (leftIntersectX >= leftX && rightIntersectX <= rightX) {
        return Pk16Dot16Multiply(bottom - top, leftIntersectX - leftX + ((rightIntersectX - leftIntersectX) >> 1));
    } else if (leftIntersectX >= rightX) {
        return bottom - top;
    } else if (leftIntersectX >= leftX) {
        if (slope > 0) {
            return (bottom - top) - Pk16Dot16FastMultiply((rightX - leftIntersectX) >> 1, rightIntersectY - top);
        } else {
            return (bottom - top) - Pk16Dot16FastMultiply((rightX - leftIntersectX) >> 1, bottom - rightIntersectY);
        }
    } else if (rightIntersectX <= leftX) {
        return 0;
    } else if (rightIntersectX <= rightX) {
        if (slope > 0) {
            return Pk16Dot16FastMultiply((rightIntersectX - leftX) >> 1, bottom - leftIntersectY);
        } else {
            return Pk16Dot16FastMultiply((rightIntersectX - leftX) >> 1, leftIntersectY - top);
        }
    } else {
        if (slope > 0) {
            return (bottom - rightIntersectY) + ((rightIntersectY - leftIntersectY) >> 1);
        } else {
            return (rightIntersectY - top) + ((leftIntersectY - rightIntersectY) >> 1);
        }
    }
}

static inline bool q26Dot6Compare(qreal p1, qreal p2)
{
    return int((p2  - p1) * 64.) == 0;
}

static inline PkPointF snapTo26Dot6Grid(const PkPointF &p)
{
    return PkPointF(std::floor(p.x() * 64) * (1 / qreal(64)),
                   std::floor(p.y() * 64) * (1 / qreal(64)));
}

/*
   The rasterize line function relies on some div by zero which should
   result in +/-inf values. However, when floating point exceptions are
   enabled, this will cause crashes, so we return high numbers instead.
   As the returned value is used in further arithmetic, returning
   FLT_MAX/DBL_MAX will also cause values, so instead return a value
   that is well outside the int-range.
 */
static inline qreal qSafeDivide(qreal x, qreal y)
{
    if (y == 0)
        return x > 0 ? 1e20 : -1e20;
    return x / y;
}

/* Conversion to int fails if the value is too large to fit into INT_MAX or
   too small to fit into INT_MIN, so we need this slightly safer conversion
   when floating point exceptions are enabled
 */
static inline int qSafeFloatToPk16Dot16(qreal x)
{
    qreal tmp = x * 65536.;
    if (tmp > qreal(INT_MAX))
        return INT_MAX;
    else if (tmp < qreal(INT_MIN))
        return -INT_MAX;
    return int(tmp);
}

void PkAliasedRasterizer::rasterizeLine(const PkPointF &a, const PkPointF &b, qreal width, bool squareCap)
{
    if (a == b || !(width > 0.0) || d->clipRect.isEmpty())
        return;

    PkPointF pa = a;
    PkPointF pb = b;

    if (squareCap) {
        PkPointF delta = pb - pa;
        pa -= (0.5f * width) * delta;
        pb += (0.5f * width) * delta;
    }

    PkPointF offs = PkPointF(pkAbs(b.y() - a.y()), pkAbs(b.x() - a.x())) * width * 0.5;
    const PkRectF clip(d->clipRect.topLeft() - offs, d->clipRect.bottomRight() + PkPoint(1, 1) + offs);

    if (!clip.contains(pa) || !clip.contains(pb)) {
        qreal t1 = 0;
        qreal t2 = 1;

        const qreal o[2] = { pa.x(), pa.y() };
        const qreal d[2] = { pb.x() - pa.x(), pb.y() - pa.y() };

        const qreal low[2] = { clip.left(), clip.top() };
        const qreal high[2] = { clip.right(), clip.bottom() };

        for (int i = 0; i < 2; ++i) {
            if (d[i] == 0) {
                if (o[i] <= low[i] || o[i] >= high[i])
                    return;
                continue;
            }
            const qreal d_inv = 1 / d[i];
            qreal t_low = (low[i] - o[i]) * d_inv;
            qreal t_high = (high[i] - o[i]) * d_inv;
            if (t_low > t_high)
                std::swap(t_low, t_high);
            if (t1 < t_low)
                t1 = t_low;
            if (t2 > t_high)
                t2 = t_high;
            if (t1 >= t2)
                return;
        }

        PkPointF npa = pa + (pb - pa) * t1;
        PkPointF npb = pa + (pb - pa) * t2;

        pa = npa;
        pb = npb;
    }

    if (!d->antialiased && d->legacyRounding) {
        pa.rx() += (COORD_OFFSET - COORD_ROUNDING)/64.;
        pa.ry() += (COORD_OFFSET - COORD_ROUNDING)/64.;
        pb.rx() += (COORD_OFFSET - COORD_ROUNDING)/64.;
        pb.ry() += (COORD_OFFSET - COORD_ROUNDING)/64.;
    }

    {
        // old delta
        const PkPointF d0 = a - b;
        const qreal w0 = d0.x() * d0.x() + d0.y() * d0.y();

        // new delta
        const PkPointF d = pa - pb;
        const qreal w = d.x() * d.x() + d.y() * d.y();

        if (w == 0)
            return;

        // adjust width which is given relative to |b - a|
        width *= std::sqrt(w0 / w);
    }

    PkSpanBuffer buffer(d->blend, d->data, d->clipRect);

    if (q26Dot6Compare(pa.y(), pb.y())) {
        const qreal x = (pa.x() + pb.x()) * 0.5f;
        const qreal dx = pkAbs(pb.x() - pa.x()) * 0.5f;

        const qreal y = pa.y();
        const qreal dy = width * dx;

        pa = PkPointF(x, y - dy);
        pb = PkPointF(x, y + dy);

        width = 1 / width;
    }

    if (q26Dot6Compare(pa.x(), pb.x())) {
        if (pa.y() > pb.y())
            std::swap(pa, pb);

        const qreal dy = pb.y() - pa.y();
        const qreal halfWidth = 0.5f * width * dy;

        qreal left = pa.x() - halfWidth;
        qreal right = pa.x() + halfWidth;

        left = pkBound(qreal(d->clipRect.left()), left, qreal(d->clipRect.right() + 1));
        right = pkBound(qreal(d->clipRect.left()), right, qreal(d->clipRect.right() + 1));

        pa.ry() = pkBound(qreal(d->clipRect.top()), pa.y(), qreal(d->clipRect.bottom() + 1));
        pb.ry() = pkBound(qreal(d->clipRect.top()), pb.y(), qreal(d->clipRect.bottom() + 1));

        if (q26Dot6Compare(left, right) || q26Dot6Compare(pa.y(), pb.y()))
            return;

        if (d->antialiased) {
            const Pk16Dot16 iLeft = int(left);
            const Pk16Dot16 iRight = int(right);
            const Pk16Dot16 leftWidth = IntToPk16Dot16(iLeft + 1)
                                       - qSafeFloatToPk16Dot16(left);
            const Pk16Dot16 rightWidth = qSafeFloatToPk16Dot16(right)
                                        - IntToPk16Dot16(iRight);

            Pk16Dot16 coverage[3];
            int x[3];
            int len[3];

            int n = 1;
            if (iLeft == iRight) {
                coverage[0] = (leftWidth + rightWidth) * 255;
                x[0] = iLeft;
                len[0] = 1;
            } else {
                coverage[0] = leftWidth * 255;
                x[0] = iLeft;
                len[0] = 1;
                if (leftWidth == Pk16Dot16Factor) {
                    len[0] = iRight - iLeft;
                } else if (iRight - iLeft > 1) {
                    coverage[1] = IntToPk16Dot16(255);
                    x[1] = iLeft + 1;
                    len[1] = iRight - iLeft - 1;
                    ++n;
                }
                if (rightWidth) {
                    coverage[n] = rightWidth * 255;
                    x[n] = iRight;
                    len[n] = 1;
                    ++n;
                }
            }

            const Pk16Dot16 iTopFP = IntToPk16Dot16(int(pa.y()));
            const Pk16Dot16 iBottomFP = IntToPk16Dot16(int(pb.y()));
            const Pk16Dot16 yPa = qSafeFloatToPk16Dot16(pa.y());
            const Pk16Dot16 yPb = qSafeFloatToPk16Dot16(pb.y());
            for (Pk16Dot16 yFP = iTopFP; yFP <= iBottomFP; yFP += Pk16Dot16Factor) {
                const Pk16Dot16 rowHeight = pkMin(yFP + Pk16Dot16Factor, yPb)
                                           - pkMax(yFP, yPa);
                const int y = Pk16Dot16ToInt(yFP);
                if (y > d->clipRect.bottom())
                    break;
                for (int i = 0; i < n; ++i) {
                    buffer.addSpan(x[i], len[i], y,
                                   Pk16Dot16ToInt(Pk16Dot16Multiply(rowHeight, coverage[i])));
                }
            }
        } else { // aliased
            int iTop = int(pa.y() + 0.5f);
            int iBottom = pb.y() < 0.5f ? -1 : int(pb.y() - 0.5f);
            int iLeft = int(left + 0.5f);
            int iRight = right < 0.5f ? -1 : int(right - 0.5f);

            int iWidth = iRight - iLeft + 1;
            for (int y = iTop; y <= iBottom; ++y)
                buffer.addSpan(iLeft, iWidth, y, 255);
        }
    } else {
        if (pa.y() > pb.y())
            std::swap(pa, pb);

        PkPointF delta = pb - pa;
        delta *= 0.5f * width;
        const PkPointF perp(delta.y(), -delta.x());

        PkPointF top;
        PkPointF left;
        PkPointF right;
        PkPointF bottom;

        if (pa.x() < pb.x()) {
            top = pa + perp;
            left = pa - perp;
            right = pb + perp;
            bottom = pb - perp;
        } else {
            top = pa - perp;
            left = pb - perp;
            right = pa + perp;
            bottom = pb + perp;
        }

        top = snapTo26Dot6Grid(top);
        bottom = snapTo26Dot6Grid(bottom);
        left = snapTo26Dot6Grid(left);
        right = snapTo26Dot6Grid(right);

        const qreal topBound = pkBound(qreal(d->clipRect.top()), top.y(), qreal(d->clipRect.bottom()));
        const qreal bottomBound = pkBound(qreal(d->clipRect.top()), bottom.y(), qreal(d->clipRect.bottom()));

        const PkPointF topLeftEdge = left - top;
        const PkPointF topRightEdge = right - top;
        const PkPointF bottomLeftEdge = bottom - left;
        const PkPointF bottomRightEdge = bottom - right;

        const qreal topLeftSlope = qSafeDivide(topLeftEdge.x(), topLeftEdge.y());
        const qreal bottomLeftSlope = qSafeDivide(bottomLeftEdge.x(), bottomLeftEdge.y());

        const qreal topRightSlope = qSafeDivide(topRightEdge.x(), topRightEdge.y());
        const qreal bottomRightSlope = qSafeDivide(bottomRightEdge.x(), bottomRightEdge.y());

        const Pk16Dot16 topLeftSlopeFP = qSafeFloatToPk16Dot16(topLeftSlope);
        const Pk16Dot16 topRightSlopeFP = qSafeFloatToPk16Dot16(topRightSlope);

        const Pk16Dot16 bottomLeftSlopeFP = qSafeFloatToPk16Dot16(bottomLeftSlope);
        const Pk16Dot16 bottomRightSlopeFP = qSafeFloatToPk16Dot16(bottomRightSlope);

        const Pk16Dot16 invTopLeftSlopeFP = qSafeFloatToPk16Dot16(qSafeDivide(1, topLeftSlope));
        const Pk16Dot16 invTopRightSlopeFP = qSafeFloatToPk16Dot16(qSafeDivide(1, topRightSlope));

        const Pk16Dot16 invBottomLeftSlopeFP = qSafeFloatToPk16Dot16(qSafeDivide(1, bottomLeftSlope));
        const Pk16Dot16 invBottomRightSlopeFP = qSafeFloatToPk16Dot16(qSafeDivide(1, bottomRightSlope));

        if (d->antialiased) {
            const Pk16Dot16 iTopFP = IntToPk16Dot16(int(topBound));
            const Pk16Dot16 iLeftFP = IntToPk16Dot16(int(left.y()));
            const Pk16Dot16 iRightFP = IntToPk16Dot16(int(right.y()));
            const Pk16Dot16 iBottomFP = IntToPk16Dot16(int(bottomBound));

            Pk16Dot16 leftIntersectAf = qSafeFloatToPk16Dot16(top.x() + (int(topBound) - top.y()) * topLeftSlope);
            Pk16Dot16 rightIntersectAf = qSafeFloatToPk16Dot16(top.x() + (int(topBound) - top.y()) * topRightSlope);
            Pk16Dot16 leftIntersectBf = 0;
            Pk16Dot16 rightIntersectBf = 0;

            if (iLeftFP < iTopFP)
                leftIntersectBf = qSafeFloatToPk16Dot16(left.x() + (int(topBound) - left.y()) * bottomLeftSlope);

            if (iRightFP < iTopFP)
                rightIntersectBf = qSafeFloatToPk16Dot16(right.x() + (int(topBound) - right.y()) * bottomRightSlope);

            Pk16Dot16 rowTop, rowBottomLeft, rowBottomRight, rowTopLeft, rowTopRight, rowBottom;
            Pk16Dot16 topLeftIntersectAf, topLeftIntersectBf, topRightIntersectAf, topRightIntersectBf;
            Pk16Dot16 bottomLeftIntersectAf, bottomLeftIntersectBf, bottomRightIntersectAf, bottomRightIntersectBf;

            int leftMin, leftMax, rightMin, rightMax;

            const Pk16Dot16 yTopFP = qSafeFloatToPk16Dot16(top.y());
            const Pk16Dot16 yLeftFP = qSafeFloatToPk16Dot16(left.y());
            const Pk16Dot16 yRightFP = qSafeFloatToPk16Dot16(right.y());
            const Pk16Dot16 yBottomFP = qSafeFloatToPk16Dot16(bottom.y());

            rowTop = pkMax(iTopFP, yTopFP);
            topLeftIntersectAf = leftIntersectAf +
                                 Pk16Dot16Multiply(topLeftSlopeFP, rowTop - iTopFP);
            topRightIntersectAf = rightIntersectAf +
                                  Pk16Dot16Multiply(topRightSlopeFP, rowTop - iTopFP);

            Pk16Dot16 yFP = iTopFP;
            while (yFP <= iBottomFP) {
                rowBottomLeft = pkMin(yFP + Pk16Dot16Factor, yLeftFP);
                rowBottomRight = pkMin(yFP + Pk16Dot16Factor, yRightFP);
                rowTopLeft = pkMax(yFP, yLeftFP);
                rowTopRight = pkMax(yFP, yRightFP);
                rowBottom = pkMin(yFP + Pk16Dot16Factor, yBottomFP);

                if (yFP == iLeftFP) {
                    const int y = Pk16Dot16ToInt(yFP);
                    leftIntersectBf = qSafeFloatToPk16Dot16(left.x() + (y - left.y()) * bottomLeftSlope);
                    topLeftIntersectBf = leftIntersectBf + Pk16Dot16Multiply(bottomLeftSlopeFP, rowTopLeft - yFP);
                    bottomLeftIntersectAf = leftIntersectAf + Pk16Dot16Multiply(topLeftSlopeFP, rowBottomLeft - yFP);
                } else {
                    topLeftIntersectBf = leftIntersectBf;
                    bottomLeftIntersectAf = leftIntersectAf + topLeftSlopeFP;
                }

                if (yFP == iRightFP) {
                    const int y = Pk16Dot16ToInt(yFP);
                    rightIntersectBf = qSafeFloatToPk16Dot16(right.x() + (y - right.y()) * bottomRightSlope);
                    topRightIntersectBf = rightIntersectBf + Pk16Dot16Multiply(bottomRightSlopeFP, rowTopRight - yFP);
                    bottomRightIntersectAf = rightIntersectAf + Pk16Dot16Multiply(topRightSlopeFP, rowBottomRight - yFP);
                } else {
                    topRightIntersectBf = rightIntersectBf;
                    bottomRightIntersectAf = rightIntersectAf + topRightSlopeFP;
                }

                if (yFP == iBottomFP) {
                    bottomLeftIntersectBf = leftIntersectBf + Pk16Dot16Multiply(bottomLeftSlopeFP, rowBottom - yFP);
                    bottomRightIntersectBf = rightIntersectBf + Pk16Dot16Multiply(bottomRightSlopeFP, rowBottom - yFP);
                } else {
                    bottomLeftIntersectBf = leftIntersectBf + bottomLeftSlopeFP;
                    bottomRightIntersectBf = rightIntersectBf + bottomRightSlopeFP;
                }

                if (yFP < iLeftFP) {
                    leftMin = Pk16Dot16ToInt(bottomLeftIntersectAf);
                    leftMax = Pk16Dot16ToInt(topLeftIntersectAf);
                } else if (yFP == iLeftFP) {
                    leftMin = Pk16Dot16ToInt(pkMax(bottomLeftIntersectAf, topLeftIntersectBf));
                    leftMax = Pk16Dot16ToInt(pkMax(topLeftIntersectAf, bottomLeftIntersectBf));
                } else {
                    leftMin = Pk16Dot16ToInt(topLeftIntersectBf);
                    leftMax = Pk16Dot16ToInt(bottomLeftIntersectBf);
                }

                leftMin = pkBound(d->clipRect.left(), leftMin, d->clipRect.right());
                leftMax = pkBound(d->clipRect.left(), leftMax, d->clipRect.right());

                if (yFP < iRightFP) {
                    rightMin = Pk16Dot16ToInt(topRightIntersectAf);
                    rightMax = Pk16Dot16ToInt(bottomRightIntersectAf);
                } else if (yFP == iRightFP) {
                    rightMin = Pk16Dot16ToInt(pkMin(topRightIntersectAf, bottomRightIntersectBf));
                    rightMax = Pk16Dot16ToInt(pkMin(bottomRightIntersectAf, topRightIntersectBf));
                } else {
                    rightMin = Pk16Dot16ToInt(bottomRightIntersectBf);
                    rightMax = Pk16Dot16ToInt(topRightIntersectBf);
                }

                rightMin = pkBound(d->clipRect.left(), rightMin, d->clipRect.right());
                rightMax = pkBound(d->clipRect.left(), rightMax, d->clipRect.right());

                if (leftMax > rightMax)
                    leftMax = rightMax;
                if (rightMin < leftMin)
                    rightMin = leftMin;

                Pk16Dot16 rowHeight = rowBottom - rowTop;

                int x = leftMin;
                while (x <= leftMax) {
                    Pk16Dot16 excluded = 0;

                    if (yFP <= iLeftFP)
                        excluded += intersectPixelFP(x, rowTop, rowBottomLeft,
                                                     bottomLeftIntersectAf, topLeftIntersectAf,
                                                     topLeftSlopeFP, invTopLeftSlopeFP);
                    if (yFP >= iLeftFP)
                        excluded += intersectPixelFP(x, rowTopLeft, rowBottom,
                                                     topLeftIntersectBf, bottomLeftIntersectBf,
                                                     bottomLeftSlopeFP, invBottomLeftSlopeFP);

                    if (x >= rightMin) {
                        if (yFP <= iRightFP)
                            excluded += (rowBottomRight - rowTop) - intersectPixelFP(x, rowTop, rowBottomRight,
                                                                                     topRightIntersectAf, bottomRightIntersectAf,
                                                                                     topRightSlopeFP, invTopRightSlopeFP);
                        if (yFP >= iRightFP)
                            excluded += (rowBottom - rowTopRight) - intersectPixelFP(x, rowTopRight, rowBottom,
                                                                                     bottomRightIntersectBf, topRightIntersectBf,
                                                                                     bottomRightSlopeFP, invBottomRightSlopeFP);
                    }

                    Pk16Dot16 coverage = rowHeight - excluded;
                    buffer.addSpan(x, 1, Pk16Dot16ToInt(yFP),
                                   Pk16Dot16ToInt(255 * coverage));
                    ++x;
                }
                if (x < rightMin) {
                    buffer.addSpan(x, rightMin - x, Pk16Dot16ToInt(yFP),
                                   Pk16Dot16ToInt(255 * rowHeight));
                    x = rightMin;
                }
                while (x <= rightMax) {
                    Pk16Dot16 excluded = 0;
                    if (yFP <= iRightFP)
                        excluded += (rowBottomRight - rowTop) - intersectPixelFP(x, rowTop, rowBottomRight,
                                                                                 topRightIntersectAf, bottomRightIntersectAf,
                                                                                 topRightSlopeFP, invTopRightSlopeFP);
                    if (yFP >= iRightFP)
                        excluded += (rowBottom - rowTopRight) - intersectPixelFP(x, rowTopRight, rowBottom,
                                                                                 bottomRightIntersectBf, topRightIntersectBf,
                                                                                 bottomRightSlopeFP, invBottomRightSlopeFP);

                    Pk16Dot16 coverage = rowHeight - excluded;
                    buffer.addSpan(x, 1, Pk16Dot16ToInt(yFP),
                                   Pk16Dot16ToInt(255 * coverage));
                    ++x;
                }

                leftIntersectAf += topLeftSlopeFP;
                leftIntersectBf += bottomLeftSlopeFP;
                rightIntersectAf += topRightSlopeFP;
                rightIntersectBf += bottomRightSlopeFP;
                topLeftIntersectAf = leftIntersectAf;
                topRightIntersectAf = rightIntersectAf;

                yFP += Pk16Dot16Factor;
                rowTop = yFP;
            }
        } else { // aliased
            int iTop = int(top.y() + 0.5f);
            int iLeft = left.y() < 0.5f ? -1 : int(left.y() - 0.5f);
            int iRight = right.y() < 0.5f ? -1 : int(right.y() - 0.5f);
            int iBottom = bottom.y() < 0.5f? -1 : int(bottom.y() - 0.5f);
            int iMiddle = pkMin(iLeft, iRight);

            Pk16Dot16 leftIntersectAf = qSafeFloatToPk16Dot16(top.x() + 0.5f + (iTop + 0.5f - top.y()) * topLeftSlope);
            Pk16Dot16 leftIntersectBf = qSafeFloatToPk16Dot16(left.x() + 0.5f + (iLeft + 1.5f - left.y()) * bottomLeftSlope);
            Pk16Dot16 rightIntersectAf = qSafeFloatToPk16Dot16(top.x() - 0.5f + (iTop + 0.5f - top.y()) * topRightSlope);
            Pk16Dot16 rightIntersectBf = qSafeFloatToPk16Dot16(right.x() - 0.5f + (iRight + 1.5f - right.y()) * bottomRightSlope);

            int ny;
            int y = iTop;
#define DO_SEGMENT(next, li, ri, ls, rs) \
            ny = pkMin(next + 1, d->clipRect.top()); \
            if (y < ny) { \
                li += ls * (ny - y); \
                ri += rs * (ny - y); \
                y = ny; \
            } \
            if (next > d->clipRect.bottom()) \
                next = d->clipRect.bottom(); \
            for (; y <= next; ++y) { \
                const int x1 = pkMax(Pk16Dot16ToInt(li), d->clipRect.left()); \
                const int x2 = pkMin(Pk16Dot16ToInt(ri), d->clipRect.right()); \
                if (x2 >= x1) \
                    buffer.addSpan(x1, x2 - x1 + 1, y, 255); \
                li += ls; \
                ri += rs; \
             }

            DO_SEGMENT(iMiddle, leftIntersectAf, rightIntersectAf, topLeftSlopeFP, topRightSlopeFP)
            DO_SEGMENT(iRight, leftIntersectBf, rightIntersectAf, bottomLeftSlopeFP, topRightSlopeFP)
            DO_SEGMENT(iLeft, leftIntersectAf, rightIntersectBf, topLeftSlopeFP, bottomRightSlopeFP);
            DO_SEGMENT(iBottom, leftIntersectBf, rightIntersectBf, bottomLeftSlopeFP, bottomRightSlopeFP);
#undef DO_SEGMENT
        }
    }
}
