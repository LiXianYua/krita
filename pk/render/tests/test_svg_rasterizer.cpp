#include "PkSvgRasterizer.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <cstring>
#include <vector>
#include <string>

int main()
{
    // Clamp finite color components before integer conversion, including when
    // percentage scaling overflows. UBSan must not diagnose the load path.
    const std::string hugeColor = "<svg viewBox='0 0 1 1'><rect width='1' height='1' fill='rgb(1e308%,-1e308%,0%)'/></svg>";
    const auto saturated = PkSvgRasterizer::render(hugeColor.data(), hugeColor.size(), 8);
    if (saturated.isNull() || saturated.pixel(4, 4) != 0xffff0000u) {
        std::cerr << "FAIL: out-of-range RGB components were not saturated\n"; return 1;
    }
    // Malformed numeric paths must not escape as backend exceptions; raster
    // span limits must be checked before allocating the output surface.
    for (const std::string svg : {
             "<svg viewBox='0 0 4 2'><path d='M0 0 L1e999 1Z'/></svg>",
             "<svg viewBox='0 0 4 2'><text font-size='1e999'>A</text></svg>"}) {
        try { (void)PkSvgRasterizer::render(svg.data(), svg.size(), 32); }
        catch (...) { std::cerr << "FAIL: malformed SVG escaped the load boundary\n"; return 1; }
    }
    const std::string wide = "<svg viewBox='0 0 32768 1'><rect width='32768' height='1'/></svg>";
    const std::string tinyImage = "<svg viewBox='0 0 1 1'><image width='1e-10' height='1e10' href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII='/></svg>";
    try { (void)PkSvgRasterizer::render(tinyImage.data(), tinyImage.size(), 8); }
    catch (...) { std::cerr << "FAIL: extreme image transform escaped\n"; return 1; }
    try {
        if (!PkSvgRasterizer::render(wide.data(), wide.size(), 32768).isNull()) {
            std::cerr << "FAIL: output exceeds the raster span range\n"; return 1;
        }
    } catch (...) { std::cerr << "FAIL: span-range exception escaped\n"; return 1; }
    std::string deep = "<svg viewBox='0 0 1 1'>";
    for (int i = 0; i < 260; ++i) deep += "<g>";
    for (int i = 0; i < 260; ++i) deep += "</g>";
    deep += "</svg>";
    if (!PkSvgRasterizer::render(deep.data(), deep.size(), 8).isNull()) {
        std::cerr << "FAIL: recursive document depth not bounded\n"; return 1;
    }
    const std::string cycle = "<svg viewBox='0 0 1 1'><defs><g id='cycle'><use href='#cycle'/></g></defs><use href='#cycle'/><path d='M0 0H1V1H0Z 0'/></svg>";
    const auto cycleImage = PkSvgRasterizer::render(cycle.data(), cycle.size(), 8);
    if (cycleImage.isNull() || cycleImage.pixel(4, 4) != 0xff000000u) {
        std::cerr << "FAIL: reference cycle or malformed close-path lost sibling painting\n"; return 1;
    }
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
