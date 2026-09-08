#include <QByteArray>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QVector>

#include <cstdint>
#include <iostream>

int main()
{
    const QByteArray svg(
        "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 4 2\">"
        "<g transform=\"translate(1 0)\" opacity=\"0.5\">"
        "<rect width=\"2\" height=\"2\" fill=\"#ff0000\"/>"
        "</g></svg>");
    QSvgRenderer renderer(svg);
    const QRect box = renderer.viewBox();
    QImage image(1000, 1000 * box.height() / box.width(), QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    renderer.render(&painter);
    painter.end();
    std::cout << std::hex << image.pixel(100, 250) << ' '
              << image.pixel(300, 250) << ' '
              << image.pixel(700, 250) << std::dec << '\n';
    QVector<QRgb> table;
    for (int i = 0; i < 256; ++i) table.push_back(qRgb(i, i, i));
    image = image.convertToFormat(QImage::Format_Indexed8, table);

    std::uint64_t hash = 1469598103934665603ull;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            hash ^= image.pixelIndex(x, y);
            hash *= 1099511628211ull;
        }
    }
    std::cout << box.x() << ' ' << box.y() << ' ' << box.width() << ' ' << box.height() << '\n'
              << image.width() << ' ' << image.height() << '\n'
              << image.pixelIndex(100, 250) << ' '
              << image.pixelIndex(300, 250) << ' '
              << image.pixelIndex(700, 250) << '\n'
              << hash << '\n';
}
