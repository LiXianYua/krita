#ifdef PK_SVG_QT_ORACLE
#include <QGuiApplication>
#include <QSvgRenderer>
#include <QPainter>
#include <QImage>
#else
#include "PkSvgRasterizer.h"
#endif
#include <cstdint>
#include <cstring>
#include <iostream>

int main(int argc, char **argv)
{
#ifdef PK_SVG_QT_ORACLE
    QGuiApplication app(argc, argv);
#else
    (void)argc;
    (void)argv;
#endif
    const struct { const char *name; const char *svg; } cases[] = {
        {"viewBox", "<svg xmlns='http://www.w3.org/2000/svg' width='100' height='100' viewBox='0 0 4 2'><rect width='2' height='2'/></svg>"},
        {"text", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 40'><text x='5' y='30' font-family='DejaVu Sans' font-size='24'>AV</text></svg>"},
        {"image", "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 0 4 2'><image x='1' y='0' width='2' height='2' xlink:href='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mNk+A8AAQUBAScY42YAAAAASUVORK5CYII='/></svg>"},
        {"use", "<svg xmlns='http://www.w3.org/2000/svg' xmlns:xlink='http://www.w3.org/1999/xlink' viewBox='0 0 4 2'><defs><rect id='r' width='1' height='2'/></defs><use x='1' xlink:href='#r'/></svg>"},
        {"gradient", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><linearGradient id='g'><stop offset='0' stop-color='black'/><stop offset='1' stop-color='white'/></linearGradient></defs><rect width='4' height='2' fill='url(#g)'/></svg>"},
        {"clip", "<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 4 2'><defs><clipPath id='c'><rect width='1' height='2'/></clipPath></defs><rect width='4' height='2' clip-path='url(#c)'/></svg>"}
    };
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
                hash ^= pixel;
                hash *= 1099511628211ull;
                if (pixel != 0xffffffffu) ++nonWhite;
            }
        }
        std::cout << item.name << '\t' << image.width() << ' ' << image.height()
                  << ' ' << nonWhite << ' ' << hash << '\n';
    }
}
