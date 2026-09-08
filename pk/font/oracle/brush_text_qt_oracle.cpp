#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QString>

#include <cstdint>
#include <iostream>

int main(int argc, char **argv)
{
    QGuiApplication application(argc, argv);
    QFont font("DejaVu Sans");
    font.setPixelSize(24);
    const QString text = QStringLiteral("A");
    const QFontMetrics metrics(font);
    QRect rect = metrics.boundingRect(text);
    if (rect.isEmpty()) rect = QRect(0, 0, 1, 1);

    QImage image(rect.size(), QImage::Format_ARGB32);
    image.fill(Qt::white);
    QPainter painter(&image);
    painter.setFont(font);
    painter.setPen(Qt::black);
    painter.drawText(-rect.x(), -rect.y(), text);
    painter.end();

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

    std::cout << font.toString().toStdString() << '\n'
              << rect.x() << ' ' << rect.y() << ' ' << rect.width() << ' ' << rect.height() << '\n'
              << image.width() << ' ' << image.height() << ' ' << nonWhite << '\n'
              << std::hex << image.pixel(0, 0) << ' '
              << image.pixel(image.width() / 2, image.height() / 2) << std::dec << '\n'
              << hash << '\n';
}
