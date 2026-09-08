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
