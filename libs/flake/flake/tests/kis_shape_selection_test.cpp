/*
 *  SPDX-FileCopyrightText: 2008 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_shape_selection_test.h"
#include <simpletest.h>

#include <kis_debug.h>
#include <PkRect.h>

#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoPathShape.h>

#include "KisImageResolutionProxy.h"
#include "kis_default_bounds.h"
#include "kis_selection.h"
#include "kis_pixel_selection.h"
#include "flake/kis_shape_selection.h"
#include "kis_image.h"
#include <testutil.h>
#include <KisDocument.h>
#include "kis_transaction.h"
#include "kis_default_bounds_base.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>

class TestKisDocument : public KisDocument
{
public:
    TestKisDocument() : KisDocument() {}
};

void KisShapeSelectionTest::testAddChild()
{
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
    PkScopedPointer<KisDocument> doc(new TestKisDocument);
    PkColor qc(Pk::white);
    qc.setAlpha(0);
    KoColor bgColor(qc, cs);
    doc->newImage("test", 300, 300, cs, bgColor, KisDocument::NewImageBackgroundStyle::CanvasColor, 1, "test", 100);
    KisImageSP image = doc->image();

    KisDefaultBoundsSP bounds(new KisDefaultBounds(image));
    KisImageResolutionProxySP resolutionProxy(new KisImageResolutionProxy(image));

    KisSelectionSP selection = new KisSelection(bounds, resolutionProxy);
    QVERIFY(!selection->hasNonEmptyPixelSelection());
    QVERIFY(!selection->hasNonEmptyShapeSelection());

    KisPixelSelectionSP pixelSelection = selection->pixelSelection();
    pixelSelection->select(PkRect(0, 0, 100, 100));

    QCOMPARE(TestUtil::alphaDevicePixel(pixelSelection, 25, 25), MAX_SELECTED);
    QCOMPARE(selection->selectedExactRect(), PkRect(0, 0, 100, 100));

    PkRectF rect(50, 50, 100, 100);
    PkTransform matrix;
    matrix.scale(1 / image->xRes(), 1 / image->yRes());
    rect = matrix.mapRect(rect);

    KoPathShape* shape = new KoPathShape();
    shape->setShapeId(KoPathShapeId);
    shape->moveTo(rect.topLeft());
    shape->lineTo(rect.topRight());
    shape->lineTo(rect.bottomRight());
    shape->lineTo(rect.bottomLeft());
    shape->close();

    KisShapeSelection * shapeSelection = new KisShapeSelection(doc->shapeController(), selection);
    selection->convertToVectorSelectionNoUndo(shapeSelection);
    shapeSelection->addShape(shape);

    QVERIFY(selection->hasNonEmptyShapeSelection());

    selection->updateProjection();
    image->waitForDone();

    QCOMPARE(selection->selectedExactRect(), PkRect(50, 50, 100, 100));
}

KoPathShape *createRectangularShape(const PkRectF &rect)
{
    KoPathShape* shape = new KoPathShape();
    shape->setShapeId(KoPathShapeId);
    shape->moveTo(rect.topLeft());
    shape->lineTo(rect.topRight());
    shape->lineTo(rect.bottomRight());
    shape->lineTo(rect.bottomLeft());
    shape->close();

    return shape;
}

void KisShapeSelectionTest::testUndoFlattening()
{
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
    PkScopedPointer<KisDocument> doc(new TestKisDocument);
    KoColor bgColor(PkColor(255, 255, 255, 0), cs);
    doc->newImage("test", 300, 300, cs, bgColor, KisDocument::NewImageBackgroundStyle::CanvasColor, 1, "test", 100);
    KisImageSP image = doc->image();

    QCOMPARE(image->locked(), false);

    KisDefaultBoundsSP bounds(new KisDefaultBounds(image));
    KisImageResolutionProxySP resolutionProxy(new KisImageResolutionProxy(image));

    KisSelectionSP selection = new KisSelection(bounds, resolutionProxy);
    QCOMPARE(selection->hasNonEmptyPixelSelection(), false);
    QCOMPARE(selection->hasNonEmptyShapeSelection(), false);

    selection->setParentNode(image->root());

    KisPixelSelectionSP pixelSelection = selection->pixelSelection();
    pixelSelection->select(PkRect(0, 0, 100, 100));

    QCOMPARE(TestUtil::alphaDevicePixel(pixelSelection, 25, 25), MAX_SELECTED);
    QCOMPARE(selection->selectedExactRect(), PkRect(0, 0, 100, 100));

    PkTransform matrix;
    matrix.scale(1 / image->xRes(), 1 / image->yRes());
    const PkRectF srcRect1(50, 50, 100, 100);
    const PkRectF rect1 = matrix.mapRect(srcRect1);

    KisShapeSelection * shapeSelection1 = new KisShapeSelection(doc->shapeController(), selection);
    selection->convertToVectorSelectionNoUndo(shapeSelection1);

    KoPathShape *shape1 = createRectangularShape(rect1);
    shapeSelection1->addShape(shape1);

    QVERIFY(selection->hasNonEmptyShapeSelection());

    selection->pixelSelection()->clear();
    QCOMPARE(selection->selectedExactRect(), PkRect());

    selection->updateProjection();
    image->waitForDone();

    QCOMPARE(selection->selectedExactRect(), srcRect1.toRect());
    QCOMPARE(selection->outlineCacheValid(), true);
    QCOMPARE(selection->outlineCache().boundingRect(), srcRect1);
    QCOMPARE(selection->hasNonEmptyShapeSelection(), true);

    KisSelectionTransaction t1(selection->pixelSelection());
    selection->pixelSelection()->clear();
    KUndo2Command *cmd1 = t1.endAndTake();
    cmd1->redo(); // first redo

    QTest::qWait(400);
    image->waitForDone();

    QCOMPARE(selection->selectedExactRect(), PkRect());
    QCOMPARE(selection->outlineCacheValid(), true);
    QCOMPARE(selection->outlineCache().boundingRect(), PkRectF());
    QCOMPARE(selection->hasNonEmptyShapeSelection(), false);

    const PkRectF srcRect2(10, 10, 20, 20);
    const PkRectF rect2 = matrix.mapRect(srcRect2);
    KoPathShape *shape2 = createRectangularShape(rect2);

    KisShapeSelection * shapeSelection2 = new KisShapeSelection(doc->shapeController(), selection);
    KUndo2Command *cmd2 = selection->convertToVectorSelection(shapeSelection2);
    cmd2->redo(); // first redo

    shapeSelection2->addShape(shape2);

    QTest::qWait(400);
    image->waitForDone();

    QCOMPARE(selection->selectedExactRect(), srcRect2.toRect());
    QCOMPARE(selection->outlineCacheValid(), true);
    QCOMPARE(selection->outlineCache().boundingRect(), srcRect2);
    QCOMPARE(selection->hasNonEmptyShapeSelection(), true);

    shapeSelection1->removeShape(shape2);

    QTest::qWait(400);
    image->waitForDone();

    QCOMPARE(selection->selectedExactRect(), PkRect());
    QCOMPARE(selection->outlineCacheValid(), true);
    QCOMPARE(selection->outlineCache().boundingRect(), PkRectF());
    QCOMPARE(selection->hasNonEmptyShapeSelection(), false);

    cmd2->undo();
    cmd1->undo();

    QTest::qWait(400);
    image->waitForDone();

    QCOMPARE(selection->selectedExactRect(), srcRect1.toRect());
    QCOMPARE(selection->outlineCacheValid(), true);
    QCOMPARE(selection->outlineCache().boundingRect(), srcRect1);
    QCOMPARE(selection->hasNonEmptyShapeSelection(), true);

    delete cmd2;
    delete cmd1;
    QTest::qWait(400);

}

#include "kis_paint_device_debug_utils.h"

void KisShapeSelectionTest::testHistoryOnFlattening()
{
    const KoColorSpace * cs = KoColorSpaceRegistry::instance()->rgb8();
    PkScopedPointer<KisDocument> doc(new TestKisDocument);
    KoColor bgColor(PkColor(255, 255, 255, 0), cs);
    doc->newImage("test", 300, 300, cs, bgColor, KisDocument::NewImageBackgroundStyle::CanvasColor, 1, "test", 100);
    KisImageSP image = doc->image();

    QCOMPARE(image->locked(), false);

    KisDefaultBoundsSP bounds(new KisDefaultBounds(image));
    KisImageResolutionProxySP resolutionProxy(new KisImageResolutionProxy(image));

    KisSelectionSP selection = new KisSelection(bounds, resolutionProxy);
    QCOMPARE(selection->hasNonEmptyPixelSelection(), false);
    QCOMPARE(selection->hasNonEmptyShapeSelection(), false);

    selection->setParentNode(image->root());

    KisPixelSelectionSP pixelSelection = selection->pixelSelection();

    KisSelectionTransaction t0(pixelSelection);
    pixelSelection->select(PkRect(70, 70, 180, 20));
    PkScopedPointer<KUndo2Command> cmd0(t0.endAndTake());
    cmd0->redo(); // first redo

    KisSelectionTransaction t1(pixelSelection);
    pixelSelection->clear();
    pixelSelection->select(PkRect(0, 0, 100, 100));
    PkScopedPointer<KUndo2Command> cmd1(t1.endAndTake());
    cmd1->redo(); // first redo

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "00_0pixel", "dd");
    QCOMPARE(TestUtil::alphaDevicePixel(pixelSelection, 25, 25), MAX_SELECTED);
    QCOMPARE(selection->selectedExactRect(), PkRect(0, 0, 100, 100));

    PkTransform matrix;
    matrix.scale(1 / image->xRes(), 1 / image->yRes());
    const PkRectF srcRect1(50, 50, 100, 100);
    const PkRectF rect1 = matrix.mapRect(srcRect1);

    KisShapeSelection * shapeSelection = new KisShapeSelection(doc->shapeController(), selection);

    PkScopedPointer<KUndo2Command> cmd2(selection->convertToVectorSelection(shapeSelection));
    cmd2->redo();

    QVERIFY(!selection->hasNonEmptyShapeSelection());
    QTest::qWait(200);
    image->waitForDone();

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "00_1converted", "dd");
    QCOMPARE(selection->selectedExactRect(), PkRect());

    KoPathShape *shape1 = createRectangularShape(rect1);
    shapeSelection->addShape(shape1);

    QVERIFY(selection->hasNonEmptyShapeSelection());
    QTest::qWait(200);
    image->waitForDone();

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "01_vector", "dd");
    QCOMPARE(selection->selectedExactRect(), PkRect(50, 50, 100, 100));

    KisSelectionTransaction flatteningTransaction(pixelSelection);
    pixelSelection->select(PkRect(80, 80, 100, 83));

    PkScopedPointer<KUndo2Command> cmd3(flatteningTransaction.endAndTake());
    cmd3->redo(); // first redo!

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "02_flattened", "dd");
    QCOMPARE(selection->selectedExactRect(), PkRect(50, 50, 130, 113));
    QVERIFY(!selection->hasNonEmptyShapeSelection());

    cmd3->undo();
    QTest::qWait(200);
    image->waitForDone();

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "03_undo_flattening", "dd");
    QCOMPARE(selection->selectedExactRect(), PkRect(50, 50, 100, 100));
    QVERIFY(selection->hasNonEmptyShapeSelection());

    cmd3->redo();
    QTest::qWait(200);
    image->waitForDone();

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "04_redo_flattening", "dd");
    QCOMPARE(selection->selectedExactRect(), PkRect(50, 50, 130, 113));
    QVERIFY(!selection->hasNonEmptyShapeSelection());

    cmd3->undo();
    QTest::qWait(200);
    image->waitForDone();

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "05_2ndundo_flattening", "dd");
    QCOMPARE(selection->selectedExactRect(), PkRect(50, 50, 100, 100));
    QVERIFY(selection->hasNonEmptyShapeSelection());

    shapeSelection->removeShape(shape1);

    QTest::qWait(200);
    image->waitForDone();

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "06_undo_add_shape", "dd");
    QVERIFY(selection->shapeSelection());
    QVERIFY(!selection->hasNonEmptyShapeSelection());
    QCOMPARE(selection->selectedExactRect(), PkRect());

    cmd2->undo();

    QTest::qWait(200);
    image->waitForDone();

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "07_undo_conversion", "dd");
    QVERIFY(!selection->shapeSelection());
    QVERIFY(!selection->hasNonEmptyShapeSelection());
    QCOMPARE(selection->selectedExactRect(), PkRect(0, 0, 100, 100));

    cmd1->undo();

    QTest::qWait(200);
    image->waitForDone();

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "08_undo_initial_paint", "dd");
    QVERIFY(!selection->shapeSelection());
    QVERIFY(!selection->hasNonEmptyShapeSelection());
    QCOMPARE(selection->selectedExactRect(), PkRect(70, 70, 180, 20));

    cmd0->undo();

    QTest::qWait(200);
    image->waitForDone();

    // KIS_DUMP_DEVICE_2(pixelSelection, PkRect(0,0,300,300), "09_undo_zero_paint", "dd");
    QVERIFY(!selection->shapeSelection());
    QVERIFY(!selection->hasNonEmptyShapeSelection());
    QCOMPARE(selection->selectedExactRect(), PkRect());
}

namespace {

class FixedLodBounds final : public KisDefaultBoundsBase
{
public:
    FixedLodBounds(const PkRect &bounds, int lod)
        : m_bounds(bounds)
        , m_lod(lod)
    {
    }

    PkRect bounds() const override { return m_bounds; }
    PkRect imageBorderRect() const override { return m_bounds; }
    bool wrapAroundMode() const override { return false; }
    WrapAroundAxis wrapAroundModeAxis() const override { return WRAPAROUND_BOTH; }
    int currentLevelOfDetail() const override { return m_lod; }
    int currentTime() const override { return 0; }
    bool externalFrameActive() const override { return false; }
    void *sourceCookie() const override { return nullptr; }

private:
    PkRect m_bounds;
    int m_lod;
};

}

void KisShapeSelectionTest::testRenderMatchesQtTiledLargePath()
{
    const KoColorSpace *rgb8 = KoColorSpaceRegistry::instance()->rgb8();
    const KoColorSpace *alpha8 = KoColorSpaceRegistry::instance()->alpha8();
    PkScopedPointer<KisDocument> doc(new TestKisDocument);
    const KoColor background(PkColor(255, 255, 255, 0), rgb8);
    doc->newImage("shape-selection-oracle", 512, 256, rgb8, background,
                  KisDocument::NewImageBackgroundStyle::CanvasColor, 1,
                  "shape-selection-oracle", 100);
    KisImageSP image = doc->image();
    KisDefaultBoundsSP selectionBounds(new KisDefaultBounds(image));
    KisImageResolutionProxySP resolutionProxy(new KisImageResolutionProxy(image));
    KisSelectionSP selection = new KisSelection(selectionBounds, resolutionProxy);
    PkScopedPointer<KisShapeSelection> shapeSelection(
        new KisShapeSelection(doc->shapeController(), selection));

    const qreal invXRes = 1.0 / image->xRes();
    const qreal invYRes = 1.0 / image->yRes();
    PkTransform fixtureTransform;
    fixtureTransform.translate(2.25, 1.5);
    fixtureTransform.rotate(13);
    const auto documentPoint = [&](qreal x, qreal y) {
        const PkPointF point = fixtureTransform.map(PkPointF(x, y));
        return PkPointF(point.x() * invXRes, point.y() * invYRes);
    };
    const auto rawDocumentPoint = [=](qreal x, qreal y) {
        return PkPointF(x * invXRes, y * invYRes);
    };

    KoPathShape *shape = new KoPathShape;
    shape->setShapeId(KoPathShapeId);
    shape->moveTo(documentPoint(2, 2));
    shape->curveTo(documentPoint(30, -2), documentPoint(-2, 28),
                   documentPoint(25, 18));
    shape->lineTo(documentPoint(3, 20));
    shape->close();
    shape->moveTo(rawDocumentPoint(0, 80));
    shape->lineTo(rawDocumentPoint(300, 80));
    shape->lineTo(rawDocumentPoint(300, 110));
    shape->lineTo(rawDocumentPoint(0, 110));
    shape->close();
    shape->moveTo(rawDocumentPoint(80000, 20));
    shape->lineTo(rawDocumentPoint(80020, 20));
    shape->lineTo(rawDocumentPoint(80020, 40));
    shape->lineTo(rawDocumentPoint(80000, 40));
    shape->close();
    shapeSelection->addShape(shape);
    shapeSelection->recalculateOutlineCache();

    QPainterPath sourceOutline;
    sourceOutline.moveTo(2, 2);
    sourceOutline.cubicTo(30, -2, -2, 28, 25, 18);
    sourceOutline.lineTo(3, 20);
    sourceOutline.closeSubpath();
    QTransform qtFixtureTransform;
    qtFixtureTransform.translate(2.25, 1.5);
    qtFixtureTransform.rotate(13);
    QPainterPath qtOutline = qtFixtureTransform.map(sourceOutline);
    qtOutline.addRect(QRectF(0, 80, 300, 30));
    qtOutline.addRect(QRectF(80000, 20, 20, 20));

    for (int lod : {0, 1}) {
        const qreal scale = 1.0 / (1 << lod);
        const PkRect requestedRect = lod == 0
            ? PkRect(0, 0, 300, 140)
            : PkRect(0, 0, 150, 70);
        const QPainterPath expectedOutline = QTransform::fromScale(scale, scale).map(qtOutline);

        for (quint8 defaultValue : {quint8(0), quint8(91)}) {
            KisPaintDeviceSP projection = new KisPaintDevice(alpha8);
            projection->setDefaultBounds(new FixedLodBounds(requestedRect, lod));
            projection->setDefaultPixel(KoColor(&defaultValue, alpha8));
            const quint8 staleValue = 37;
            projection->fill(requestedRect, KoColor(&staleValue, alpha8));

            shapeSelection->renderToProjection(projection, requestedRect);

            QImage expected(requestedRect.width(), requestedRect.height(), QImage::Format_ARGB32);
            expected.fill(Qt::black);
            QPainter painter(&expected);
            painter.setRenderHint(QPainter::Antialiasing, true);
            painter.translate(-requestedRect.x(), -requestedRect.y());
            painter.fillPath(expectedOutline, Qt::white);
            painter.end();

            for (int y = 0; y < requestedRect.height(); ++y) {
                for (int x = 0; x < requestedRect.width(); ++x) {
                    const int absoluteX = requestedRect.x() + x;
                    const int absoluteY = requestedRect.y() + y;
                    const quint8 actual = TestUtil::alphaDevicePixel(projection, absoluteX, absoluteY);
                    const quint8 oracle = qRed(expected.pixel(x, y));
                    const QString context = QStringLiteral(
                        "lod=%1 default=%2 x=%3 y=%4 Qt=%5 Pk=%6")
                        .arg(lod).arg(defaultValue).arg(absoluteX).arg(absoluteY)
                        .arg(oracle).arg(actual);
                    QVERIFY2(actual == oracle, qPrintable(context));
                }
            }
        }
    }
}


SIMPLE_TEST_MAIN(KisShapeSelectionTest)
