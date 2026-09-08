#ifdef PK_SVG_QT_ORACLE
#include <QGuiApplication>
#include <QSvgRenderer>
#include <QPainter>
#include <QImage>
#include <QFontMetricsF>
#include <QPainterPath>
#else
#include "PkSvgRasterizer.h"
#include <PkFontRasterizer.h>
#endif
#include <cstdint>
#include <cstring>
#include <iostream>
#include <fstream>
#include <iomanip>

int main(int argc, char **argv)
{
#ifdef PK_SVG_QT_ORACLE
    QGuiApplication app(argc, argv);
#else
    (void)argc;
    (void)argv;
#endif
    if (argc > 1 && std::strcmp(argv[1], "--outline") == 0) {
#ifdef PK_SVG_QT_ORACLE
        QFont font("DejaVu Sans"); font.setPixelSize(100);
        QPainterPath path; path.addText(QPointF(), font, "AV");
        const double advance = QFontMetricsF(font).horizontalAdvance("AV");
#else
        PkFont font; font.setFamily("DejaVu Sans"); font.setPixelSize(100);
        double advance = 0;
        const auto path = PkFontRasterizer::outline("AV", font, &advance);
#endif
        std::cout << std::setprecision(17) << "advance " << advance << '\n';
        for (int i = 0; i < path.elementCount(); ++i) {
            const auto e = path.elementAt(i);
            std::cout << int(e.type) << ' ' << e.x << ' ' << e.y << '\n';
        }
        return 0;
    }
    const struct { const char *name; const char *svg; } cases[] = {
        {"viewBox", "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='100' viewBox='0 0 4 2'><rect width='2' height='2'/></svg>"},
        {"text", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 40'><text x='5' y='30' font-family='DejaVu Sans' font-size='24'>AV</text></svg>"},
        {"image", "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 0 4 2'><image x='1' y='0' width='2' height='2' xlink:href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII='/></svg>"},
        {"use", "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 0 4 2'><defs><rect id='r' width='1' height='2'/></defs><use x='1' xlink:href='#r'/></svg>"},
        {"gradient", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><linearGradient id='g'><stop offset='0' stop-color='black'/><stop offset='1' stop-color='white'/></linearGradient></defs><rect width='4' height='2' fill='url(#g)'/></svg>"},
        {"clip", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><clipPath id='c'><rect width='1' height='2'/></clipPath></defs><rect width='4' height='2' clip-path='url(#c)'/></svg>"},
        {"css", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><style>.r {fill:red;fill-opacity:0.5}</style><rect class='r' x='1' width='2' height='2'/></svg>"},
        {"path", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><path d='M.5 .5 C1 0 3 0 3.5 1.5 L.5 1.5Z' fill='#3865ab' stroke='black' stroke-width='.1'/></svg>"},
        {"arcAbsolute", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><path d='M.2 1 A.8 .8 0 0 1 1.8 1' fill='none' stroke='black' stroke-width='.2'/></svg>"},
        {"arcRelative", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><path d='M.2 1 a.8 .8 0 0 1 1.6 0' fill='none' stroke='black' stroke-width='.2'/></svg>"},
        {"tspan", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 40'><text x='5' y='30' font-family='DejaVu Sans' font-size='24'>A<tspan fill='red'>V</tspan></text></svg>"},
        {"fractionalViewBox", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='.2 .3 4.4 2.3'><rect width='2' height='2'/></svg>"},
        {"solidColor", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><solidColor id='c' solid-color='red' solid-opacity='.5'/></defs><rect width='4' height='2' fill='url(#c)'/></svg>"},
        {"arabic", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 40'><text x='5' y='30' font-family='DejaVu Sans' font-size='24'>سلام</text></svg>"},
        {"cjk", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 40'><text x='5' y='30' font-family='DejaVu Sans' font-size='24'>画</text></svg>"},
        {"nestedSvg", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><svg x='1' y='0' width='2' height='2' viewBox='0 0 1 1'><rect width='1' height='1'/></svg></svg>"},
        {"implicitViewBox", "<svg xmlns='http://www.w3.org/2000/svg'><rect x='1' y='2' width='4' height='2'/></svg>"},
        {"roundedRect", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><rect x='.1' y='.2' width='3.7' height='1.7' rx='.37' ry='.23'/></svg>"},
        {"lengthUnits", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><rect width='1in' height='12pt'/><rect y='30' width='1cm' height='5mm'/><rect y='60' width='20%' height='2pc'/></svg>"},
        {"gradientReference", "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 0 4 2'><defs><linearGradient id='a' x1='20%' x2='80%' y2='100%' spreadMethod='reflect' gradientTransform='translate(.1 .2)'><stop offset='0' stop-color='red'/><stop offset='1' stop-color='blue'/></linearGradient><linearGradient id='b' xlink:href='#a'/></defs><rect width='4' height='2' fill='url(#b)'/></svg>"},
        {"gradientAlpha", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><linearGradient id='a'><stop offset='0' stop-color='red' stop-opacity='.2'/><stop offset='1' stop-color='blue' stop-opacity='.8'/></linearGradient></defs><rect width='4' height='2' fill='url(#a)'/></svg>"},
        {"imageAlpha", "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 0 4 2'><image x='.3' y='.1' width='3.2' height='1.7' xlink:href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAYAAABytg0kAAAAGUlEQVR4nGP4z8AQwPCfYRoDA8P/O/+BPAA4cAa+YMCwbAAAAABJRU5ErkJggg=='/></svg>"},
        {"switch", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><switch><rect width='4' height='2' requiredFeatures='urn:unsupported' fill='red'/><rect width='3' height='2' display='none'/><rect width='2' height='2' requiredFeatures='http://www.w3.org/Graphics/SVG/feature/1.2/#Shape'/><rect width='4' height='2' fill='blue'/></switch></svg>"},
        {"localImage", "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 0 4 2'><image width='4' height='2' xlink:href='svg-oracle-image.ppm'/></svg>"},
        {"embeddedFont", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 40'><defs><font id='FixtureFont' horiz-adv-x='600'><font-face font-family='FixtureFont' units-per-em='1000'/><glyph unicode='A' d='M0 0L300 900L600 0Z'/><missing-glyph horiz-adv-x='400' d='M0 0H300V800H0Z'/></font></defs><text x='5' y='30' font-family='FixtureFont' font-size='24'>AB</text></svg>"},
        {"spanWhitespace", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 40'><text x='5' y='30' font-family='DejaVu Sans' font-size='24'>A <tspan fill='red'>V</tspan> A</text></svg>"},
        {"textArea", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><textArea x='5' y='5' width='80' height='90' font-family='DejaVu Sans' font-size='24'>one two three four</textArea></svg>"},
        {"radial", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><radialGradient id='g' cx='.4' cy='.5' r='.3' fx='.25' fy='.3'><stop offset='0' stop-color='red'/><stop offset='.4' stop-color='blue' stop-opacity='.5'/><stop offset='1' stop-color='green'/></radialGradient></defs><rect width='4' height='2' fill='url(#g)'/></svg>"},
        {"patternPaintServer", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><pattern id='p' patternUnits='userSpaceOnUse' width='2' height='2'><rect width='1' height='2' fill='red'/></pattern></defs><rect width='4' height='2' fill='url(#p)'/></svg>"},
        {"unknownPaintServer", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><futurePaint id='u'/></defs><rect width='4' height='2' fill='url(#u)'/></svg>"},
        {"textAreaBreak", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><textArea x='5' y='5' width='80' height='90' font-family='DejaVu Sans' font-size='24' text-anchor='middle'>one<tbreak/>two<tbreak/><tbreak/>three</textArea></svg>"},
        {"animationInitial", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><rect width='1' height='2' fill='green'><animateColor attributeName='fill' from='red' to='blue' dur='10s'/><animateTransform attributeName='transform' type='translate' from='1 0' to='3 0' dur='10s'/></rect></svg>"},
        {"fontStyle", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 40'><text x='95' y='30' font-family='DejaVu Sans' font-size='24' font-weight='700' font-style='oblique' text-anchor='end'>AV</text></svg>"},
        {"duplicateStops", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><linearGradient id='g'><stop offset='0' stop-color='red'/><stop offset='.5' stop-color='red'/><stop offset='.5' stop-color='blue'/><stop offset='1' stop-color='blue'/></linearGradient></defs><rect width='4' height='2' fill='url(#g)'/></svg>"},
        {"gradientUserSpace", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><linearGradient id='g' gradientUnits='userSpaceOnUse' x1='0' x2='100%'><stop offset='0' stop-color='red'/><stop offset='1' stop-color='blue'/></linearGradient></defs><rect width='4' height='2' fill='url(#g)'/></svg>"},
        {"rootLengths", "<svg xmlns='http://www.w3.org/2000/svg' width='1in' height='1cm'><rect width='30' height='20'/></svg>"},
        {"opaquePng", "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 0 4 2'><image width='4' height='2' xlink:href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAIAAAABCAIAAAB7QOjdAAAAD0lEQVR4nGP4z8DA8J8BAAf/Af8Bf4mnAAAAAElFTkSuQmCC'/></svg>"},
        {"jpeg", "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 0 4 2'><image width='4' height='2' xlink:href='data:image/jpeg;base64,/9j/4AAQSkZJRgABAQAAAQABAAD/2wBDAAEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQEBAQH/wAALCAABAAIBAREA/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/9oACAEBAAA/AP6QP+Cev/Jgn7Dv/Zn/AOzR/wCqX8FV/9k='/></svg>"}
    };
    {
        std::ofstream fixture("svg-oracle-image.ppm", std::ios::binary);
        fixture << "P6\n2 1\n255\n";
        const unsigned char rgb[] = {255, 0, 0, 0, 255, 0};
        fixture.write(reinterpret_cast<const char *>(rgb), sizeof(rgb));
    }
    std::ofstream pixels;
    if (argc > 1) pixels.open(argv[1], std::ios::binary);
    for (const auto &item : cases) {
#ifdef PK_SVG_QT_ORACLE
        QSvgRenderer renderer(QByteArray(item.svg));
        const QRect box = renderer.viewBox();
        QImage image(1000, 1000 * box.height() / box.width(), QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        renderer.render(&painter);
        painter.end();
#else
        const PkImage image = PkSvgRasterizer::render(item.svg, std::strlen(item.svg), 1000);
#endif
        std::uint64_t hash = 1469598103934665603ull;
        int nonWhite = 0;
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const std::uint32_t pixel = image.pixel(x, y);
                if (pixels) pixels.write(reinterpret_cast<const char *>(&pixel), sizeof(pixel));
                hash ^= pixel;
                hash *= 1099511628211ull;
                if (pixel != 0xffffffffu) ++nonWhite;
            }
        }
        std::cout << item.name << '\t' << image.width() << ' ' << image.height()
                  << ' ' << nonWhite << ' ' << hash << '\n';
    }
}
