// Compile separately against real Qt and native Pk. No production bridge.
#ifdef PK_BRUSH_QT_ORACLE
#include <QBrush>
#include <QImage>
#include <QPainter>
#include <QTransform>
using Brush = QBrush;
using Transform = QTransform;
using Color = QColor;
using Point = QPointF;
#else
#include "PkPainter.h"
using Brush = PkBrush;
using Transform = PkTransform;
using Color = PkColor;
using Point = PkPointF;
struct Sink final : PkPainterBackend {
    void submit(const PkPaintCommand &) override {}
};
#endif

#include <iomanip>
#include <iostream>

static void print(const Brush &brush)
{
    const auto t = brush.transform();
    const auto p = t.map(Point(3, 7));
    std::cout << t.m11() << ' ' << t.m12() << ' ' << t.m13() << ' '
              << t.m21() << ' ' << t.m22() << ' ' << t.m23() << ' '
              << t.m31() << ' ' << t.m32() << ' ' << t.m33() << ' '
              << p.x() << ' ' << p.y() << '\n';
}

int main()
{
    std::cout << std::setprecision(17);
    Brush brush(Color(40, 50, 60));
    print(brush);
    Transform gradientToUser(20, 0, 0, 30, 100, 200);
    Transform local(2, 1, 3, 4, 5, 6);
    // Measured SVG OBB compensation order, plus the noncommuting reverse.
    brush.setTransform(local * gradientToUser);
    print(brush);
    brush.setTransform(gradientToUser * local);
    print(brush);
    const Brush copy = brush;
    brush.setTransform(Transform(1, 2, 0.01, 3, 4, 0.02, 5, 6, 1));
    print(brush);
    print(copy);
    std::cout << (brush == copy) << ' ' << (copy == Brush(copy)) << '\n';
#ifdef PK_BRUSH_QT_ORACLE
    QImage device(16, 16, QImage::Format_ARGB32);
    QPainter painter(&device);
#else
    Sink backend;
    PkPainter painter(backend);
#endif
    painter.setBrush(copy);
    painter.save();
    painter.setBrush(brush);
    painter.restore();
    print(painter.brush());
    Brush returned = painter.brush();
    returned.setTransform(Transform());
    print(painter.brush());
    print(returned);
    return 0;
}
