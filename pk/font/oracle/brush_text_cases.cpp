// Test-only independent Qt/native brush boundary oracle. Neither backend shares
// layout, metrics or rasterization code with the other.
#ifdef PK_FONT_QT_ORACLE
#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTextLayout>
#include <QGlyphRun>
#else
#include "PkFontRasterizer.h"
#endif
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>

int main(int argc, char **argv)
{
#ifdef PK_FONT_QT_ORACLE
    QGuiApplication app(argc, argv);
#else
    (void)argc;
    (void)argv;
#endif
    std::ofstream raw;
    if (argc == 3 && std::string(argv[1]) == "--pixels") {
        raw.open(argv[2], std::ios::binary);
        if (!raw) return 2;
    }
    const char *cases[] = {"A", "AV", "画", "e\xcc\x81", "سلام", "j", " ", " A ", "ffi", "abc אבג"};
    for (const char *text : cases) {
#ifdef PK_FONT_QT_ORACLE
        QFont font("DejaVu Sans");
        font.setPixelSize(24);
        const QString string = QString::fromUtf8(text);
        QRect bounds = QFontMetrics(font).boundingRect(string);
        if (argc > 1 && std::string(argv[1]) == "--details") {
            std::cerr << text << " bounds " << bounds.x() << ',' << bounds.y() << ',' << bounds.width() << ',' << bounds.height() << '\n';
            QTextLayout layout(string, font);
            layout.beginLayout(); layout.createLine(); layout.endLayout();
            for (const QGlyphRun &run : layout.glyphRuns()) {
                std::cerr << " font " << run.rawFont().familyName().toStdString() << ' '
                          << run.rawFont().styleName().toStdString() << " px=" << run.rawFont().pixelSize() << '\n';
                for (int i = 0; i < run.glyphIndexes().size(); ++i) {
                    const auto glyph = run.glyphIndexes()[i];
                    const auto box = run.rawFont().boundingRect(glyph);
                    std::cerr << glyph << " at " << run.positions()[i].x() << " box " << box.x() << ',' << box.width() << '\n';
                }
            }
        }
        if (bounds.isEmpty()) bounds = QRect(0, 0, 1, 1);
        QImage image(bounds.size(), QImage::Format_ARGB32);
        image.fill(Qt::white);
        QPainter painter(&image);
        painter.setFont(font);
        painter.setPen(Qt::black);
        painter.drawText(-bounds.x(), -bounds.y(), string);
        painter.end();
#else
        PkFont font("DejaVu Sans");
        font.setPixelSize(24);
        const PkImage image = PkFontRasterizer::render(PkString(text), font);
#endif
        std::uint64_t hash = 1469598103934665603ull;
        int nonWhite = 0;
        if (raw.is_open()) {
            const std::uint32_t size[] = {static_cast<std::uint32_t>(image.width()), static_cast<std::uint32_t>(image.height())};
            raw.write(reinterpret_cast<const char *>(size), sizeof(size));
        }
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const std::uint32_t pixel = image.pixel(x, y);
                if (raw.is_open()) raw.write(reinterpret_cast<const char *>(&pixel), sizeof(pixel));
                hash ^= pixel;
                hash *= 1099511628211ull;
                if (pixel != 0xffffffffu) ++nonWhite;
            }
        }
        std::cout << text << '\t' << image.width() << ' ' << image.height()
                  << ' ' << nonWhite << ' ' << hash << '\n';
    }
    return raw.is_open() && !raw ? 2 : 0;
}
