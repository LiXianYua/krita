/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt boud @valdyas.org
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_gradient_painter_test.h"

#include <simpletest.h>
#include "kis_gradient_painter.h"

#include "kis_paint_device.h"
#include "kis_selection.h"

#include <KoColor.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoCompositeOpRegistry.h>

#include <resources/KoStopGradient.h>

#include "krita_utils.h"
#include <kis_algebra_2d.h>
#include <testutil.h>


void KisGradientPainterTest::testSimplifyPath()
{
    PkPolygonF selectionPolygon;
    selectionPolygon.append(PkPointF(100, 100));
    selectionPolygon.append(PkPointF(200, 100)); selectionPolygon.append(PkPointF(202, 100));
    selectionPolygon.append(PkPointF(200, 200)); selectionPolygon.append(PkPointF(100, 200));
    selectionPolygon.append(PkPointF(100, 102));

    PkPainterPath path;
    path.addPolygon(selectionPolygon);

    PkPainterPath simplifiedPath = KisAlgebra2D::trySimplifyPath(path, 10.0);

    PkPainterPath ref;
    ref.moveTo(100,100);
    ref.lineTo(200,100);
    ref.lineTo(200,200);
    ref.lineTo(100,200);

    QCOMPARE(simplifiedPath, ref);
}

void testShapedGradientPainterImpl(const PkPolygonF &selectionPolygon,
                                   const QString &testName,
                                   const PkPolygonF &selectionErasePolygon = PkPolygonF())
{
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dev = new KisPaintDevice(cs);

    PkRect imageRect(0,0,300,300);

    KisSelectionSP selection = new KisSelection();
    KisPixelSelectionSP pixelSelection = selection->pixelSelection();

    KisPainter selPainter(pixelSelection);
    selPainter.setFillStyle(KisPainter::FillStyleForegroundColor);
    selPainter.setPaintColor(KoColor(Qt::white, pixelSelection->colorSpace()));
    selPainter.paintPolygon(selectionPolygon);

    if (!selectionErasePolygon.isEmpty()) {
        selPainter.setCompositeOpId(COMPOSITE_ERASE);
        selPainter.setPaintColor(KoColor(Qt::white, pixelSelection->colorSpace()));
        selPainter.paintPolygon(selectionErasePolygon);
    }

    selPainter.end();

    pixelSelection->invalidateOutlineCache();

    (void)pixelSelection->convertToQImage(0, imageRect);

    PkGradient testGradient(PkGradientEnums::LinearGradient);
    testGradient.setColorAt(0.0, PkColor(Qt::white)); testGradient.setColorAt(0.5, PkColor(Qt::green));
    testGradient.setColorAt(1.0, PkColor(Qt::black)); testGradient.setSpread(PkGradientEnums::ReflectSpread);
    PkSharedPointer<KoStopGradient> gradient(KoStopGradient::fromQGradient(&testGradient));

    KisGradientPainter gc(dev, selection);
    gc.setGradient(gradient);
    gc.setGradientShape(KisGradientPainter::GradientShapePolygonal);

    gc.paintGradient(selectionPolygon.boundingRect().topLeft(),
                     selectionPolygon.boundingRect().bottomRight(),
                     KisGradientPainter::GradientRepeatNone,
                     0,
                     false,
                     imageRect.x(),
                     imageRect.y(),
                     imageRect.width(),
                     imageRect.height());

    QVERIFY(TestUtil::checkQImageExternal(dev->convertToQImage(0, imageRect),
                                          "shaped_gradient",
                                          "fill",
                                          testName, 1, 1, 0));
}

void KisGradientPainterTest::testShapedGradientPainterRect()
{
    PkPolygonF selectionPolygon;

    selectionPolygon.append(PkPointF(100, 100));
    selectionPolygon.append(PkPointF(200, 100)); selectionPolygon.append(PkPointF(202, 100));
    selectionPolygon.append(PkPointF(200, 200)); selectionPolygon.append(PkPointF(100, 200));

    testShapedGradientPainterImpl(selectionPolygon, "rect_shape");
}

void KisGradientPainterTest::testShapedGradientPainterRectPierced()
{
    PkPolygonF selectionPolygon;

    selectionPolygon.append(PkPointF(100, 100)); selectionPolygon.append(PkPointF(200, 100));
    selectionPolygon.append(PkPointF(200, 200)); selectionPolygon.append(PkPointF(100, 200));

    PkPolygonF selectionErasePolygon;
    selectionErasePolygon.append(PkPointF(150, 150)); selectionErasePolygon.append(PkPointF(155, 150));
    selectionErasePolygon.append(PkPointF(155, 155)); selectionErasePolygon.append(PkPointF(150, 155));

    testShapedGradientPainterImpl(selectionPolygon, "rect_shape_pierced", selectionErasePolygon);
}

void KisGradientPainterTest::testShapedGradientPainterNonRegular()
{
    PkPolygonF selectionPolygon;
    selectionPolygon << PkPointF(100, 100);
    selectionPolygon << PkPointF(200, 120);
    selectionPolygon << PkPointF(170, 140);
    selectionPolygon << PkPointF(200, 180);
    selectionPolygon << PkPointF(30, 220);

    testShapedGradientPainterImpl(selectionPolygon, "nonregular_shape");
}

void KisGradientPainterTest::testShapedGradientPainterNonRegularPierced()
{
    PkPolygonF selectionPolygon;
    selectionPolygon << PkPointF(100, 100);
    selectionPolygon << PkPointF(200, 120);
    selectionPolygon << PkPointF(170, 140);
    selectionPolygon << PkPointF(200, 180);
    selectionPolygon << PkPointF(30, 220);

    PkPolygonF selectionErasePolygon;
    selectionErasePolygon << PkPointF(150, 150);
    selectionErasePolygon << PkPointF(155, 150);
    selectionErasePolygon << PkPointF(155, 155);
    selectionErasePolygon << PkPointF(150, 155);

    testShapedGradientPainterImpl(selectionPolygon, "nonregular_shape_pierced", selectionErasePolygon);
}

#include "kis_polygonal_gradient_shape_strategy.h"

void KisGradientPainterTest::testFindShapedExtremums()
{
    PkPolygonF selectionPolygon;
    selectionPolygon << PkPointF(100, 100);
    selectionPolygon << PkPointF(200, 120);
    selectionPolygon << PkPointF(170, 140);
    selectionPolygon << PkPointF(200, 180);
    selectionPolygon << PkPointF(30, 220);

    PkPolygonF selectionErasePolygon;
    selectionErasePolygon << PkPointF(101, 101);
    selectionErasePolygon << PkPointF(190, 120);
    selectionErasePolygon << PkPointF(160, 140);
    selectionErasePolygon << PkPointF(200, 180);
    selectionErasePolygon << PkPointF(30, 220);

    PkPainterPath path;
    path.addPolygon(selectionPolygon);
    path.closeSubpath();
    path.addPolygon(selectionErasePolygon);
    path.closeSubpath();

    PkPointF center =
        KisPolygonalGradientShapeStrategy::testingCalculatePathCenter(
            4, path, 2.0, true);

    dbgKrita << ppVar(center);

    QVERIFY(path.contains(center));
}

void KisGradientPainterTest::testSplitDisjointPaths()
{
    PkPainterPath path;

    // small bug: the smaller rect is also merged
    path.addRect(PkRectF(323, 123, 4, 4));
    path.addRect(PkRectF(300, 100, 50, 50));
    path.addRect(PkRectF(320, 120, 10, 10));

    path.addRect(PkRectF(200, 100, 50, 50));
    path.addRect(PkRectF(240, 120, 70, 10));

    path.addRect(PkRectF(100, 100, 50, 50));
    path.addRect(PkRectF(120, 120, 10, 10));

    path = path.simplified();

    const PkList<PkPainterPath> result = KritaUtils::splitDisjointPaths(path);
    Q_UNUSED(result);
}

#include "kis_cached_gradient_shape_strategy.h"

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/variance.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>

using namespace boost::accumulators;

void KisGradientPainterTest::testCachedStrategy()
{
    PkPolygonF selectionPolygon;
    selectionPolygon << PkPointF(100, 100);
    selectionPolygon << PkPointF(200, 120);
    selectionPolygon << PkPointF(170, 140);
    selectionPolygon << PkPointF(200, 180);
    selectionPolygon << PkPointF(30, 220);

    PkPainterPath selectionPath;
    selectionPath.addPolygon(selectionPolygon);

    PkRect rc = selectionPolygon.boundingRect().toAlignedRect();

    KisGradientShapeStrategy *strategy =
        new KisPolygonalGradientShapeStrategy(selectionPath, 2.0);

    KisCachedGradientShapeStrategy cached(rc, 4, 4, strategy);

    accumulator_set<qreal, stats<tag::variance, tag::max, tag::min> > accum;
    const qreal maxRelError = 5.0 / 256;


    for (int y = rc.y(); y <= rc.bottom(); y++) {
        for (int x = rc.x(); x <= rc.right(); x++) {
            if (!selectionPolygon.containsPoint(PkPointF(x, y), Qt::OddEvenFill)) continue;

            qreal ref = strategy->valueAt(x, y);
            qreal value = cached.valueAt(x, y);

            if (ref == 0.0) continue;

            qreal relError = (ref - value)/* / ref*/;
            accum(relError);

            if (relError > maxRelError) {
                //dbgKrita << ppVar(x) << ppVar(y) << ppVar(value) << ppVar(ref) << ppVar(relError);
            }
        }
    }

    dbgKrita << ppVar(count(accum));
    dbgKrita << ppVar(mean(accum));
    dbgKrita << ppVar(variance(accum));
    dbgKrita << ppVar((min)(accum));
    dbgKrita << ppVar((max)(accum));

    qreal varError = variance(accum);
    QVERIFY(varError < maxRelError);

    qreal maxError = qMax(qAbs((min)(accum)), qAbs((max)(accum)));
    QVERIFY(maxError < 2 * maxRelError);
}

SIMPLE_TEST_MAIN(KisGradientPainterTest)
