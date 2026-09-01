#include "path_rasterizer_cases.h"

#include "../private/kis_path_rasterizer_p.h"

#include <PkPainterPath.h>

#include <cstdio>
#include <cstring>

namespace {

PkPainterPath makePath(const RasterCase &c)
{
    PkPainterPath path;
    path.setFillRule(c.fillRule == 1 ? Qt::WindingFill : Qt::OddEvenFill);
    for (std::size_t i = 0; i < c.commandCount; ++i) {
        const auto &cmd = c.commands[i];
        switch (cmd.verb) {
        case PathVerb::MoveTo: path.moveTo(cmd.x1, cmd.y1); break;
        case PathVerb::LineTo: path.lineTo(cmd.x1, cmd.y1); break;
        case PathVerb::CubicTo:
            path.cubicTo(cmd.x1, cmd.y1, cmd.x2, cmd.y2, cmd.x3, cmd.y3);
            break;
        case PathVerb::Close: path.closeSubpath(); break;
        }
    }
    return path;
}

bool isFillCase(const RasterCase &c)
{
    return c.mode == RasterMode::Fill;
}

bool valid(const RasterCase &c)
{
    return c.clipWidth > 0 && c.clipHeight > 0
        && c.clipWidth <= 4096 && c.clipHeight <= 4096
        && c.commands != nullptr && c.commandCount > 0;
}

int emitCase(const RasterCase &c)
{
    if (!valid(c)) {
        return 3;
    }
    const PkRect clip(c.clipX, c.clipY, c.clipWidth, c.clipHeight);
    const auto mask = KisPathRasterizer::rasterizeFill(makePath(c), clip, c.antialiased);
    if (mask.bounds != clip || mask.stride != c.clipWidth
        || mask.alpha.size() != std::size_t(c.clipWidth) * std::size_t(c.clipHeight)) {
        return 3;
    }

    std::printf("KPR1\n%s\n%d %d %d %d\n",
                c.name, c.clipX, c.clipY, c.clipWidth, c.clipHeight);
    for (uint8_t coverage : mask.alpha) {
        std::printf("%02x", coverage);
    }
    std::putchar('\n');
    return 0;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--list") == 0) {
        for (const auto &c : pathRasterizerCases()) {
            if (isFillCase(c)) {
                std::printf("%s\n", c.name);
            }
        }
        return 0;
    }
    if (argc == 3 && std::strcmp(argv[1], "--case") == 0) {
        for (const auto &c : pathRasterizerCases()) {
            if (isFillCase(c) && std::strcmp(argv[2], c.name) == 0) {
                return emitCase(c);
            }
        }
        return 2;
    }
    return 2;
}
