#pragma once

#include "PkFont.h"
#include "PkImage.h"
#include "PkString.h"
#include <PkPainterPath.h>
#include <PkTransform.h>

#include <vector>

class PkFontRasterizer
{
public:
    struct Metrics { double ascent = 0; double descent = 0; double rightOverhang = 0; };

    // Coverage of shaped glyphs rasterized with `transform`, in device pixels.
    // `mask[i] = 255 - <render()'s grayscale at the same pixel>`, so 0 is no ink
    // and 255 is full ink -- in the non-gamma path that is the glyph coverage
    // itself.
    //
    // `(offsetX, offsetY)` locates the mask on the device pixel grid: cell
    // `(col, row)` covers device pixel
    //   (floor(devicePoint.x()) + offsetX + col,
    //    round(devicePoint.y()) + offsetY + row)
    // -- Qt's own glyph blit rounding (qpaintengine_raster.cpp:2892-2893:
    // `qFloor(pos.x) + bitmap_left, qRound(pos.y) - bitmap_top`; the x fraction
    // goes into the font engine's sub-pixel delta, the y one has nowhere to go).
    // The baseline origin passed in may be fractional; the mask is snapped to
    // that grid.  At `devicePoint == (0, 0)` -- what `render()` uses -- the
    // offsets are the ink box relative to the baseline origin itself, i.e.
    // `(minimumX, -ascent)`.
    struct TextCoverage {
        std::vector<unsigned char> mask;   // width*height; 0 = no ink, 255 = full
        int width = 0, height = 0;
        int offsetX = 0, offsetY = 0;
        bool isEmpty() const { return width <= 0 || height <= 0; }
    };

    // Render UTF-16 text as an opaque black-on-white ARGB32 brush image.
    static PkImage render(const PkString &text, const PkFont &font);
    // Shaped SVG glyph outlines in user coordinates, relative to the baseline.
    static PkPainterPath outline(const PkString &text, const PkFont &font, double *advance = nullptr,
                                Metrics *metrics = nullptr);
    // devicePoint = the text baseline origin in device coordinates; transform =
    // the brush transform (only its 2x2 linear part is passed per glyph; the
    // translation is already baked into devicePoint).  An identity 2x2 -- which
    // includes a pure translation -- runs the same code path as `render()`; its
    // mask and offsets are the ink box relative to the text origin, so they do
    // not depend on `devicePoint` at all.
    static TextCoverage coverage(const PkString &text, const PkFont &font,
                                 const PkPointF &devicePoint, const PkTransform &transform);
};
