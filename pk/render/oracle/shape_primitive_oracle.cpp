// 两侧各编一份：-DPK_SHAPE_QT_ORACLE 走真 QPainter，否则走 PkPainter +
// PkImageRasterBackend。两边打印同一份「用例名 + 像素摘要」表，diff 即判据。
//
// 形制照抄 pk/render/oracle/brush_gradient.cpp（R线-spec「对拍怎么做·形态契约」：
// 两侧**真的分别** include 各自的头，不是同一个实现换个壳）。
#include "shape_primitive_cases.h"

#include <iostream>

static CaseImage renderCase(const pkShapeCases::Case &c)
{
    CaseImage image = makeCaseImage();
    fillCaseImage(image);
#ifdef PK_SHAPE_QT_ORACLE
    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(c.hasPen ? QPen(Qt::black, c.penWidth) : QPen(Qt::NoPen));
    painter.setBrush(c.hasBrush ? QBrush(Qt::red) : QBrush(Qt::NoBrush));
    if (c.polygon) {
        QPolygonF polygon;
        for (const auto &p : c.points) polygon << p;
        painter.drawPolygon(polygon, c.fillRule ? Qt::WindingFill : Qt::OddEvenFill);
    } else {
        painter.drawEllipse(c.ellipseRect);
    }
    painter.end();
#else
    PkImageRasterBackend backend(image);
    PkPainter painter(backend);
    painter.setRenderHint(PkPainter::RenderHint::Antialiasing, true);
    painter.setPen(c.hasPen ? PkPen(PkColor(Pk::black), c.penWidth) : PkPen(Pk::NoPen));
    painter.setBrush(c.hasBrush ? PkBrush(PkColor(Pk::red)) : PkBrush(Pk::NoBrush));
    if (c.polygon) {
        PkPolygonF polygon;
        for (const auto &p : c.points) polygon.append(p);
        painter.drawPolygon(polygon);
    } else {
        painter.drawEllipse(c.ellipseRect);
    }
#endif
    return image;
}

int main()
{
    std::cout << "backend " << kBackendName << '\n';
    for (const auto &c : pkShapeCases::table()) {
        const CaseImage image = renderCase(c);
        std::cout << c.name << ' ' << pkShapeCases::digest(image) << '\n';
    }
    return 0;
}
