// R-64 探针：Pk 侧文档产出（Qt-free）。逐用例产出两种 svg 根属性形态：
//   bounds  —— PkSvgPainterBackend(canvasRect())（= oracle 现用形态）
//   default —— PkSvgPainterBackend()（= SvgWriter.cpp:251 真实调用点形态，无 width/height/viewBox）
//
// 落点：pk/render/oracle/probes/pkdocs.cpp（Qt-free，Pk 侧）。由同目录 run_probes.sh 编译运行；它回答什么、期望读数见同目录 README.md。
#include "svg_primitive_cases.h"
#include <iostream>

static std::string renderCase(const pkSvgCases::Case &c, bool useBounds)
{
    PkSvgPainterBackend backend = useBounds ? PkSvgPainterBackend(pkSvgCases::canvasRect())
                                            : PkSvgPainterBackend();
    PkPainter painter(backend);
    painter.setRenderHint(PkPainter::RenderHint::Antialiasing, true);
    painter.setPen(c.hasPen ? PkPen(PkColor(Pk::black), c.penWidth) : PkPen(Pk::NoPen));
    painter.setBrush(c.hasBrush ? PkBrush(PkColor(Pk::red)) : PkBrush(Pk::NoBrush));
    switch (c.kind) {
    case pkSvgCases::Kind::Ellipse: painter.drawEllipse(c.rect); break;
    case pkSvgCases::Kind::Polygon: {
        PkPolygonF polygon;
        for (const auto &p : c.points) polygon.append(p);
        painter.drawPolygon(polygon);
        break; }
    case pkSvgCases::Kind::Arc: painter.drawArc(c.rect, c.startAngle16, c.spanAngle16); break;
    }
    return backend.document();
}

int main()
{
    for (const auto &c : pkSvgCases::table()) {
        std::cout << c.name << "\tbounds\t" << renderCase(c, true) << '\n';
        std::cout << c.name << "\tdefault\t" << renderCase(c, false) << '\n';
    }
    return 0;
}
