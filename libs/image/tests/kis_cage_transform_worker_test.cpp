/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_cage_transform_worker_test.h"

#include <simpletest.h>

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
