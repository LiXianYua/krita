/* SPDX-FileCopyrightText: 2016 The Qt Company Ltd.
 * SPDX-License-Identifier: LGPL-3.0-only OR GPL-2.0-or-later
 * Native ARGB32 span path from Qt 5.15.7 qcosmeticstroker_p.h.
 */
#pragma once
#include "PkRasterDefs.h"
#include <PkPen.h>
#include <PkPainterPath.h>
#include <PkTransform.h>
#include <cstdlib>
#include <new>

class PkCosmeticStroker;
using StrokeLine = bool (*)(PkCosmeticStroker *, qreal, qreal, qreal, qreal, int);
class PkCosmeticStroker
{
public:
    struct Point { int x; int y; };
    struct PointF { qreal x; qreal y; };
    enum Caps { NoCaps = 0, CapBegin = 1, CapEnd = 2 };
    enum Direction { NoDirection = 0, TopToBottom = 1, BottomToTop = 2,
        LeftToRight = 4, RightToLeft = 8, VerticalMask = 3, HorizontalMask = 12 };
    PkCosmeticStroker(const PkPen &pen, const PkTransform &transform, bool aa,
                     qreal scale, const PkRect &rect, PK_FT_SpanFunc callback, void *data)
        : lastPen(pen), matrix(transform), antialiased(aa), txscale(scale),
          deviceRect(rect), clip(rect), blend(callback), context(data) { setup(); }
    ~PkCosmeticStroker() { free(pattern); free(reversePattern); }
    PkCosmeticStroker(const PkCosmeticStroker &) = delete;
    PkCosmeticStroker &operator=(const PkCosmeticStroker &) = delete;
    void drawLine(const PkPointF &, const PkPointF &);
    void drawPath(const PkPainterPath &);
    void drawPoints(const PkPoint *, int);
    void drawPoints(const PkPointF *, int);
    bool clipLine(qreal &, qreal &, qreal &, qreal &);

    PkPen lastPen;
    PkTransform matrix;
    bool antialiased;
    qreal txscale;
    PkRect deviceRect, clip;
    qreal xmin, xmax, ymin, ymax;
    StrokeLine stroke;
    bool drawCaps;
    int *pattern = nullptr;
    int *reversePattern = nullptr;
    int patternSize = 0, patternLength = 0, patternOffset = 0;
    bool legacyRounding = false;
    enum { NSPANS = 255 };
    PK_FT_Span spans[NSPANS];
    int current_span = 0;
    PK_FT_SpanFunc blend;
    void *context;
    int opacity;
    Direction lastDir = NoDirection;
    Point lastPixel;
    bool lastAxisAligned = false;
private:
    void setup();
    void renderCubic(const PkPointF &, const PkPointF &, const PkPointF &, const PkPointF &, int);
    void renderCubicSubdivision(PointF *, int, int);
    void calculateLastPoint(qreal, qreal, qreal, qreal);
};
