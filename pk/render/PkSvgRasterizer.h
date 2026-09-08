#pragma once

#include <cstddef>

#include "PkImage.h"

class PkSvgRasterizer
{
public:
    // Rasterize an SVG document onto an opaque white ARGB32 image. The output
    // width is fixed and the document view box controls the aspect ratio.
    static PkImage render(const char *svg, std::size_t size, int outputWidth);
};
