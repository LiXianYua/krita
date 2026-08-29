/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "PkQPainterAdapter.h"

#include <QtGui/QImage>
#include <QtGui/QPainter>
#include <QtTest/QtTest>

#include "PkFlakeBridge.h"
#include <PkPainter.h>

namespace
{

QColor pixelColor(const QImage &image, int x, int y)
{
    return image.pixelColor(x, y);
}

int coloredPixels(const QImage &image, const QRect &area)
{
    int count = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            if (pixelColor(image, x, y).alpha() != 0) {
                ++count;
            }
        }
    }
    return count;
}

}

class PkQPainterAdapterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void restoresSavedPenAndBrushState();
    void clipsTranslucentCropPath();
    void drawsKnifePrimitives();
    void appliesKarbonTransformToRectangle();
    void blitsSmartPatchImage();
};

void PkQPainterAdapterTest::restoresSavedPenAndBrushState()
{
    QImage image(48, 24, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter qtPainter(&image);
    PkQPainterAdapter backend(qtPainter);
    PkPainter painter(backend);

    PkPen redPen(PkColor(255, 0, 0), 2.0);
    painter.setPen(redPen);
    painter.setBrush(PkBrush(PkColor(0, 0, 255)));
    painter.save();

    PkPen greenPen(PkColor(0, 255, 0), 2.0);
    painter.setPen(greenPen);
    painter.setBrush(PkBrush(PkColor(255, 255, 0)));
    painter.drawRect(PkRectF(3, 3, 14, 14));

    painter.restore();
    painter.drawRect(PkRectF(27, 3, 14, 14));
    qtPainter.end();

    QCOMPARE(pixelColor(image, 10, 10), QColor(255, 255, 0));
    QCOMPARE(pixelColor(image, 3, 10), QColor(0, 255, 0));
    QCOMPARE(pixelColor(image, 34, 10), QColor(0, 0, 255));
    QCOMPARE(pixelColor(image, 27, 10), QColor(255, 0, 0));
}

void PkQPainterAdapterTest::clipsTranslucentCropPath()
{
    QImage image(32, 32, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter qtPainter(&image);
    PkQPainterAdapter backend(qtPainter);
    PkPainter painter(backend);

    painter.setPen(Qt::NoPen);
    painter.setBrush(PkBrush(PkColor(240, 20, 40, 128)));
    painter.setClipRect(PkRectF(6, 7, 18, 16), Qt::IntersectClip);
    PkPainterPath cropShade;
    cropShade.addRect(PkRectF(0, 0, 32, 32));
    painter.drawPath(cropShade);
    qtPainter.end();

    const QColor inside = pixelColor(image, 12, 12);
    QCOMPARE(inside.alpha(), 128);
    QVERIFY(qAbs(inside.red() - 240) <= 1);
    QVERIFY(qAbs(inside.green() - 20) <= 1);
    QVERIFY(qAbs(inside.blue() - 40) <= 1);
    QCOMPARE(pixelColor(image, 2, 2), QColor(0, 0, 0, 0));
    QCOMPARE(pixelColor(image, 25, 12), QColor(0, 0, 0, 0));
}

void PkQPainterAdapterTest::drawsKnifePrimitives()
{
    QImage image(72, 48, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter qtPainter(&image);
    PkQPainterAdapter backend(qtPainter);
    PkPainter painter(backend);

    PkPen pen(PkColor(255, 255, 255), 2.0);
    pen.setCapStyle(Qt::FlatCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.setRenderHint(static_cast<unsigned>(QPainter::Antialiasing), false);
    painter.drawLine(PkPointF(3, 6), PkPointF(22, 6));
    painter.drawArc(PkRectF(27, 2, 16, 16), 0, 180 * 16);
    painter.drawEllipse(PkPointF(55, 10), 8, 6);

    PkPolygonF polygon;
    polygon.append(PkPointF(8, 29));
    polygon.append(PkPointF(20, 42));
    polygon.append(PkPointF(3, 42));
    painter.drawPolygon(polygon);
    qtPainter.end();

    QVERIFY(coloredPixels(image, QRect(2, 4, 22, 5)) >= 30);
    QVERIFY(coloredPixels(image, QRect(26, 1, 19, 19)) >= 12);
    QVERIFY(coloredPixels(image, QRect(46, 2, 19, 17)) >= 24);
    QVERIFY(coloredPixels(image, QRect(2, 28, 20, 16)) >= 30);
}

void PkQPainterAdapterTest::appliesKarbonTransformToRectangle()
{
    QImage image(44, 32, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter qtPainter(&image);
    PkQPainterAdapter backend(qtPainter);
    PkPainter painter(backend);

    painter.setPen(Qt::NoPen);
    painter.setBrush(PkBrush(PkColor(20, 210, 80)));
    PkTransform transform;
    transform.translate(21, 11);
    painter.setTransform(transform);
    painter.drawRect(PkRectF(0, 0, 9, 7));
    qtPainter.end();

    QCOMPARE(pixelColor(image, 24, 14), QColor(20, 210, 80));
    QCOMPARE(pixelColor(image, 3, 3), QColor(0, 0, 0, 0));
}

void PkQPainterAdapterTest::blitsSmartPatchImage()
{
    QImage image(30, 24, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter qtPainter(&image);
    PkQPainterAdapter backend(qtPainter);
    PkPainter painter(backend);

    PkImage patch(3, 2, PkImage::Format_ARGB32_Premultiplied);
    patch.fill(0xff3264c8U);
    painter.drawImage(PkRectF(8, 6, 12, 8), patch);
    qtPainter.end();

    QCOMPARE(pixelColor(image, 12, 9), QColor(0x32, 0x64, 0xc8));
    QCOMPARE(pixelColor(image, 3, 3), QColor(0, 0, 0, 0));
}

QTEST_GUILESS_MAIN(PkQPainterAdapterTest)

#include "PkQPainterAdapterTest.moc"
