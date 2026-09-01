#include "path_rasterizer_cases.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QVector>
#include <QtGlobal>

#include <cstdio>
#include <cstring>

namespace {

constexpr PathCommand nested[] = {
    {PathVerb::MoveTo, 4.25, 4, 0, 0, 0, 0}, {PathVerb::LineTo, 28.25, 4, 0, 0, 0, 0},
    {PathVerb::LineTo, 28.25, 28, 0, 0, 0, 0}, {PathVerb::LineTo, 4.25, 28, 0, 0, 0, 0}, {PathVerb::Close, 0, 0, 0, 0, 0, 0},
    {PathVerb::MoveTo, 10.5, 10, 0, 0, 0, 0}, {PathVerb::LineTo, 22.5, 10, 0, 0, 0, 0},
    {PathVerb::LineTo, 22.5, 22, 0, 0, 0, 0}, {PathVerb::LineTo, 10.5, 22, 0, 0, 0, 0}, {PathVerb::Close, 0, 0, 0, 0, 0, 0}
};
constexpr PathCommand rect[] = {
    {PathVerb::MoveTo, 3.25, 3.5, 0, 0, 0, 0}, {PathVerb::LineTo, 18.75, 3.5, 0, 0, 0, 0},
    {PathVerb::LineTo, 18.75, 18.25, 0, 0, 0, 0}, {PathVerb::LineTo, 3.25, 18.25, 0, 0, 0, 0}, {PathVerb::Close, 0, 0, 0, 0, 0, 0}
};
constexpr PathCommand open_cubic[] = {
    {PathVerb::MoveTo, 2.5, 12.25, 0, 0, 0, 0}, {PathVerb::LineTo, 29.5, 12.75, 0, 0, 0, 0},
    {PathVerb::MoveTo, 7.5, 7.25, 0, 0, 0, 0}, {PathVerb::CubicTo, 8, 2, 25, 2, 24.5, 8},
    {PathVerb::CubicTo, 24, 14, 9, 16, 7.5, 7.25}, {PathVerb::Close, 0, 0, 0, 0, 0, 0}
};
constexpr PathCommand crossing[] = {
    {PathVerb::MoveTo, -5.5, 13.2, 0, 0, 0, 0}, {PathVerb::CubicTo, 1, -3, 12, 25, 21.5, 8.75},
    {PathVerb::CubicTo, 24, 4, 28, 17, 34.5, 12.5}, {PathVerb::LineTo, -5.5, 12.5, 0, 0, 0, 0}, {PathVerb::Close, 0, 0, 0, 0, 0, 0}
};
constexpr PathCommand chunk_boundary[] = {
    {PathVerb::MoveTo, 247.25, 247.5, 0, 0, 0, 0}, {PathVerb::LineTo, 256.75, 247.5, 0, 0, 0, 0},
    {PathVerb::LineTo, 256.5, 257.25, 0, 0, 0, 0}, {PathVerb::LineTo, 247.25, 256.5, 0, 0, 0, 0}, {PathVerb::Close, 0, 0, 0, 0, 0, 0}
};
constexpr PathCommand mirrored_h[] = {
    {PathVerb::MoveTo, 24.75, 5.5, 0, 0, 0, 0}, {PathVerb::CubicTo, 24, 24, 12, 1, 7.25, 19.5},
    {PathVerb::LineTo, 24.75, 19.5, 0, 0, 0, 0}, {PathVerb::Close, 0, 0, 0, 0, 0, 0}
};
constexpr PathCommand mirrored_v[] = {
    {PathVerb::MoveTo, 7.25, 26.5, 0, 0, 0, 0}, {PathVerb::CubicTo, 8, 8, 20, 31, 24.75, 12.5},
    {PathVerb::LineTo, 7.25, 12.5, 0, 0, 0, 0}, {PathVerb::Close, 0, 0, 0, 0, 0, 0}
};
constexpr double dash[] = {2.0, 1.0};

const std::vector<RasterCase> cases = {
    {"nested_oddeven", RasterMode::Fill, 0, false, 0, 0, 32, 32, 1, 1, 0, 0, 4, 0, nullptr, 0, nested, sizeof(nested)/sizeof(*nested)},
    {"nested_winding", RasterMode::Fill, 1, false, 0, 0, 32, 32, 1, 1, 0, 0, 4, 0, nullptr, 0, nested, sizeof(nested)/sizeof(*nested)},
    {"aa_off_rect", RasterMode::Fill, 0, false, 0, 0, 24, 24, 1, 1, 0, 0, 4, 0, nullptr, 0, rect, sizeof(rect)/sizeof(*rect)},
    {"aa_on_rect", RasterMode::Fill, 0, true, 0, 0, 24, 24, 1, 1, 0, 0, 4, 0, nullptr, 0, rect, sizeof(rect)/sizeof(*rect)},
    {"aa_off_curve", RasterMode::Stroke, 0, false, 0, 0, 32, 32, 1, 1, 0, 0, 4, 0, nullptr, 0, open_cubic, sizeof(open_cubic)/sizeof(*open_cubic)},
    {"aa_on_curve", RasterMode::Stroke, 0, true, 0, 0, 32, 32, 1, 1, 0, 0, 4, 0, nullptr, 0, open_cubic, sizeof(open_cubic)/sizeof(*open_cubic)},
    {"open_line_closed_cubic", RasterMode::Stroke, 0, true, 0, 0, 32, 32, 3.5, 1, 0, 0, 4, 0, nullptr, 0, open_cubic, sizeof(open_cubic)/sizeof(*open_cubic)},
    {"crossing_clip", RasterMode::Fill, 0, true, -3, 5, 19, 17, 1, 1, 0, 0, 4, 0, nullptr, 0, crossing, sizeof(crossing)/sizeof(*crossing)},
    {"mirror_horizontal", RasterMode::Stroke, 0, true, 0, 0, 32, 32, 1, 1, 16, 0, 4, 0, nullptr, 0, mirrored_h, sizeof(mirrored_h)/sizeof(*mirrored_h)},
    {"mirror_vertical", RasterMode::Stroke, 0, true, 0, 0, 32, 32, 8, 1, 32, 128, 4, 0, nullptr, 0, mirrored_v, sizeof(mirrored_v)/sizeof(*mirrored_v)},
    {"chunk_boundary", RasterMode::Fill, 0, false, 248, 248, 16, 16, 1, 1, 0, 0, 4, 0, nullptr, 0, chunk_boundary, sizeof(chunk_boundary)/sizeof(*chunk_boundary)},
    {"stroke_flat_1", RasterMode::Stroke, 0, true, 0, 0, 32, 32, 1, 1, 0, 0, 4, 0, nullptr, 0, open_cubic, sizeof(open_cubic)/sizeof(*open_cubic)},
    {"stroke_square_3_5", RasterMode::Stroke, 0, true, 0, 0, 32, 32, 3.5, 1, 16, 64, 4, 0, nullptr, 0, open_cubic, sizeof(open_cubic)/sizeof(*open_cubic)},
    {"stroke_round_dash_8", RasterMode::Stroke, 0, true, 0, 0, 32, 32, 8, 6, 32, 128, 4, 0.5, dash, 2, open_cubic, sizeof(open_cubic)/sizeof(*open_cubic)},
    {"stroke_miter", RasterMode::Stroke, 0, true, 0, 0, 32, 32, 3.5, 1, 0, 0, 4, 0, nullptr, 0, crossing, sizeof(crossing)/sizeof(*crossing)},
    {"stroke_bevel", RasterMode::Stroke, 0, true, 0, 0, 32, 32, 3.5, 1, 0, 64, 4, 0, nullptr, 0, crossing, sizeof(crossing)/sizeof(*crossing)},
};

QPainterPath makePath(const RasterCase &c)
{
    QPainterPath path;
    path.setFillRule(c.fillRule == 1 ? Qt::WindingFill : Qt::OddEvenFill);
    for (std::size_t i = 0; i < c.commandCount; ++i) {
        const auto &cmd = c.commands[i];
        switch (cmd.verb) {
        case PathVerb::MoveTo: path.moveTo(cmd.x1, cmd.y1); break;
        case PathVerb::LineTo: path.lineTo(cmd.x1, cmd.y1); break;
        case PathVerb::CubicTo: path.cubicTo(cmd.x1, cmd.y1, cmd.x2, cmd.y2, cmd.x3, cmd.y3); break;
        case PathVerb::Close: path.closeSubpath(); break;
        }
    }
    return path;
}

bool valid(const RasterCase &c)
{
    return c.clipWidth > 0 && c.clipHeight > 0 && c.clipWidth <= 4096 && c.clipHeight <= 4096
        && c.commands != nullptr && c.commandCount > 0;
}

int emitCase(const RasterCase &c)
{
    if (!valid(c)) return 3;
    QImage image(c.clipWidth, c.clipHeight, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::black);
    QPainter painter(&image);
    painter.translate(-c.clipX, -c.clipY);
    painter.setRenderHint(QPainter::Antialiasing, c.antialiased);
    const QPainterPath path = makePath(c);
    if (c.mode == RasterMode::Fill) {
        painter.fillPath(path, QBrush(Qt::white));
    } else {
        QPen pen(Qt::white);
        pen.setWidthF(c.penWidth);
        pen.setStyle(static_cast<Qt::PenStyle>(c.penStyle));
        pen.setCapStyle(static_cast<Qt::PenCapStyle>(c.capStyle));
        pen.setJoinStyle(static_cast<Qt::PenJoinStyle>(c.joinStyle));
        pen.setMiterLimit(c.miterLimit);
        if (c.dashCount) { QVector<qreal> pattern; for (std::size_t i = 0; i < c.dashCount; ++i) pattern.append(c.dashPattern[i]); pen.setDashPattern(pattern); pen.setDashOffset(c.dashOffset); }
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(path);
    }
    painter.end();
    std::printf("KPR1\n%s\n%d %d %d %d\n", c.name, c.clipX, c.clipY, c.clipWidth, c.clipHeight);
    for (int y = 0; y < image.height(); ++y) for (int x = 0; x < image.width(); ++x) std::printf("%02x", qRed(image.pixel(x, y)));
    std::putchar('\n');
    return 0;
}
}

const std::vector<RasterCase> &pathRasterizerCases() { return cases; }

int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0) { std::printf("%s\n", qVersion()); return 0; }
    if (argc == 2 && std::strcmp(argv[1], "--list") == 0) { for (const auto &c : cases) std::printf("%s\n", c.name); return 0; }
    if (argc == 3 && std::strcmp(argv[1], "--case") == 0) {
        for (const auto &c : cases) if (std::strcmp(argv[2], c.name) == 0) return emitCase(c);
        return 2;
    }
    return 2;
}
