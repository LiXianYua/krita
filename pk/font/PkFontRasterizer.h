#pragma once

#include "PkFont.h"
#include "PkImage.h"
#include "PkString.h"
#include <PkPainterPath.h>

class PkFontRasterizer
{
public:
    struct Metrics { double ascent = 0; double descent = 0; double rightOverhang = 0; };
    // Render UTF-16 text as an opaque black-on-white ARGB32 brush image.
    static PkImage render(const PkString &text, const PkFont &font);
    // Shaped SVG glyph outlines in user coordinates, relative to the baseline.
    static PkPainterPath outline(const PkString &text, const PkFont &font, double *advance = nullptr,
                                Metrics *metrics = nullptr);
};
