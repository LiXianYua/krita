/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

#include <array>
#include <random>
#include <memory>
#include <stdexcept>
#include <vector>

#include <PkImageRasterBackend.h>
#include <PkPainter.h>

class PkImageRasterBackendTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void blendsImageWithOpacity();
    void matchesQtArgb32SourceOverMatrix();
    void matchesQtShortSpansAndTails();
    void matchesQtPlusPixelsAndOverlappingMasks();
    void matchesQtTransformedClippedPathCoverage();
    void matchesQtStrokePixels();
    void matchesQtGradientPixels();
    void clipsToDestinationBounds();
    void rejectsUnsupportedOperations();
    void reportsDestinationDevicePixelRatio();
};

void PkImageRasterBackendTest::matchesQtGradientPixels()
{
    for (int kind = 0; kind < 3; ++kind) {
    for (int stopCount = 1; stopCount <= 3; ++stopCount) {
    for (double opacity : {0.25, 0.9, 1.0}) {
        for (int spread = 0; spread < 3; ++spread) {
            QImage qtImage(41, 23, QImage::Format_ARGB32);
            PkImage pkImage(41, 23, PkImage::Format_ARGB32);
            qtImage.fill(0x80602040u); pkImage.fill(0x80602040u);
            std::unique_ptr<QGradient> qtStorage;
            if (kind == 0) qtStorage = std::make_unique<QLinearGradient>(5, 3, 28, 17);
            else if (kind == 1) qtStorage = std::make_unique<QRadialGradient>(20, 12, 17, 16, 9);
            else qtStorage = std::make_unique<QConicalGradient>(20, 12, 37);
            QGradient &qtGradient = *qtStorage;
            auto pkGradient = PkGradient::linear(PkPointF(5, 3), PkPointF(28, 17));
            if (kind == 1) pkGradient = PkGradient::radial(PkPointF(20, 12), 17, PkPointF(16, 9));
            else if (kind == 2) pkGradient = PkGradient::conical(PkPointF(20, 12), 37);
            qtGradient.setColorAt(0, QColor(250, 15, 40, 71));
            pkGradient.setColorAt(0, PkColor(250, 15, 40, 71));
            if (stopCount >= 2) {
                qtGradient.setColorAt(1, QColor(20, 210, 185, 243));
                pkGradient.setColorAt(1, PkColor(20, 210, 185, 243));
            }
            if (stopCount == 3) {
                qtGradient.setColorAt(0.4, QColor(130, 75, 215, 160));
                pkGradient.setColorAt(0.4, PkColor(130, 75, 215, 160));
            }
            qtGradient.setSpread(static_cast<QGradient::Spread>(spread));
            pkGradient.setSpread(spread == 0 ? PkGradient::PadSpread :
                                 spread == 1 ? PkGradient::ReflectSpread : PkGradient::RepeatSpread);
            QBrush qtBrush(qtGradient);
            PkBrush pkBrush(pkGradient);
            qtBrush.setTransform(QTransform(1.25, 0.1, 0.2, 0.8, 1.5, -2));
            pkBrush.setTransform(PkTransform(1.25, 0.1, 0.2, 0.8, 1.5, -2));
            QPainter qtPainter(&qtImage);
            PkImageRasterBackend backend(pkImage);
            PkPainter painter(backend);
            qtPainter.setOpacity(opacity); painter.setOpacity(opacity);
            qtPainter.fillRect(QRectF(0, 0, 41, 23), qtBrush);
            painter.fillRect(PkRectF(0, 0, 41, 23), pkBrush);
            qtPainter.end();
            for (int y = 0; y < 23; ++y) for (int x = 0; x < 41; ++x) {
                const QString context = QStringLiteral("opacity=%1 spread=%2 x=%3 y=%4 Qt=%5 Pk=%6 kind=%7 stops=%8")
                    .arg(opacity).arg(spread).arg(x).arg(y)
                    .arg(qtImage.pixel(x, y), 8, 16, QLatin1Char('0'))
                    .arg(pkImage.pixel(x, y), 8, 16, QLatin1Char('0')).arg(kind).arg(stopCount);
                QVERIFY2(pkImage.pixel(x, y) == qtImage.pixel(x, y), qPrintable(context));
            }
        }
    }
    }
    }
}

void PkImageRasterBackendTest::matchesQtStrokePixels()
{
    for (bool dispatch : {false, true}) {
    for (double width : {0.0, 1.0, 3.25}) {
    for (bool antialias : {false, true}) {
        for (bool dashed : {false, true}) {
            QImage qtImage(40, 32, QImage::Format_ARGB32);
            PkImage pkImage(40, 32, PkImage::Format_ARGB32);
            qtImage.fill(0u); pkImage.fill(0u);
            QPainter qtPainter(&qtImage);
            PkImageRasterBackend backend(pkImage);
            PkPainter painter(backend);
            qtPainter.setRenderHint(QPainter::Antialiasing, antialias);
            painter.setRenderHint(PkPainter::Antialiasing, antialias);
            qtPainter.translate(3.5, 2.25); painter.translate(3.5, 2.25);
            QPainterPath qtPath;
            PkPainterPath pkPath;
            qtPath.moveTo(2, 3); pkPath.moveTo(2, 3);
            qtPath.lineTo(22, 5); pkPath.lineTo(22, 5);
            qtPath.lineTo(12, 24); pkPath.lineTo(12, 24);
            QPen qtPen(QColor(71, 133, 237, 198), width);
            PkPen pkPen(PkColor(71, 133, 237, 198), width);
            qtPen.setCapStyle(Qt::RoundCap); pkPen.setCapStyle(Pk::RoundCap);
            qtPen.setJoinStyle(Qt::MiterJoin); pkPen.setJoinStyle(Pk::MiterJoin);
            if (dashed) { qtPen.setStyle(Qt::DashLine); pkPen.setStyle(Pk::DashLine); }
            if (dispatch) {
                qtPainter.setPen(qtPen); painter.setPen(pkPen);
                qtPainter.drawPath(qtPath); painter.drawPath(pkPath);
                qtPainter.save(); painter.save();
                qtPainter.setPen(Qt::NoPen); painter.setPen(Pk::NoPen);
                qtPainter.drawLine(QPointF(1, 1), QPointF(10, 1));
                painter.drawLine(PkPointF(1, 1), PkPointF(10, 1));
                qtPainter.restore(); painter.restore();
                qtPainter.drawLine(QPointF(1, 1), QPointF(10, 1));
                painter.drawLine(PkPointF(1, 1), PkPointF(10, 1));
                if (width == 0) {
                    qtPainter.drawPoint(QPointF(29.5, 5.25));
                    painter.drawPoint(PkPointF(29.5, 5.25));
                }
            } else {
                qtPainter.strokePath(qtPath, qtPen);
                painter.strokePath(pkPath, pkPen);
            }
            qtPainter.end();
            for (int y = 0; y < 32; ++y) {
                for (int x = 0; x < 40; ++x) {
                    const QString context = QStringLiteral("aa=%1 dash=%2 x=%3 y=%4 Qt=%5 Pk=%6")
                        .arg(antialias).arg(dashed).arg(x).arg(y)
                        .arg(qtImage.pixel(x, y), 8, 16, QLatin1Char('0'))
                        .arg(pkImage.pixel(x, y), 8, 16, QLatin1Char('0'));
                    QVERIFY2(pkImage.pixel(x, y) == qtImage.pixel(x, y), qPrintable(context));
                }
            }
        }
    }
    }
    }
}

void PkImageRasterBackendTest::matchesQtTransformedClippedPathCoverage()
{
    // Exact area coverage, cubic subdivision, winding holes, device-space
    // clip persistence and Source clearing are independently rendered by Qt.
    for (bool antialias : {false, true}) {
    for (double opacity : {0.0, 0.25, 0.4, 0.9, 1.0}) {
    for (bool oddEven : {false, true}) {
        QImage qtImage(32, 24, QImage::Format_ARGB32);
        PkImage pkImage(32, 24, PkImage::Format_ARGB32);
        qtImage.fill(0x80604020u);
        pkImage.fill(0x80604020u);
        QPainter qtPainter(&qtImage);
        PkImageRasterBackend backend(pkImage);
        PkPainter painter(backend);
        qtPainter.setRenderHint(QPainter::Antialiasing, antialias);
        painter.setRenderHint(PkPainter::Antialiasing, antialias);
        qtPainter.translate(2.25, 1.5);
        painter.translate(2.25, 1.5);
        qtPainter.setClipRect(QRectF(1, 2, 23, 17));
        painter.setClipRect(PkRectF(1, 2, 23, 17));
        qtPainter.save();
        painter.save();
        qtPainter.rotate(13);
        painter.rotate(13);
        qtPainter.setOpacity(opacity);
        painter.setOpacity(opacity);
        QPainterPath qtPath;
        PkPainterPath pkPath;
        qtPath.moveTo(2, 2); pkPath.moveTo(2, 2);
        qtPath.cubicTo(30, -2, -2, 28, 25, 18);
        pkPath.cubicTo(30, -2, -2, 28, 25, 18);
        qtPath.lineTo(3, 20); pkPath.lineTo(3, 20);
        qtPath.closeSubpath(); pkPath.closeSubpath();
        qtPath.addRect(QRectF(5, 6, 8, 9));
        pkPath.addRect(PkRectF(5, 6, 8, 9));
        qtPath.setFillRule(oddEven ? Qt::OddEvenFill : Qt::WindingFill);
        pkPath.setFillRule(oddEven ? Pk::OddEvenFill : Pk::WindingFill);
        qtPainter.fillPath(qtPath, QColor(123, 231, 87, 179));
        painter.fillPath(pkPath, PkBrush(PkColor(123, 231, 87, 179)));
        qtPainter.restore();
        painter.restore();
        qtPainter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setCompositionMode(Pk::CompositionMode_Source);
        qtPainter.fillRect(QRectF(4, 4, 5, 6), Qt::transparent);
        painter.fillRect(PkRectF(4, 4, 5, 6), PkBrush(PkColor(Pk::transparent)));
        qtPainter.end();
        for (int y = 0; y < 24; ++y) {
            for (int x = 0; x < 32; ++x) {
                const QString context = QStringLiteral("oddEven=%1 x=%2 y=%3 Qt=%4 Pk=%5")
                    .arg(oddEven).arg(x).arg(y)
                    .arg(qtImage.pixel(x, y), 8, 16, QLatin1Char('0'))
                    .arg(pkImage.pixel(x, y), 8, 16, QLatin1Char('0'));
                QVERIFY2(pkImage.pixel(x, y) == qtImage.pixel(x, y), qPrintable(context));
            }
        }
    }
    }
    }
}

void PkImageRasterBackendTest::matchesQtPlusPixelsAndOverlappingMasks()
{
    // A fixed SourceOver backend, pre-saturation opacity, or lost saved
    // composition state all change these pixels. Qt supplies the reference.
    for (int width : {1, 2, 3, 4, 7, 8, 9, 15, 17, 65}) {
        QImage qtSource(width, 24, QImage::Format_ARGB32);
        QImage qtOriginal(width, 24, QImage::Format_ARGB32);
        PkImage pkSource(width, 24, PkImage::Format_ARGB32);
        PkImage pkOriginal(width, 24, PkImage::Format_ARGB32);
        std::mt19937 random(1847);
        constexpr unsigned edges[] = {0, 1, 63, 127, 128, 254, 255};
        for (int y = 0; y < 24; ++y) {
            for (int x = 0; x < width; ++x) {
                // The first rows are white glyph coverage masks, including
                // transparent, low alpha, and saturated overlap boundaries.
                const uint32_t source = y < 7 ? (edges[y] << 24) | 0xffffffu : random();
                const uint32_t destination = y < 7 ?
                    (edges[(x + y) % 7] << 24) | 0xffffffu : random();
                qtSource.setPixel(x, y, source);
                pkSource.setPixel(x, y, source);
                qtOriginal.setPixel(x, y, destination);
                pkOriginal.setPixel(x, y, destination);
            }
        }
        for (double opacity : {0.0, 1.0 / 256, 0.1, 0.25, 0.5, 0.9, 255.0 / 256, 1.0}) {
            QImage qtDestination = qtOriginal;
            PkImage pkDestination = pkOriginal;
            QPainter qtPainter(&qtDestination);
            PkImageRasterBackend backend(pkDestination);
            PkPainter painter(backend);
            qtPainter.setOpacity(opacity);
            painter.setOpacity(opacity);
            qtPainter.setCompositionMode(QPainter::CompositionMode_Plus);
            painter.setCompositionMode(Pk::CompositionMode_Plus);
            for (int pass = 0; pass < 3; ++pass) {
                const int offset = pass - 1;
                qtPainter.drawImage(QPoint(offset, 0), qtSource);
                painter.drawImage(PkRectF(offset, 0, width, 24), pkSource);
                for (int y = 0; y < 24; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const QString context = QStringLiteral("width=%1 opacity=%2 pass=%3 x=%4 y=%5 Qt=%6 Pk=%7")
                            .arg(width).arg(opacity, 0, 'g', 17).arg(pass).arg(x).arg(y)
                            .arg(qtDestination.pixel(x, y), 8, 16, QLatin1Char('0'))
                            .arg(pkDestination.pixel(x, y), 8, 16, QLatin1Char('0'));
                        QVERIFY2(pkDestination.pixel(x, y) == qtDestination.pixel(x, y), qPrintable(context));
                    }
                }
            }
            qtPainter.save();
            painter.save();
            qtPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setCompositionMode(Pk::CompositionMode_SourceOver);
            qtPainter.setOpacity(0.25);
            painter.setOpacity(0.25);
            qtPainter.drawImage(QPoint(), qtSource);
            painter.drawImage(PkRectF(0, 0, width, 24), pkSource);
            qtPainter.restore();
            painter.restore();
            QCOMPARE(painter.compositionMode(), Pk::CompositionMode_Plus);
            QCOMPARE(painter.opacity(), opacity);
            qtPainter.drawImage(QPoint(), qtSource);
            painter.drawImage(PkRectF(0, 0, width, 24), pkSource);
            qtPainter.end();
            for (int y = 0; y < 24; ++y) {
                for (int x = 0; x < width; ++x) {
                    QCOMPARE(pkDestination.pixel(x, y), qtDestination.pixel(x, y));
                }
            }
        }
    }
}

void PkImageRasterBackendTest::reportsDestinationDevicePixelRatio()
{
    PkImage destination(4, 4, PkImage::Format_ARGB32);
    destination.setDevicePixelRatio(1.75);
    PkImageRasterBackend backend(destination);
    PkPainter painter(backend);

    QCOMPARE(painter.devicePixelRatio(), 1.75);
}

void PkImageRasterBackendTest::blendsImageWithOpacity()
{
    PkImage destination(3, 1, PkImage::Format_ARGB32);
    destination.setPixel(0, 0, 0xff202020u);
    destination.setPixel(1, 0, 0x804080c0u);
    destination.setPixel(2, 0, 0xd4616161u);

    PkImage opaqueSource(1, 1, PkImage::Format_ARGB32);
    opaqueSource.setPixel(0, 0, 0xffe06020u);
    PkImage translucentSource(1, 1, PkImage::Format_ARGB32);
    translucentSource.setPixel(0, 0, 0x80e02060u);
    PkImage reviewerSource(1, 1, PkImage::Format_ARGB32);
    reviewerSource.setPixel(0, 0, 0xd49c22acu);

    PkImageRasterBackend backend(destination);
    PkPainter painter(backend);
    painter.setOpacity(0.25);
    painter.drawImage(PkRectF(0, 0, 1, 1), opaqueSource);
    painter.setOpacity(0.5);
    painter.drawImage(PkRectF(1, 0, 1, 1), translucentSource);
    painter.setOpacity(0.1);
    painter.drawImage(PkRectF(2, 0, 1, 1), reviewerSource);

    QCOMPARE(destination.pixel(0, 0), 0xff4f3020u);
    QCOMPARE(destination.pixel(1, 0), 0xa0805a9au);
    QCOMPARE(destination.pixel(2, 0), 0xd7665b68u);
}

void PkImageRasterBackendTest::matchesQtArgb32SourceOverMatrix()
{
    constexpr std::array<unsigned, 12> boundaries {
        0, 1, 2, 15, 16, 63, 64, 127, 128, 129, 254, 255
    };
    constexpr std::array<qreal, 18> opacities {
        0.0, 0.1, 1.0 / 256.0, 1.0 / 255.0, 0.01, 0.25,
        63.0 / 256.0, 64.0 / 256.0, 127.0 / 256.0,
        0.5, 128.0 / 255.0, 129.0 / 256.0, 0.75, 0.9,
        254.0 / 256.0, 254.0 / 255.0, 255.0 / 256.0, 1.0
    };

    std::vector<uint32_t> sources;
    std::vector<uint32_t> destinations;
    for (unsigned a : boundaries) {
        for (unsigned value : boundaries) {
            sources.push_back(a == 0 ? 0 :
                              (a << 24) |
                                  (value << 16) |
                                  (((value * 73u + 19u) & 0xffu) << 8) |
                                  ((value * 151u + 7u) & 0xffu));
            destinations.push_back(a == 0 ? 0 :
                                   (a << 24) | (value << 16) |
                                       (value << 8) | value);
        }
    }

    const int pixelCount = static_cast<int>(sources.size() * destinations.size());
    for (qreal opacity : opacities) {
        QImage qtSource(pixelCount, 1, QImage::Format_ARGB32);
        QImage qtDestination(pixelCount, 1, QImage::Format_ARGB32);
        PkImage pkSource(pixelCount, 1, PkImage::Format_ARGB32);
        PkImage pkDestination(pixelCount, 1, PkImage::Format_ARGB32);

        int x = 0;
        for (uint32_t source : sources) {
            for (uint32_t destination : destinations) {
                qtSource.setPixel(x, 0, source);
                qtDestination.setPixel(x, 0, destination);
                pkSource.setPixel(x, 0, source);
                pkDestination.setPixel(x, 0, destination);
                ++x;
            }
        }

        QPainter qtPainter(&qtDestination);
        qtPainter.setOpacity(opacity);
        qtPainter.drawImage(QPoint(), qtSource);
        qtPainter.end();

        PkImageRasterBackend backend(pkDestination);
        PkPainter painter(backend);
        painter.setOpacity(opacity);
        painter.drawImage(PkRectF(0, 0, pixelCount, 1), pkSource);

        int mismatches = 0;
        int firstMismatch = -1;
        QString mismatchDetails;
        for (int i = 0; i < pixelCount; ++i) {
            if (qtDestination.pixel(i, 0) != pkDestination.pixel(i, 0)) {
                if (firstMismatch < 0) {
                    firstMismatch = i;
                }
                if (mismatches < 8) {
                    const auto source = sources.at(
                        static_cast<std::size_t>(i) / destinations.size());
                    const auto destination = destinations.at(
                        static_cast<std::size_t>(i) % destinations.size());
                    mismatchDetails += QStringLiteral(
                        " [%1 src=%2 dst=%3 Qt=%4 Pk=%5]")
                        .arg(i)
                        .arg(source, 8, 16, QLatin1Char('0'))
                        .arg(destination, 8, 16, QLatin1Char('0'))
                        .arg(qtDestination.pixel(i, 0), 8, 16, QLatin1Char('0'))
                        .arg(pkDestination.pixel(i, 0), 8, 16, QLatin1Char('0'));
                }
                ++mismatches;
            }
        }

        const QString diagnostic = QStringLiteral(
            "opacity=%1 mismatches=%2/%3 first=%4 Qt=%5 Pk=%6")
            .arg(opacity, 0, 'g', 17)
            .arg(mismatches)
            .arg(pixelCount)
            .arg(firstMismatch)
            .arg(firstMismatch >= 0 ? qtDestination.pixel(firstMismatch, 0) : 0,
                 8, 16, QLatin1Char('0'))
            .arg(firstMismatch >= 0 ? pkDestination.pixel(firstMismatch, 0) : 0,
                 8, 16, QLatin1Char('0')) + mismatchDetails;
        QVERIFY2(mismatches == 0, qPrintable(diagnostic));
    }
}

void PkImageRasterBackendTest::matchesQtShortSpansAndTails()
{
    constexpr std::array<qreal, 10> opacities {
        0.0, 1.0 / 256.0, 0.1, 0.25, 127.0 / 256.0,
        0.5, 0.75, 0.9, 255.0 / 256.0, 1.0
    };

    // Exact minimum reproduction from the production-closure review. The
    // transparent neighbor is significant because Qt fetches ARGB32 pixels
    // in groups before composing and storing the short span.
    {
        QImage qtSource(2, 1, QImage::Format_ARGB32);
        QImage qtDestination(2, 1, QImage::Format_ARGB32);
        PkImage pkSource(2, 1, PkImage::Format_ARGB32);
        PkImage pkDestination(2, 1, PkImage::Format_ARGB32);
        constexpr std::array<uint32_t, 2> sources {0x00ebcc83u, 0x3caf8806u};
        constexpr std::array<uint32_t, 2> destinations {0x00cbcbcbu, 0x3c818181u};
        for (int x = 0; x < 2; ++x) {
            qtSource.setPixel(x, 0, sources[static_cast<std::size_t>(x)]);
            qtDestination.setPixel(x, 0, destinations[static_cast<std::size_t>(x)]);
            pkSource.setPixel(x, 0, sources[static_cast<std::size_t>(x)]);
            pkDestination.setPixel(x, 0, destinations[static_cast<std::size_t>(x)]);
        }

        QPainter qtPainter(&qtDestination);
        qtPainter.setOpacity(0.9);
        qtPainter.drawImage(QPoint(), qtSource);
        qtPainter.end();

        PkImageRasterBackend backend(pkDestination);
        PkPainter painter(backend);
        painter.setOpacity(0.9);
        painter.drawImage(PkRectF(0, 0, 2, 1), pkSource);

        QCOMPARE(qtDestination.pixel(1, 0), 0x65998540u);
        QCOMPARE(pkDestination.pixel(1, 0), qtDestination.pixel(1, 0));
    }

    constexpr int seedCount = 200;
    for (int width = 1; width <= 65; ++width) {
        QImage qtSource(width, seedCount, QImage::Format_ARGB32);
        QImage originalQtDestination(width, seedCount, QImage::Format_ARGB32);
        PkImage pkSource(width, seedCount, PkImage::Format_ARGB32);
        PkImage originalPkDestination(width, seedCount, PkImage::Format_ARGB32);

        for (int seed = 0; seed < seedCount; ++seed) {
            std::mt19937 random(static_cast<std::mt19937::result_type>(seed));
            for (int x = 0; x < width; ++x) {
                const uint32_t source = random();
                const unsigned destinationAlpha = random() >> 24;
                const unsigned gray = random() & 0xffu;
                const uint32_t destination = (destinationAlpha << 24) |
                    (gray << 16) | (gray << 8) | gray;
                qtSource.setPixel(x, seed, source);
                originalQtDestination.setPixel(x, seed, destination);
                pkSource.setPixel(x, seed, source);
                originalPkDestination.setPixel(x, seed, destination);
            }
        }

        for (qreal opacity : opacities) {
            QImage qtDestination = originalQtDestination;
            PkImage pkDestination = originalPkDestination;

            QPainter qtPainter(&qtDestination);
            qtPainter.setOpacity(opacity);
            qtPainter.drawImage(QPoint(), qtSource);
            qtPainter.end();

            PkImageRasterBackend backend(pkDestination);
            PkPainter painter(backend);
            painter.setOpacity(opacity);
            painter.drawImage(PkRectF(0, 0, width, seedCount), pkSource);

            for (int seed = 0; seed < seedCount; ++seed) {
                for (int x = 0; x < width; ++x) {
                    const uint32_t qtPixel = qtDestination.pixel(x, seed);
                    const uint32_t pkPixel = pkDestination.pixel(x, seed);
                    const QString diagnostic = QStringLiteral(
                        "width=%1 tail=%2 seed=%3 x=%4 opacity=%5 "
                        "src=%6 dst=%7 Qt=%8 Pk=%9")
                        .arg(width)
                        .arg(width % 8)
                        .arg(seed)
                        .arg(x)
                        .arg(opacity, 0, 'g', 17)
                        .arg(qtSource.pixel(x, seed), 8, 16, QLatin1Char('0'))
                        .arg(originalQtDestination.pixel(x, seed), 8, 16,
                             QLatin1Char('0'))
                        .arg(qtPixel, 8, 16, QLatin1Char('0'))
                        .arg(pkPixel, 8, 16, QLatin1Char('0'));
                    QVERIFY2(pkPixel == qtPixel, qPrintable(diagnostic));
                }
            }
        }
    }
}

void PkImageRasterBackendTest::clipsToDestinationBounds()
{
    PkImage destination(1, 1, PkImage::Format_ARGB32);
    destination.fill(0xff000000u);

    PkImage source(2, 1, PkImage::Format_ARGB32);
    source.setPixel(0, 0, 0xffff0000u);
    source.setPixel(1, 0, 0xff00ff00u);

    PkImageRasterBackend backend(destination);
    PkPainter painter(backend);
    painter.drawImage(PkRectF(-1, 0, 2, 1), source);

    QCOMPARE(destination.pixel(0, 0), 0xff00ff00u);
}

void PkImageRasterBackendTest::rejectsUnsupportedOperations()
{
    PkImage destination(1, 1, PkImage::Format_ARGB32);
    PkImage source(1, 1, PkImage::Format_ARGB32);
    PkImageRasterBackend backend(destination);
    PkPainter painter(backend);

    QVERIFY_EXCEPTION_THROWN(painter.drawRect(PkRectF(0, 0, 1, 1)), std::logic_error);

    PkImage unsupportedDestination(1, 1, PkImage::Format_RGBA8888);
    PkImageRasterBackend unsupportedBackend(unsupportedDestination);
    PkPainter unsupportedPainter(unsupportedBackend);
    QVERIFY_EXCEPTION_THROWN(
        unsupportedPainter.drawImage(PkRectF(0, 0, 1, 1), source),
        std::invalid_argument);
}

QTEST_MAIN(PkImageRasterBackendTest)

#include "PkImageRasterBackendTest.moc"
