#pragma once

#include "PkFont.h"
#include "PkImage.h"
#include "PkString.h"

class PkFontRasterizer
{
public:
    // Render UTF-16 text as an opaque black-on-white ARGB32 brush image.
    static PkImage render(const PkString &text, const PkFont &font);
};
