/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>

#include <stdexcept>

#include <PkImageRasterBackend.h>
#include <PkPainter.h>

class PkImageRasterBackendTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void blendsImageWithOpacity();
    void clipsToDestinationBounds();
    void rejectsUnsupportedOperations();
};

void PkImageRasterBackendTest::blendsImageWithOpacity()
{
    PkImage destination(2, 1, PkImage::Format_ARGB32);
    destination.setPixel(0, 0, 0xff202020u);
    destination.setPixel(1, 0, 0x804080c0u);

    PkImage opaqueSource(1, 1, PkImage::Format_ARGB32);
    opaqueSource.setPixel(0, 0, 0xffe06020u);
    PkImage translucentSource(1, 1, PkImage::Format_ARGB32);
    translucentSource.setPixel(0, 0, 0x80e02060u);

    PkImageRasterBackend backend(destination);
    PkPainter painter(backend);
    painter.setOpacity(0.25);
    painter.drawImage(PkRectF(0, 0, 1, 1), opaqueSource);
    painter.setOpacity(0.5);
    painter.drawImage(PkRectF(1, 0, 1, 1), translucentSource);

    QCOMPARE(destination.pixel(0, 0), 0xff4f3020u);
    QCOMPARE(destination.pixel(1, 0), 0xa0805a9au);
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
