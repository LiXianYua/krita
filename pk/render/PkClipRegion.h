/*
 * Integer polygon scan conversion uses the Bresenham edge recurrence from
 * Qt 5.15 qregion.cpp's poly.h / PolyReg.c extracts.
 *
 * Copyright (c) 1987 X Consortium
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * X CONSORTIUM BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * Except as contained in this notice, the name of the X Consortium shall not be
 * used in advertising or otherwise to promote the sale, use or other dealings
 * in this Software without prior written authorization from the X Consortium.
 *
 * Copyright 1987 by Digital Equipment Corporation, Maynard, Massachusetts.
 * All Rights Reserved
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appear in all copies and that
 * both that copyright notice and this permission notice appear in
 * supporting documentation, and that the name of Digital not be
 * used in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.
 *
 * DIGITAL DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING
 * ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL
 * DIGITAL BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR
 * ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,
 * ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#pragma once

#include <PkRegion.h>
#include <PkPolygon.h>
#include <algorithm>
#include <cstdint>
#include <vector>

namespace PkRender {
inline PkRegion clipRegionFromPolygon(const PkPolygon &polygon, Pk::FillRule rule)
{
    struct Edge {
        int top, bottom, winding;
        int64_t x, d, m, m1, incr1, incr2;
        void advance()
        {
            if (m1 > 0 ? d > 0 : d >= 0) { x += m1; d += incr1; }
            else { x += m; d += incr2; }
        }
    };
    std::vector<Edge> edges;
    if (polygon.size() < 3) return PkRegion();
    auto previous = polygon.last();
    for (const auto &current : polygon) {
        const bool clockwise = previous.y() <= current.y();
        const auto top = clockwise ? previous : current;
        const auto bottom = clockwise ? current : previous;
        previous = current;
        if (top.y() == bottom.y()) continue;
        const int64_t dy = int64_t(bottom.y()) - top.y();
        const int64_t dx = int64_t(bottom.x()) - top.x();
        Edge edge {top.y(), bottom.y(), clockwise ? 1 : -1, top.x(), 0, dx / dy, 0, 0, 0};
        if (dx < 0) {
            edge.m1 = edge.m - 1;
            edge.incr1 = -2 * dx + 2 * dy * edge.m1;
            edge.incr2 = -2 * dx + 2 * dy * edge.m;
            edge.d = 2 * edge.m * dy - 2 * dx - 2 * dy;
        } else {
            edge.m1 = edge.m + 1;
            edge.incr1 = 2 * dx - 2 * dy * edge.m1;
            edge.incr2 = 2 * dx - 2 * dy * edge.m;
            edge.d = -2 * edge.m * dy + 2 * dx;
        }
        edges.push_back(edge);
    }
    if (edges.empty()) return PkRegion();
    int top = edges.front().top, bottom = edges.front().bottom;
    for (const auto &edge : edges) {
        top = std::min(top, edge.top); bottom = std::max(bottom, edge.bottom);
    }
    // QRegion also rejects polygon scan-conversion taller than 100000 rows.
    if (int64_t(bottom) - top > 100000) return PkRegion();
    PkRegion result;
    std::vector<Edge *> active;
    for (int y = top; y < bottom; ++y) {
        active.clear();
        for (auto &edge : edges) if (edge.top <= y && y < edge.bottom) active.push_back(&edge);
        std::stable_sort(active.begin(), active.end(), [](const Edge *a, const Edge *b) { return a->x < b->x; });
        int winding = 0;
        int64_t start = 0;
        for (const auto *edge : active) {
            const bool before = rule == Pk::WindingFill ? winding != 0 : (winding & 1) != 0;
            winding += edge->winding;
            const bool after = rule == Pk::WindingFill ? winding != 0 : (winding & 1) != 0;
            if (!before && after) start = edge->x;
            else if (before && !after && edge->x > start) result |= PkRect(int(start), y, int(edge->x - start), 1);
        }
        for (auto *edge : active) edge->advance();
    }
    return result;
}
}
