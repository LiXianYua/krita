#include "PkSvgRasterizer.h"
#include "PkSvgDocument_p.h"

PkImage PkSvgRasterizer::render(const char *svg, std::size_t size, int outputWidth)
try
{
    return pkRenderSvgDocument(svg, size, outputWidth);
}
catch (const std::bad_alloc &) { return {}; }
catch (const std::length_error &) { return {}; }
catch (const std::invalid_argument &) { return {}; }
