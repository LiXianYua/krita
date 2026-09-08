// Independent executables: each branch uses its own value implementation.
#ifdef PK_BRUSH_QT_ORACLE
#include <QBrush>
using Brush = QBrush;
using Color = QColor;
using Gradient = QGradient;
using Stops = QGradientStops;
using Point = QPointF;
using Transform = QTransform;
namespace Style = Qt;
static QLinearGradient linear() { return QLinearGradient(1, 2, 30, 40); }
static QRadialGradient radial() { return QRadialGradient(Point(4, 5), 10, Point(5, 6)); }
static QConicalGradient conical() { return QConicalGradient(Point(7, 8), 60); }
#else
#include "PkBrush.h"
using Brush = PkBrush;
using Color = PkColor;
using Gradient = PkGradient;
using Stops = PkGradientStops;
using Point = PkPointF;
using Transform = PkTransform;
namespace Style = Pk;
static Gradient linear() { return Gradient::linear(Point(1, 2), Point(30, 40)); }
static Gradient radial() { return Gradient::radial(Point(4, 5), 10, Point(5, 6)); }
static Gradient conical() { return Gradient::conical(Point(7, 8), 60); }
#endif
#include <iostream>
#include <iomanip>

static void printStops(const Gradient &g)
{
    const auto stops = g.stops();
    std::cout << stops.size();
    for (const auto &stop : stops) {
#ifdef PK_BRUSH_QT_ORACLE
        const auto pos = stop.first;
        const auto color = stop.second;
#else
        const auto pos = stop.offset;
        const auto color = stop.color;
#endif
        std::cout << ' ' << pos << ':' << color.red() << ',' << color.green()
                  << ',' << color.blue() << ',' << color.alpha();
    }
    std::cout << '\n';
}

static void printBrush(const Brush &b)
{
    std::cout << int(b.style()) << ' ' << b.color().red() << ' '
              << b.color().green() << ' ' << b.color().blue() << ' '
              << (b.gradient() != nullptr) << ' ' << b.transform().dx() << '\n';
    if (b.gradient()) printStops(*b.gradient());
}

int main()
{
    std::cout << std::setprecision(17);
    auto g = linear();
    printStops(g);
    auto explicitRamp = g;
    explicitRamp.setStops({{0, Color(0, 0, 0)}, {1, Color(255, 255, 255)}});
    std::cout << "default-equality " << (g == explicitRamp) << '\n';
    std::cout << "linear " << g.start().x() << ' ' << g.start().y() << ' '
              << g.finalStop().x() << ' ' << g.finalStop().y() << '\n';
    auto movedLinear = g;
    movedLinear.setStart(3, 4);
    movedLinear.setFinalStop(50, 60);
    std::cout << (movedLinear == g) << ' ' << movedLinear.start().x() << ' '
              << movedLinear.finalStop().y() << '\n';
    const Color red(255, 0, 0), blue(0, 0, 255), green(0, 255, 0);
    g.setStops({{0.8, red}, {0.2, blue}, {0.8, green}, {-1, red}, {2, blue}});
    printStops(g);
    g.setStops({{0.2, red}, {0.2, green}, {0.8, blue}});
    printStops(g);
    g.setStops({});
    printStops(g);
    g.setColorAt(0.5, red);
    printStops(g);
    g.setColorAt(0.5, blue);
    g.setColorAt(-0.1, green);
    printStops(g);
    const auto original = g;
    g.setColorAt(0.5, green);
    std::cout << (original == g) << '\n';
    printStops(original);
    g.setSpread(Gradient::ReflectSpread);
    g.setCoordinateMode(Gradient::ObjectBoundingMode);
    const Brush snapshot(g);
    printBrush(snapshot);
    std::cout << (snapshot.gradient()->spread() == Gradient::ReflectSpread) << ' '
              << (snapshot.gradient()->coordinateMode() == Gradient::ObjectBoundingMode) << '\n';
    Brush changed = snapshot;
    changed.setColor(red);
    printBrush(changed);
    std::cout << (snapshot == changed) << '\n';
    changed.setTransform(Transform(2, 1, 3, 4, 5, 6));
    changed.setStyle(Style::RadialGradientPattern);
    printBrush(changed);
    changed.setStyle(Style::SolidPattern);
    printBrush(changed);
    printBrush(snapshot);
    printBrush(Brush(Gradient()));
    auto r = radial();
    std::cout << "radial " << r.center().x() << ' ' << r.center().y() << ' '
              << r.focalPoint().x() << ' ' << r.focalPoint().y() << '\n';
    const auto r0 = r;
    r.setCenterRadius(11);
    std::cout << r.radius() << ' ' << r.centerRadius() << ' ' << (r == r0) << '\n';
    r.setRadius(10);
    std::cout << r.radius() << ' ' << r.centerRadius() << ' ' << (r == r0) << '\n';
    r.setFocalRadius(2);
    std::cout << (r == r0) << '\n';
    auto movedRadial = r;
    movedRadial.setCenter(7, 9);
    movedRadial.setFocalPoint(8, 10);
    std::cout << (movedRadial == r) << ' ' << movedRadial.center().x() << ' '
              << movedRadial.focalPoint().y() << '\n';
    printBrush(Brush(r));
    auto c = conical();
    std::cout << "conical " << c.center().x() << ' ' << c.center().y() << ' ' << c.angle() << '\n';
    printBrush(Brush(c));
    const auto c0 = c;
    c.setAngle(61);
    std::cout << (c == c0) << '\n';
    auto spread = linear();
    auto mode = spread;
    spread.setSpread(Gradient::RepeatSpread);
    mode.setCoordinateMode(Gradient::ObjectBoundingMode);
    std::cout << "spread-mode " << (spread == linear()) << ' ' << (mode == linear()) << '\n';
    return 0;
}
