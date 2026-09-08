/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_warp_transform_worker_test.h"

#include <simpletest.h>
#include <testutil.h>

#include "kis_warptransform_worker.h"

#include <KoProgressUpdater.h>

struct WarpTransformWorkerData {

    WarpTransformWorkerData() {
        TestUtil::TestProgressBar bar;
        KoProgressUpdater pu(&bar);
        updater = pu.startSubtask();

        const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
        // QImage image(TestUtil::fetchDataFileLazy("test_transform_quality.png"));
        const PkImage image = TestUtil::pkImageFromQImage(
            QImage(TestUtil::fetchDataFileLazy("test_transform_quality_second.png")));

        dev = new KisPaintDevice(cs);
        dev->convertFromQImage(image, 0);

        alpha = 1.0;

        bounds = PkRectF(dev->exactBounds());

        origPoints << bounds.topLeft();
        origPoints << bounds.topRight();
        origPoints << bounds.bottomRight();
        origPoints << bounds.bottomLeft();

        origPoints << 0.5 * (bounds.bottomLeft() + bounds.bottomRight());
        origPoints << 0.5 * (bounds.bottomLeft() + bounds.bottomRight()) + PkPointF(-20, 0);


        transfPoints << bounds.topLeft();
        transfPoints << bounds.bottomLeft() + 0.6 * (bounds.topRight() - bounds.bottomLeft());
        transfPoints << bounds.topLeft() + 0.8 * (bounds.bottomRight() - bounds.topLeft());
        transfPoints << bounds.bottomLeft() + PkPointF(200, 0);

        transfPoints << 0.5 * (bounds.bottomLeft() + bounds.bottomRight()) + PkPointF(40,20);
        transfPoints << 0.5 * (bounds.bottomLeft() + bounds.bottomRight()) + PkPointF(-20, 0) + PkPointF(-40,20);
    }


    KisPaintDeviceSP dev;
    PkVector<PkPointF> origPoints;
    PkVector<PkPointF> transfPoints;
    qreal alpha;
    KoUpdaterPtr updater;
    PkRectF bounds;
};


void KisWarpTransformWorkerTest::test()
{
    WarpTransformWorkerData d;
    KisPaintDeviceSP srcDev = new KisPaintDevice(*d.dev);

    KisWarpTransformWorker worker(KisWarpTransformWorker::RIGID_TRANSFORM,
                                  d.origPoints,
                                  d.transfPoints,
                                  d.alpha,
                                  d.updater);

    QBENCHMARK_ONCE {
        worker.run(srcDev, d.dev);
    }

    PkImage result = d.dev->convertToQImage(0);

    TestUtil::checkQImage(result, "warp_transform_test", "simple", "tr");
}

void KisWarpTransformWorkerTest::testQImage()
{
    TestUtil::TestProgressBar bar;
    KoProgressUpdater pu(&bar);
    KoUpdaterPtr updater = pu.startSubtask();

//    QImage image(TestUtil::fetchDataFileLazy("test_transform_quality.png"));
    PkImage image = TestUtil::pkImageFromQImage(
        QImage(TestUtil::fetchDataFileLazy("test_transform_quality_second.png")));
    image.convertTo(PkImage::Format_ARGB32);


    PkVector<PkPointF> origPoints;
    PkVector<PkPointF> transfPoints;
    qreal alpha = 1.0;

    PkRectF bounds(image.rect());

    origPoints << bounds.topLeft();
    origPoints << bounds.topRight();
    origPoints << bounds.bottomRight();
    origPoints << bounds.bottomLeft();

    origPoints << 0.5 * (bounds.bottomLeft() + bounds.bottomRight());
    origPoints << 0.5 * (bounds.bottomLeft() + bounds.bottomRight()) + PkPointF(-20, 0);


    transfPoints << bounds.topLeft();
    transfPoints << bounds.bottomLeft() + 0.6 * (bounds.topRight() - bounds.bottomLeft());
    transfPoints << bounds.topLeft() + 0.8 * (bounds.bottomRight() - bounds.topLeft());
    transfPoints << bounds.bottomLeft() + PkPointF(200, 0);

    transfPoints << 0.5 * (bounds.bottomLeft() + bounds.bottomRight()) + PkPointF(40,20);
    transfPoints << 0.5 * (bounds.bottomLeft() + bounds.bottomRight()) + PkPointF(-20, 0) + PkPointF(-40,20);


    PkImage result;
    PkPointF newOffset;

    QBENCHMARK_ONCE {
        result = KisWarpTransformWorker::transformImage(
            KisWarpTransformWorker::RIGID_TRANSFORM,
            origPoints, transfPoints, alpha,
            image, PkPointF(), &newOffset);
    }

    TestUtil::checkQImage(result, "warp_transform_test", "qimage", "tr");
}

#include "kis_grid_interpolation_tools.h"

void KisWarpTransformWorkerTest::testGridSize()
{
    QCOMPARE(GridIterationTools::calcGridDimension(1, 7, 4), 3);
    QCOMPARE(GridIterationTools::calcGridDimension(1, 8, 4), 3);
    QCOMPARE(GridIterationTools::calcGridDimension(1, 9, 4), 4);
    QCOMPARE(GridIterationTools::calcGridDimension(0, 7, 4), 3);
    QCOMPARE(GridIterationTools::calcGridDimension(1, 8, 4), 3);
    QCOMPARE(GridIterationTools::calcGridDimension(4, 9, 4), 3);
    QCOMPARE(GridIterationTools::calcGridDimension(0, 9, 4), 4);
    QCOMPARE(GridIterationTools::calcGridDimension(-1, 9, 4), 5);

    QCOMPARE(GridIterationTools::calcGridDimension(0, 300, 8), 39);
}

void KisWarpTransformWorkerTest::testBackwardInterpolatorExtrapolation()
{
    PkPolygonF src;

    src << PkPointF(0, 0);
    src << PkPointF(100, 0);
    src << PkPointF(100, 100);
    src << PkPointF(0, 100);

    PkPolygonF dst(src);
    std::rotate(dst.begin(), dst.begin() + 1, dst.end());
    KisFourPointInterpolatorBackward interp(src, dst);

    // standard checks
    QCOMPARE(interp.map(PkPointF(0,0)), PkPointF(0,100));
    QCOMPARE(interp.map(PkPointF(100,0)), PkPointF(0,0));
    QCOMPARE(interp.map(PkPointF(100,100)), PkPointF(100,0));
    QCOMPARE(interp.map(PkPointF(0,100)), PkPointF(100,100));

    // extrapolate!
    QCOMPARE(interp.map(PkPointF(-10,0)), PkPointF(0,110));
    QCOMPARE(interp.map(PkPointF(0,-10)), PkPointF(-10,100));
    QCOMPARE(interp.map(PkPointF(-10,-10)), PkPointF(-10,110));

    QCOMPARE(interp.map(PkPointF(110,0)), PkPointF(0,-10));
    QCOMPARE(interp.map(PkPointF(100,-10)), PkPointF(-10,0));
    QCOMPARE(interp.map(PkPointF(110,-10)), PkPointF(-10,-10));

    QCOMPARE(interp.map(PkPointF(110,100)), PkPointF(100, -10));
    QCOMPARE(interp.map(PkPointF(100,110)), PkPointF(110, 0));
    QCOMPARE(interp.map(PkPointF(110,110)), PkPointF(110,-10));

    QCOMPARE(interp.map(PkPointF(-10,100)), PkPointF(100, 110));
    QCOMPARE(interp.map(PkPointF(0,110)), PkPointF(110, 100));
    QCOMPARE(interp.map(PkPointF(-10,110)), PkPointF(110,110));
}
#include "krita_utils.h"
void KisWarpTransformWorkerTest::testNeedChangeRects()
{
    WarpTransformWorkerData d;
    KisWarpTransformWorker worker(KisWarpTransformWorker::RIGID_TRANSFORM,
                                  d.origPoints,
                                  d.transfPoints,
                                  d.alpha,
                                  d.updater);

    QCOMPARE(KisAlgebra2D::sampleRectWithPoints(d.bounds.toAlignedRect()).size(), 9);
    QCOMPARE(worker.approxChangeRect(d.bounds.toAlignedRect()), PkRect(-44,-44, 982,986));
}


SIMPLE_TEST_MAIN(KisWarpTransformWorkerTest)
