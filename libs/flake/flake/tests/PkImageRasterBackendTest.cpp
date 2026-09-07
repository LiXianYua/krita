/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>
#include <QImage>
#include <QPainter>

#include <array>
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
    void clipsToDestinationBounds();
    void rejectsUnsupportedOperations();
};

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
