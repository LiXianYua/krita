/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_cage_transform_worker_test.h"

#include <simpletest.h>

#include <QPainter>
#include <QRandomGenerator>

#include <KoProgressUpdater.h>
#include <KoUpdater.h>

#include <testutil.h>
#include <kistest.h>

#include <kis_cage_transform_worker.h>
#include <algorithm>

void testCage(bool clockwise, bool unityTransform, bool benchmarkPrepareOnly = false, int pixelPrecision = 8, bool testQImage = false)
{
    TestUtil::TestProgressBar bar;
    KoProgressUpdater pu(&bar);
    KoUpdaterPtr updater = pu.startSubtask();

    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    const QImage sourceImage(TestUtil::fetchDataFileLazy("test_cage_transform.png"));
    const PkImage image = TestUtil::pkImageFromQImage(sourceImage);

    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->convertFromQImage(image, 0);

    KisPaintDeviceSP srcDev = new KisPaintDevice(*dev);

    PkVector<PkPointF> origPoints;
    PkVector<PkPointF> transfPoints;

    PkRectF bounds(dev->exactBounds());

    origPoints << bounds.topLeft();
    origPoints << 0.5 * (bounds.topLeft() + bounds.topRight());
    origPoints << 0.5 * (bounds.topLeft() + bounds.bottomRight());
    origPoints << 0.5 * (bounds.topRight() + bounds.bottomRight());
    origPoints << bounds.bottomRight();
    origPoints << bounds.bottomLeft();

    if (!clockwise) {
        std::reverse(origPoints.begin(), origPoints.end());
    }

    if (unityTransform) {
        transfPoints = origPoints;
    } else {
        transfPoints << bounds.topLeft();
        transfPoints << 0.5 * (bounds.topLeft() + bounds.topRight());
        transfPoints << 0.5 * (bounds.bottomLeft() + bounds.bottomRight());
        transfPoints << 0.5 * (bounds.bottomLeft() + bounds.bottomRight()) +
            (bounds.bottomLeft() - bounds.topLeft());
        transfPoints << bounds.bottomLeft() +
            (bounds.bottomLeft() - bounds.topLeft());
        transfPoints << bounds.bottomLeft();

        if (!clockwise) {
            std::reverse(transfPoints.begin(), transfPoints.end());
        }
    }

    KisCageTransformWorker worker(dev->region().boundingRect(),
                                  origPoints,
                                  updater,
                                  pixelPrecision);

    PkImage result;
    PkPointF srcImageOffset(0, 0);
    PkPointF dstImageOffset;

    QBENCHMARK_ONCE {
        if (!testQImage) {
            worker.prepareTransform();
            if (!benchmarkPrepareOnly) {
                worker.setTransformedCage(transfPoints);
                worker.run(srcDev, dev);

            }
        } else {
            KisCageTransformWorker qimageWorker(image,
                                                srcImageOffset,
                                                origPoints,
                                                updater,
                                                pixelPrecision);
            qimageWorker.prepareTransform();
            qimageWorker.setTransformedCage(transfPoints);
            result = qimageWorker.runOnImage(&dstImageOffset);
        }
    }

    QString testName = QString("%1_%2")
        .arg(clockwise ? "clk" : "cclk")
        .arg(unityTransform ? "unity" : "normal");

    if (testQImage) {
        QVERIFY(TestUtil::checkQImage(result, "cage_transform_test", "cage_qimage", testName, 1, 1));
    } else if (!benchmarkPrepareOnly && pixelPrecision == 8) {

        result = dev->convertToQImage(0);
        QVERIFY(TestUtil::checkQImage(result, "cage_transform_test", "cage", testName, 1, 1));
    }
}

void KisCageTransformWorkerTest::testCageClockwise()
{
    testCage(true, false);
}

void KisCageTransformWorkerTest::testCageClockwisePrepareOnly()
{
    testCage(true, false, true);
}

void KisCageTransformWorkerTest::testCageClockwisePixelPrecision4()
{
    testCage(true, false, false, 4);
}

void KisCageTransformWorkerTest::testCageClockwisePixelPrecision8QImage()
{
    testCage(true, false, false, 8, true);
}

void KisCageTransformWorkerTest::testCageCounterclockwise()
{
    testCage(false, false);
}

void KisCageTransformWorkerTest::testCageClockwiseUnity()
{
    testCage(true, true);
}

void KisCageTransformWorkerTest::testCageCounterclockwiseUnity()
{
    testCage(false, true);
}

void KisCageTransformWorkerTest::testRunOnImagePainterParity()
{
    QImage source(7, 7, QImage::Format_ARGB32);
    for (int y = 0; y < source.height(); ++y) {
        for (int x = 0; x < source.width(); ++x) {
            source.setPixel(x, y, qRgba(20 + 17 * x, 30 + 13 * y,
                                        40 + 5 * (x + y), 255));
        }
    }

    const auto runCase = [&source](const PkPointF &sourceOffset,
                                   const PkVector<PkPointF> &relativeCage) {
        PkVector<PkPointF> originalCage;
        for (const PkPointF &point : relativeCage) {
            originalCage << sourceOffset + point;
        }

        const PkPointF outputOffset(0.0, 0.0);
        PkImage actualPk(12, 12, PkImage::Format_ARGB32);
        actualPk.fill(0);
        PkImage transformedImage(actualPk.size(), actualPk.format());
        transformedImage.fill(0);
        transformedImage.setPixel(0, 0, qRgba(200, 10, 20, 255));
        transformedImage.setPixel(9, 9, qRgba(5, 220, 30, 255));

        KisCageTransformWorker::compositeImages(
            &actualPk, TestUtil::pkImageFromQImage(source), sourceOffset,
            outputOffset, PkPolygonF(originalCage), transformedImage);

        if (sourceOffset == PkPointF(2.0, 2.0)) {
            QVERIFY(!PkPolygonF(originalCage).containsPoint(PkPointF(3.5, 2.5),
                                                            Pk::OddEvenFill));
        }

        QImage expected(actualPk.width(), actualPk.height(), QImage::Format_ARGB32);
        expected.fill(Qt::transparent);

        QPolygonF localCage;
        for (const PkPointF &point : originalCage) {
            localCage << QPointF(point.x() - outputOffset.x(),
                                 point.y() - outputOffset.y());
        }

        {
            QPainter painter(&expected);
            painter.drawImage(QPointF(sourceOffset.x() - outputOffset.x(),
                                      sourceOffset.y() - outputOffset.y()), source);
            painter.setBrush(Qt::black);
            painter.setPen(Qt::black);
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.drawPolygon(localCage);
            painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.drawImage(QPoint(), TestUtil::diagnosticQImage(transformedImage));
        }

        QImage fillOnly(expected.size(), QImage::Format_ARGB32);
        fillOnly.fill(Qt::white);
        {
            QPainter painter(&fillOnly);
            painter.setPen(Qt::NoPen);
            painter.setBrush(Qt::black);
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.drawPolygon(localCage);
        }
        QImage penOnly(expected.size(), QImage::Format_ARGB32);
        penOnly.fill(Qt::white);
        {
            QPainter painter(&penOnly);
            painter.setBrush(Qt::NoBrush);
            painter.setPen(Qt::black);
            painter.setCompositionMode(QPainter::CompositionMode_Clear);
            painter.drawPolygon(localCage);
        }

        const QImage actual = TestUtil::diagnosticQImage(actualPk);
        for (int y = 0; y < actual.height(); ++y) {
            for (int x = 0; x < actual.width(); ++x) {
                const QString mismatch = QStringLiteral(
                    "sourceOffset=(%1,%2) pixel=(%3,%4) actual=%5 expected=%6 fill=%7 pen=%8")
                    .arg(sourceOffset.x()).arg(sourceOffset.y())
                    .arg(x).arg(y)
                    .arg(actual.pixel(x, y), 0, 16)
                    .arg(expected.pixel(x, y), 0, 16)
                    .arg(fillOnly.pixel(x, y), 0, 16)
                    .arg(penOnly.pixel(x, y), 0, 16);
                QVERIFY2(actual.pixel(x, y) == expected.pixel(x, y),
                         qPrintable(mismatch));
            }
        }
    };

    const PkVector<PkPointF> square {
        PkPointF(1.0, 1.0), PkPointF(5.0, 1.0),
        PkPointF(5.0, 5.0), PkPointF(1.0, 5.0)
    };
    const PkVector<PkPointF> slanted {
        PkPointF(1.2, 1.0), PkPointF(5.6, 1.8),
        PkPointF(4.8, 5.5), PkPointF(0.7, 4.6)
    };

    runCase(PkPointF(2.0, 2.0), square);
    runCase(PkPointF(2.25, 2.75), square);
    runCase(PkPointF(2.5, 2.5), square);
    runCase(PkPointF(2.25, 2.75), slanted);

    PkVector<PkPointF> reversedSlanted = slanted;
    std::reverse(reversedSlanted.begin(), reversedSlanted.end());
    runCase(PkPointF(-2.25, -1.75), slanted);
    runCase(PkPointF(8.5, 7.25), reversedSlanted);
}

#include <QtGlobal>


PkPointF generatePoint(const PkRectF &rc, QRandomGenerator &rng)
{
    qreal cx = rng.bounded(1.0);
    qreal cy = rng.bounded(1.0);

    PkPointF diff = rc.bottomRight() - rc.topLeft();

    PkPointF pt = rc.topLeft() + PkPointF(cx * diff.x(), cy * diff.y());
    return pt;
}

void KisCageTransformWorkerTest::stressTestRandomCages()
{
    TestUtil::TestProgressBar bar;
    KoProgressUpdater pu(&bar);
    KoUpdaterPtr updater = pu.startSubtask();

    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    QImage image(TestUtil::fetchDataFileLazy("test_cage_transform.png"));

    KisPaintDeviceSP dev = new KisPaintDevice(cs);
    dev->convertFromQImage(TestUtil::pkImageFromQImage(image), 0);

    KisPaintDeviceSP dstDev = new KisPaintDevice(cs);

    const int pixelPrecision = 8;
    PkRectF bounds(dev->exactBounds());

    QRandomGenerator rng{};

    for (int numPoints = 4; numPoints < 15; numPoints+=5) {
        for (int j = 0; j < 200; j++) {
            PkVector<PkPointF> origPoints;
            PkVector<PkPointF> transfPoints;

            dbgKrita << ppVar(j);

            for (int i = 0; i < numPoints; i++) {
                origPoints << generatePoint(bounds, rng);
                transfPoints << generatePoint(bounds, rng);
            }

            // no just hope it doesn't crash ;)
            KisCageTransformWorker worker(dev->region().boundingRect(),
                                          origPoints,
                                          updater,
                                          pixelPrecision);
            worker.prepareTransform();
            worker.setTransformedCage(transfPoints);
            worker.run(dev, dstDev);
        }
    }
}

#include "kis_green_coordinates_math.h"

void KisCageTransformWorkerTest::testUnityGreenCoordinates()
{
    PkVector<PkPointF> origPoints;
    PkVector<PkPointF> transfPoints;

    PkRectF bounds(0,0,300,300);

    origPoints << bounds.topLeft();
    origPoints << 0.5 * (bounds.topLeft() + bounds.topRight());
    origPoints << 0.5 * (bounds.topLeft() + bounds.bottomRight());
    origPoints << 0.5 * (bounds.topRight() + bounds.bottomRight());
    origPoints << bounds.bottomRight();
    origPoints << bounds.bottomLeft();

    transfPoints = origPoints;

    PkVector<PkPointF> points;
    points << PkPointF(10,10);
    points << PkPointF(140,10);
    points << PkPointF(140,140);
    points << PkPointF(10,140);

    points << PkPointF(10,160);
    points << PkPointF(140,160);
    points << PkPointF(140,290);
    points << PkPointF(10,290);

    points << PkPointF(160,160);
    points << PkPointF(290,160);
    points << PkPointF(290,290);
    points << PkPointF(160,290);

    KisGreenCoordinatesMath cage;

    cage.precalculateGreenCoordinates(origPoints, points);
    cage.generateTransformedCageNormals(transfPoints);

    PkVector<PkPointF> newPoints;

    for (int i = 0; i < points.size(); i++) {
        newPoints << cage.transformedPoint(i, transfPoints);
        QCOMPARE(points[i], newPoints.last());
    }
}

#include "kis_algebra_2d.h"

void KisCageTransformWorkerTest::testTransformAsBase()
{
    PkPointF t(1.0, 0.0);
    PkPointF b1(1.0, 0.0);
    PkPointF b2(2.0, 0.0);
    PkPointF result;


    t = PkPointF(1.0, 0.0);
    b1 = PkPointF(1.0, 0.0);
    b2 = PkPointF(2.0, 0.0);
    result = KisAlgebra2D::transformAsBase(t, b1, b2);
    QCOMPARE(result, PkPointF(2.0, 0.0));

    t = PkPointF(1.0, 0.0);
    b1 = PkPointF(1.0, 0.0);
    b2 = PkPointF(0.0, 1.0);
    result = KisAlgebra2D::transformAsBase(t, b1, b2);
    QCOMPARE(result, PkPointF(0.0, 1.0));

    t = PkPointF(1.0, 0.0);
    b1 = PkPointF(1.0, 0.0);
    b2 = PkPointF(0.0, 2.0);
    result = KisAlgebra2D::transformAsBase(t, b1, b2);
    QCOMPARE(result, PkPointF(0.0, 2.0));

    t = PkPointF(0.0, 1.0);
    b1 = PkPointF(1.0, 0.0);
    b2 = PkPointF(2.0, 0.0);
    result = KisAlgebra2D::transformAsBase(t, b1, b2);
    QCOMPARE(result, PkPointF(0.0, 2.0));

    t = PkPointF(0.0, 1.0);
    b1 = PkPointF(1.0, 0.0);
    b2 = PkPointF(0.0, 1.0);
    result = KisAlgebra2D::transformAsBase(t, b1, b2);
    QCOMPARE(result, PkPointF(-1.0, 0.0));

    t = PkPointF(0.0, 1.0);
    b1 = PkPointF(1.0, 0.0);
    b2 = PkPointF(0.0, 2.0);
    result = KisAlgebra2D::transformAsBase(t, b1, b2);
    QCOMPARE(result, PkPointF(-2.0, 0.0));
}

void KisCageTransformWorkerTest::testAngleBetweenVectors()
{
    PkPointF b1(1.0, 0.0);
    PkPointF b2(2.0, 0.0);
    qreal result;

    b1 = PkPointF(1.0, 0.0);
    b2 = PkPointF(0.0, 1.0);
    result = KisAlgebra2D::angleBetweenVectors(b1, b2);
    QCOMPARE(result, M_PI_2);

    b1 = PkPointF(1.0, 0.0);
    b2 = PkPointF(std::sqrt(0.5), std::sqrt(0.5));
    result = KisAlgebra2D::angleBetweenVectors(b1, b2);
    QCOMPARE(result, M_PI / 4);

    PkTransform t;
    t.rotateRadians(M_PI / 4);
    QCOMPARE(t.map(b1), b2);
}

KISTEST_MAIN(KisCageTransformWorkerTest)
