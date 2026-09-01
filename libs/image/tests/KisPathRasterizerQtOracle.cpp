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
    painter.translate(-double(c.clipX), -double(c.clipY));
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

int main(int argc, char **argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--version") == 0) { std::printf("%s\n", qVersion()); return 0; }
    if (argc == 2 && std::strcmp(argv[1], "--list") == 0) { for (const auto &c : pathRasterizerCases()) std::printf("%s\n", c.name); return 0; }
    if (argc == 2 && std::strcmp(argv[1], "--list-fill") == 0) { for (const auto &c : pathRasterizerCases()) if (c.mode == RasterMode::Fill) std::printf("%s\n", c.name); return 0; }
    if (argc == 3 && std::strcmp(argv[1], "--case") == 0) {
        for (const auto &c : pathRasterizerCases()) if (std::strcmp(argv[2], c.name) == 0) return emitCase(c);
        return 2;
    }
    return 2;
}
