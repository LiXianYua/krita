/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <QTest>
#include <QBuffer>
#include <QPainter>
#include <QPainterPath>
#include <QSvgGenerator>
#include <QSvgRenderer>
#include <PkPainter.h>
#include "../../svg/PkSvgPainterBackend.h"

class PkSvgPainterBackendTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void preservesVectorPathPainting();
    void preservesGradientAndImagePainting();
};

void PkSvgPainterBackendTest::preservesVectorPathPainting()
{
    // A missing transform, fill, cubic, or restored pen changes rendered pixels.
    // Qt generates the reference SVG independently from the native commands.
    QByteArray reference;
    QBuffer buffer(&reference);
    buffer.open(QIODevice::WriteOnly);
    QSvgGenerator generator;
    generator.setOutputDevice(&buffer);
    generator.setSize(QSize(48, 40));
    generator.setViewBox(QRect(0, 0, 48, 40));
    generator.setResolution(72);
    QPainter qt(&generator);
    PkSvgPainterBackend backend(PkRectF(0, 0, 48, 40));
    PkPainter pk(backend);
    qt.translate(4, 3); pk.translate(4, 3);
    qt.setOpacity(.7); pk.setOpacity(.7);
    QPainterPath qtPath;
    PkPainterPath pkPath;
    qtPath.moveTo(1, 2); pkPath.moveTo(1, 2);
    qtPath.cubicTo(7, 25, 21, -3, 32, 25); pkPath.cubicTo(7, 25, 21, -3, 32, 25);
    qtPath.lineTo(1, 25); pkPath.lineTo(1, 25);
    qtPath.closeSubpath(); pkPath.closeSubpath();
    qt.fillPath(qtPath, QColor(60, 140, 230, 180));
    pk.fillPath(pkPath, PkColor(60, 140, 230, 180));
    qt.save(); pk.save();
    qt.setPen(QPen(QColor(200, 50, 40), 2)); pk.setPen(PkPen(PkColor(200, 50, 40), 2));
    qt.drawLine(QPointF(2, 31), QPointF(32, 31)); pk.drawLine(PkLineF(2, 31, 32, 31));
    qt.restore(); pk.restore();
    qt.drawLine(QPointF(2, 34), QPointF(32, 34)); pk.drawLine(PkLineF(2, 34, 32, 34));
    qt.end();

    const std::string native = backend.document();
    QVERIFY(!native.empty());
    QSvgRenderer expected(reference), actual(QByteArray::fromStdString(native));
    QVERIFY(expected.isValid()); QVERIFY(actual.isValid());
    QImage a(48, 40, QImage::Format_ARGB32), b(a.size(), a.format());
    a.fill(0); b.fill(0);
    QPainter qa(&a), qb(&b);
    expected.render(&qa); actual.render(&qb);
    qa.end(); qb.end();
    for (int y = 0; y < a.height(); ++y) for (int x = 0; x < a.width(); ++x)
        QCOMPARE(b.pixel(x, y), a.pixel(x, y));
}
void PkSvgPainterBackendTest::preservesGradientAndImagePainting()
{
    // Stop alpha, gradient units/transform/spread, and image placement are
    // measured against Qt's independently serialized SVG (not native helpers).
    for (bool radial : {false, true}) {
        QByteArray reference;
        QBuffer buffer(&reference); buffer.open(QIODevice::WriteOnly);
        QSvgGenerator generator;
        generator.setOutputDevice(&buffer);
        generator.setSize(QSize(40, 36)); generator.setViewBox(QRect(0, 0, 40, 36));
        generator.setResolution(72);
        QPainter qt(&generator);
        PkSvgPainterBackend backend(PkRectF(0, 0, 40, 36));
        PkPainter pk(backend);
        QLinearGradient linear(0, 0, 1, 1);
        QRadialGradient round(QPointF(.5, .5), .6, QPointF(.4, .6));
        QGradient qg = radial ? QGradient(round) : QGradient(linear);
        auto pg = radial ? PkGradient::radial(PkPointF(.5, .5), .6, PkPointF(.4, .6))
                         : PkGradient::linear(PkPointF(0, 0), PkPointF(1, 1));
        qg.setCoordinateMode(QGradient::ObjectBoundingMode); pg.setCoordinateMode(PkGradient::ObjectBoundingMode);
        qg.setSpread(QGradient::ReflectSpread); pg.setSpread(PkGradient::ReflectSpread);
        qg.setColorAt(0, QColor(220, 30, 40, 100)); pg.setColorAt(0, PkColor(220, 30, 40, 100));
        qg.setColorAt(1, QColor(20, 170, 230, 240)); pg.setColorAt(1, PkColor(20, 170, 230, 240));
        QBrush qb(qg); PkBrush pb(pg);
        QTransform qtTransform; qtTransform.translate(.1, 0);
        PkTransform pkTransform; pkTransform.translate(.1, 0);
        qb.setTransform(qtTransform); pb.setTransform(pkTransform);
        qt.fillRect(QRectF(2, 3, 33, 24), qb); pk.fillRect(PkRectF(2, 3, 33, 24), pb);
        QImage qi(3, 2, QImage::Format_ARGB32); PkImage pi(3, 2, PkImage::Format_ARGB32);
        for (int y = 0; y < 2; ++y) for (int x = 0; x < 3; ++x) {
            const unsigned pixel = qRgba(20 + x * 50, 70 + y * 50, 200, 100 + x * 40);
            qi.setPixel(x, y, pixel); pi.setPixel(x, y, pixel);
        }
        qt.drawImage(QRectF(4, 28, 12, 6), qi); pk.drawImage(PkRectF(4, 28, 12, 6), pi);
        qt.end();
        QSvgRenderer expected(reference), actual(QByteArray::fromStdString(backend.document()));
        QVERIFY(actual.isValid());
        QImage a(40, 36, QImage::Format_ARGB32), b(a.size(), a.format()); a.fill(0); b.fill(0);
        QPainter qa(&a), qbPainter(&b); expected.render(&qa); actual.render(&qbPainter);
        qa.end(); qbPainter.end();
        for (int y = 0; y < 36; ++y) for (int x = 0; x < 40; ++x) {
            QVERIFY2(b.pixel(x,y) == a.pixel(x,y), qPrintable(QString("radial=%1 x=%2 y=%3").arg(radial).arg(x).arg(y)));
        }
    }
}
QTEST_MAIN(PkSvgPainterBackendTest)
#include "PkSvgPainterBackendTest.moc"
