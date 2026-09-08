#include "PkSvgRasterizer.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <cstring>
#include <vector>

int main()
{
    const char *extreme = "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1e30'/>";
    if (!PkSvgRasterizer::render(extreme, std::strlen(extreme), 1000).isNull()) {
        std::cerr << "FAIL: SVG out-of-range height must be rejected before integer conversion\n";
        return 1;
    }
    const char *square = "<svg xmlns='http://www.w3.org/2000/svg' width='1' height='1'/>";
    try {
        if (!PkSvgRasterizer::render(square, std::strlen(square), std::numeric_limits<int>::max()).isNull() ||
            !PkSvgRasterizer::render(square, std::numeric_limits<std::size_t>::max(), 1000).isNull()) {
            std::cerr << "FAIL: SVG byte/stride limits were not enforced\n";
            return 1;
        }
    } catch (...) {
        std::cerr << "FAIL: SVG allocation failure escaped the loader boundary\n";
        return 1;
    }
    static const char svg[] =
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 4 2\">"
        "<g transform=\"translate(1 0)\" opacity=\"0.5\">"
        "<rect width=\"2\" height=\"2\" fill=\"#ff0000\"/>"
        "</g></svg>";
    const PkImage image = PkSvgRasterizer::render(svg, sizeof(svg) - 1, 1000);
    if (image.isNull() || image.width() != 1000 || image.height() != 500) {
        std::cerr << "FAIL: SVG viewBox dimensions were not preserved\n";
        return 1;
    }
    if (image.pixel(100, 250) != 0xFFFFFFFFu ||
        image.pixel(300, 250) != 0xFFFF8080u ||
        image.pixel(700, 250) != 0xFFFF8080u) {
        std::cerr << "FAIL: SVG transform/opacity/color pixels differ: "
                  << std::hex << image.pixel(100, 250) << ' '
                  << image.pixel(300, 250) << ' '
                  << image.pixel(700, 250) << '\n';
        return 1;
    }

    std::cout << "SVG rasterizer matches Qt 5.15 brush oracle\n";
    return 0;
}
