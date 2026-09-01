/*
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_painter_test.h"

#define KRITA_TESTSDK_PK_NATIVE
#include <simpletest.h>

#include <PkColor.h>
#include <PkElapsedTimer.h>
#include <PkList.h>
#include <PkRect.h>

#include <KoChannelInfo.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoCompositeOpRegistry.h>

#include "kis_datamanager.h"
#include "kis_types.h"
#include "kis_paint_device.h"
#include "kis_painter.h"
#include "kis_pixel_selection.h"
#include "kis_fill_painter.h"
#include <kis_fixed_paint_device.h>
#include <kis_iterator_ng.h>
#include <kis_sequential_iterator.h>

#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkTransform.h>

#include "../private/kis_path_rasterizer_p.h"

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <vector>

namespace {

PkPainterPath nestedRectPath(Qt::FillRule fillRule)
{
    PkPainterPath path;
    path.setFillRule(fillRule);
    path.moveTo(7.25, 6.5);
    path.lineTo(29.75, 6.5);
    path.lineTo(29.75, 27.5);
    path.lineTo(7.25, 27.5);
    path.closeSubpath();
    path.moveTo(13.0, 12.0);
    path.lineTo(23.0, 12.0);
    path.lineTo(23.0, 22.0);
    path.lineTo(13.0, 22.0);
    path.closeSubpath();
    return path;
}

PkRect fillBounds(const PkPainterPath &path, const PkRect &requestedRect)
{
    PkRect bounds = path.boundingRect().toAlignedRect();
    bounds.adjust(-1, -1, 1, 1);
    if (requestedRect.isValid()) {
        bounds &= requestedRect;
    }
    return bounds;
}

PkRect strokeBounds(const PkPainterPath &path, const PkPen &pen,
                    const PkRect &requestedRect)
{
    PkRect bounds = path.boundingRect().toAlignedRect();
    const int penWidth = qRound(pen.widthF());
    bounds.adjust(-penWidth, -penWidth, penWidth, penWidth);
    bounds.adjust(-1, -1, 1, 1);
    if (!requestedRect.isNull()) {
        bounds &= requestedRect;
    }
    return bounds;
}

std::size_t alphaIndex(const PkRect &checkRect, int x, int y)
{
    return std::size_t(y - checkRect.y()) * std::size_t(checkRect.width())
        + std::size_t(x - checkRect.x());
}

void writeFillExpected(std::vector<quint8> &expected,
                       const PkRect &checkRect,
                       const PkPainterPath &path,
                       const PkRect &requestedRect,
                       int chunkWidth,
                       int chunkHeight,
                       bool antialiased)
{
    const PkRect bounds = fillBounds(path, requestedRect);
    for (int x = bounds.x(); x < bounds.x() + bounds.width(); x += chunkWidth) {
        for (int y = bounds.y(); y < bounds.y() + bounds.height(); y += chunkHeight) {
            const PkRect chunk(x, y,
                               std::min(bounds.x() + bounds.width() - x, chunkWidth),
                               std::min(bounds.y() + bounds.height() - y, chunkHeight));
            const KisPathRasterizer::CoverageMask mask =
                KisPathRasterizer::rasterizeFill(path, chunk, antialiased);
            PK_COMPARE(mask.bounds, chunk);
            for (int row = chunk.y(); row <= chunk.bottom(); ++row) {
                for (int column = chunk.x(); column <= chunk.right(); ++column) {
                    if (checkRect.contains(column, row)) {
                        expected[alphaIndex(checkRect, column, row)] =
                            mask.coverageAt(column, row);
                    }
                }
            }
        }
    }
}

void writeStrokeExpected(std::vector<quint8> &expected,
                         const PkRect &checkRect,
                         const PkPainterPath &path,
                         const PkPen &pen,
                         const PkRect &requestedRect,
                         int chunkWidth,
                         int chunkHeight,
                         bool antialiased)
{
    const PkRect bounds = strokeBounds(path, pen, requestedRect);
    for (int x = bounds.x(); x < bounds.x() + bounds.width(); x += chunkWidth) {
        for (int y = bounds.y(); y < bounds.y() + bounds.height(); y += chunkHeight) {
            const PkRect chunk(x, y,
                               std::min(bounds.x() + bounds.width() - x, chunkWidth),
                               std::min(bounds.y() + bounds.height() - y, chunkHeight));
            const KisPathRasterizer::CoverageMask mask =
                KisPathRasterizer::rasterizeStroke(path, pen, chunk, antialiased);
            PK_COMPARE(mask.bounds, chunk);
            for (int row = chunk.y(); row <= chunk.bottom(); ++row) {
                for (int column = chunk.x(); column <= chunk.right(); ++column) {
                    if (checkRect.contains(column, row)) {
                        expected[alphaIndex(checkRect, column, row)] =
                            mask.coverageAt(column, row);
                    }
                }
            }
        }
    }
}

void compareEveryAlphaByte(const KisPaintDeviceSP &device,
                           const PkRect &checkRect,
                           const std::vector<quint8> &expected)
{
    PK_COMPARE(expected.size(),
             std::size_t(checkRect.width()) * std::size_t(checkRect.height()));
    KisSequentialConstIterator it(device, checkRect);
    while (it.nextPixel()) {
        const quint8 actual = device->colorSpace()->opacityU8(it.oldRawData());
        const quint8 wanted = expected[alphaIndex(checkRect, it.x(), it.y())];
        PK_COMPARE(actual, wanted);
        if (wanted == OPACITY_TRANSPARENT_U8) {
            PK_COMPARE(actual, quint8(OPACITY_TRANSPARENT_U8));
        }
    }
}

KisPaintDeviceSP renderFill(const PkPainterPath &path,
                            const PkRect &requestedRect = PkRect(),
                            int chunkWidth = 255,
                            int chunkHeight = 255,
                            const PkPointF &mirrorCenter = PkPointF(),
                            bool mirrorHorizontally = false,
                            bool mirrorVertically = false)
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP device = new KisPaintDevice(cs);
    KisPainter painter(device);
    painter.setPaintColor(KoColor(Qt::red, cs));
    painter.setFillStyle(KisPainter::FillStyleForegroundColor);
    painter.setStrokeStyle(KisPainter::StrokeStyleNone);
    painter.setAntiAliasPolygonFill(true);
    painter.setMaskImageSize(chunkWidth, chunkHeight);
    painter.setMirrorInformation(mirrorCenter, mirrorHorizontally, mirrorVertically);
    painter.fillPainterPath(path, requestedRect);
    painter.end();
    return device;
}

} // namespace

void KisPainterTest::allCsApplicator(void (KisPainterTest::* funcPtr)(const KoColorSpace*cs))
{
    const PkList<const KoColorSpace*> colorspaces = KoColorSpaceRegistry::instance()->allColorSpaces(KoColorSpaceRegistry::AllColorSpaces, KoColorSpaceRegistry::OnlyDefaultProfile);

    for (const KoColorSpace *cs : colorspaces) {

        const PkString csId = cs->id();
        // ALL THESE COLORSPACES ARE BROKEN: WE NEED UNITTESTS FOR COLORSPACES!
        if (csId.startsWith("KS")) continue;
        if (csId.startsWith("Xyz")) continue;
        if (csId.startsWith("Y")) continue;
        if (csId.contains("AF")) continue;
        if (csId == "GRAYU16") continue; // No point in testing bounds with a cs without alpha
        if (csId == "GRAYU8") continue; // No point in testing bounds with a cs without alpha

        if (cs && cs->compositeOp(COMPOSITE_OVER) != 0) {
            (this->*funcPtr)(cs);
        }
    }
}

void KisPainterTest::testSimpleBlt(const KoColorSpace * cs)
{

    KisPaintDeviceSP dst = new KisPaintDevice(cs);
    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KoColor c(Qt::red, cs);
    c.setOpacity(quint8(128));
    src->fill(20, 20, 20, 20, c.data());

    PK_COMPARE(src->exactBounds(), PkRect(20, 20, 20, 20));

    const KoCompositeOp* op;

    {
        op = cs->compositeOp(COMPOSITE_OVER);
        KisPainter painter(dst);
        painter.setCompositeOpId(op);
        painter.bitBlt(50, 50, src, 20, 20, 20, 20);
        painter.end();
        PK_COMPARE(dst->exactBounds(), PkRect(50,50,20,20));
    }

    dst->clear();

    {
        op = cs->compositeOp(COMPOSITE_COPY);
        KisPainter painter(dst);
        painter.setCompositeOpId(op);
        painter.bitBlt(50, 50, src, 20, 20, 20, 20);
        painter.end();
        PK_COMPARE(dst->exactBounds(), PkRect(50,50,20,20));
    }
}

void KisPainterTest::testSimpleBlt()
{
    allCsApplicator(&KisPainterTest::testSimpleBlt);
}

/*

Note: the bltSelection tests assume the following geometry:

0,0               0,30
  +---------+------+
  |  10,10  |      |
  |    +----+      |
  |    |####|      |
  |    |####|      |
  +----+----+      |
  |       20,20    |
  |                |
  |                |
  +----------------+
                  30,30
 */
void KisPainterTest::testPaintDeviceBltSelection(const KoColorSpace * cs)
{

    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KoColor c(Qt::red, cs);
    c.setOpacity(quint8(128));
    src->fill(0, 0, 20, 20, c.data());

    PK_COMPARE(src->exactBounds(), PkRect(0, 0, 20, 20));

    KisSelectionSP selection = new KisSelection();
    selection->pixelSelection()->select(PkRect(10, 10, 20, 20));
    selection->updateProjection();
    PK_COMPARE(selection->selectedExactRect(), PkRect(10, 10, 20, 20));

    KisPainter painter(dst);
    painter.setSelection(selection);

    painter.bitBlt(0, 0, src, 0, 0, 30, 30);
    painter.end();

    PK_COMPARE(dst->exactBounds(), PkRect(10, 10, 10, 10));

    const KoCompositeOp* op = cs->compositeOp(COMPOSITE_SUBTRACT);
    if (op->id() == COMPOSITE_SUBTRACT) {

        KisPaintDeviceSP dst2 = new KisPaintDevice(cs);
        KisPainter painter2(dst2);
        painter2.setSelection(selection);
        painter2.setCompositeOpId(op);
        painter2.bitBlt(0, 0, src, 0, 0, 30, 30);
        painter2.end();

        PK_COMPARE(dst2->exactBounds(), PkRect(10, 10, 10, 10));
    }
}

void KisPainterTest::testPaintDeviceBltSelection()
{
    allCsApplicator(&KisPainterTest::testPaintDeviceBltSelection);
}

void KisPainterTest::testPaintDeviceBltSelectionIrregular(const KoColorSpace * cs)
{

    KisPaintDeviceSP dst = new KisPaintDevice(cs);
    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KisFillPainter gc(src);
    gc.fillRect(0, 0, 20, 20, KoColor(Qt::red, cs));
    gc.end();

    PK_COMPARE(src->exactBounds(), PkRect(0, 0, 20, 20));

    KisSelectionSP sel = new KisSelection();

    KisPixelSelectionSP psel = sel->pixelSelection();
    psel->select(PkRect(10, 15, 20, 15));
    psel->select(PkRect(15, 10, 15, 5));

    PK_COMPARE(psel->selectedExactRect(), PkRect(10, 10, 20, 20));
    KisSequentialConstIterator alphaIt(psel, PkRect(13, 13, 1, 1));
    PK_VERIFY(alphaIt.nextPixel());
    PK_COMPARE(psel->colorSpace()->opacityU8(alphaIt.oldRawData()), quint8(MIN_SELECTED));

    KisPainter painter(dst);
    painter.setSelection(sel);
    painter.bitBlt(0, 0, src, 0, 0, 30, 30);
    painter.end();

    PK_COMPARE(dst->exactBounds(), PkRect(10, 10, 10, 10));
    for (KoChannelInfo *channel : cs->channels()) {
        // Only compare alpha if there actually is an alpha channel in
        // this colorspace
        if (channel->channelType() == KoChannelInfo::ALPHA) {
            PkColor c;

            dst->pixel(13, 13, &c);

            PK_COMPARE((int) c.alpha(), (int) OPACITY_TRANSPARENT_U8);
        }
    }
}


void KisPainterTest::testPaintDeviceBltSelectionIrregular()
{
    allCsApplicator(&KisPainterTest::testPaintDeviceBltSelectionIrregular);
}

void KisPainterTest::testPaintDeviceBltSelectionInverted(const KoColorSpace * cs)
{

    KisPaintDeviceSP dst = new KisPaintDevice(cs);
    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KisFillPainter gc(src);
    gc.fillRect(0, 0, 30, 30, KoColor(Qt::red, cs));
    gc.end();
    PK_COMPARE(src->exactBounds(), PkRect(0, 0, 30, 30));

    KisSelectionSP sel = new KisSelection();
    KisPixelSelectionSP psel = sel->pixelSelection();
    psel->select(PkRect(10, 10, 20, 20));
    psel->invert();
    sel->updateProjection();

    KisPainter painter(dst);
    painter.setSelection(sel);
    painter.bitBlt(0, 0, src, 0, 0, 30, 30);
    painter.end();
    PK_COMPARE(dst->exactBounds(), PkRect(0, 0, 30, 30));
}

void KisPainterTest::testPaintDeviceBltSelectionInverted()
{
    allCsApplicator(&KisPainterTest::testPaintDeviceBltSelectionInverted);
}


void KisPainterTest::testSelectionBltSelection()
{
    KisPixelSelectionSP src = new KisPixelSelection();
    src->select(PkRect(0, 0, 20, 20));
    PK_COMPARE(src->selectedExactRect(), PkRect(0, 0, 20, 20));

    KisSelectionSP sel = new KisSelection();

    KisPixelSelectionSP Selection = sel->pixelSelection();
    Selection->select(PkRect(10, 10, 20, 20));
    PK_COMPARE(Selection->selectedExactRect(), PkRect(10, 10, 20, 20));

    sel->updateProjection();
    KisPixelSelectionSP dst = new KisPixelSelection();
    KisPainter painter(dst);
    painter.setSelection(sel);
    painter.bitBlt(0, 0, src, 0, 0, 30, 30);
    painter.end();

    PK_COMPARE(dst->selectedExactRect(), PkRect(10, 10, 10, 10));

    KisSequentialConstIterator it(dst, PkRect(10, 10, 10, 10));
    while (it.nextPixel()) {
        // These are selections, so only one channel and it should
        // be totally selected
        PK_COMPARE(it.oldRawData()[0], MAX_SELECTED);
    }
}

/*

Test with non-square selection

0,0               0,30
  +-----------+------+
  |    13,13  |      |
  |      x +--+      |
  |     +--+##|      |
  |     |#####|      |
  +-----+-----+      |
  |         20,20    |
  |                  |
  |                  |
  +------------------+
                  30,30
 */
void KisPainterTest::testSelectionBltSelectionIrregular()
{

    KisPaintDeviceSP dev = new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8());

    KisPixelSelectionSP src = new KisPixelSelection();
    src->select(PkRect(0, 0, 20, 20));
    PK_COMPARE(src->selectedExactRect(), PkRect(0, 0, 20, 20));

    KisSelectionSP sel = new KisSelection();

    KisPixelSelectionSP Selection = sel->pixelSelection();
    Selection->select(PkRect(10, 15, 20, 15));
    Selection->select(PkRect(15, 10, 15, 5));
    PK_COMPARE(Selection->selectedExactRect(), PkRect(10, 10, 20, 20));
    KisSequentialConstIterator selectionAlphaIt(Selection, PkRect(13, 13, 1, 1));
    PK_VERIFY(selectionAlphaIt.nextPixel());
    PK_COMPARE(Selection->colorSpace()->opacityU8(selectionAlphaIt.oldRawData()), quint8(MIN_SELECTED));

    sel->updateProjection();

    KisPixelSelectionSP dst = new KisPixelSelection();
    KisPainter painter(dst);
    painter.setSelection(sel);
    painter.bitBlt(0, 0, src, 0, 0, 30, 30);
    painter.end();

    PK_COMPARE(dst->selectedExactRect(), PkRect(10, 10, 10, 10));
    KisSequentialConstIterator dstAlphaIt(dst, PkRect(13, 13, 1, 1));
    PK_VERIFY(dstAlphaIt.nextPixel());
    PK_COMPARE(dst->colorSpace()->opacityU8(dstAlphaIt.oldRawData()), quint8(MIN_SELECTED));
}

void KisPainterTest::testSelectionBitBltFixedSelection()
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KoColor c(Qt::red, cs);
    c.setOpacity(quint8(128));
    src->fill(0, 0, 20, 20, c.data());

    PK_COMPARE(src->exactBounds(), PkRect(0, 0, 20, 20));

    KisFixedPaintDeviceSP fixedSelection = new KisFixedPaintDevice(cs);
    fixedSelection->setRect(PkRect(0, 0, 20, 20));
    fixedSelection->initialize();
    KoColor fill(Qt::white, cs);
    fixedSelection->fill(5, 5, 10, 10, fill.data());
    fixedSelection->convertTo(KoColorSpaceRegistry::instance()->alpha8());

    KisPainter painter(dst);

    painter.bitBltWithFixedSelection(0, 0, src, fixedSelection, 20, 20);
    painter.end();

    PK_COMPARE(dst->exactBounds(), PkRect(5, 5, 10, 10));
    /*
dbgKrita << "canary1.5";
    dst->clear();
    painter.begin(dst);

    painter.bitBltWithFixedSelection(0, 0, src, fixedSelection, 10, 20);
    painter.end();
dbgKrita << "canary2";
    PK_COMPARE(dst->exactBounds(), PkRect(5, 5, 5, 10));

    dst->clear();
    painter.begin(dst);

    painter.bitBltWithFixedSelection(0, 0, src, fixedSelection, 5, 5, 5, 5, 10, 20);
    painter.end();
dbgKrita << "canary3";
    PK_COMPARE(dst->exactBounds(), PkRect(5, 5, 5, 10));

    dst->clear();
    painter.begin(dst);

    painter.bitBltWithFixedSelection(5, 5, src, fixedSelection, 10, 20);
    painter.end();
dbgKrita << "canary4";
    PK_COMPARE(dst->exactBounds(), PkRect(10, 10, 5, 10));
    */
}

void KisPainterTest::testSelectionBitBltEraseCompositeOp()
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dst = new KisPaintDevice(cs);
    KoColor c(Qt::red, cs);
    dst->fill(0, 0, 150, 150, c.data());

    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KoColor c2(Qt::black, cs);
    src->fill(50, 50, 50, 50, c2.data());

    KisSelectionSP sel = new KisSelection();
    KisPixelSelectionSP selection = sel->pixelSelection();
    selection->select(PkRect(25, 25, 100, 100));
    sel->updateProjection();

    const KoCompositeOp* op = cs->compositeOp(COMPOSITE_ERASE);
    KisPainter painter(dst);
    painter.setSelection(sel);
    painter.setCompositeOpId(op);
    painter.bitBlt(0, 0, src, 0, 0, 150, 150);
    painter.end();

    //dst->convertToQImage(0).save("result.png");

    PkRect erasedRect(50, 50, 50, 50);
    KisSequentialConstIterator it(dst, PkRect(0, 0, 150, 150));
    while (it.nextPixel()) {
        if(!erasedRect.contains(it.x(), it.y())) {
             PK_VERIFY(memcmp(it.oldRawData(), c.data(), cs->pixelSize()) == 0);
        }
    }

}

void KisPainterTest::testSimpleAlphaCopy()
{
    KisPaintDeviceSP src = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
    KisPaintDeviceSP dst = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
    quint8 p = 128;
    src->fill(0, 0, 100, 100, &p);
    PK_VERIFY(src->exactBounds() == PkRect(0, 0, 100, 100));
    KisPainter gc(dst);
    gc.setCompositeOpId(KoColorSpaceRegistry::instance()->alpha8()->compositeOp(COMPOSITE_COPY));
    gc.bitBlt(PkPoint(0, 0), src, src->exactBounds());
    gc.end();
    PK_COMPARE(dst->exactBounds(), PkRect(0, 0, 100, 100));

}

void KisPainterTest::checkPerformance()
{
    KisPaintDeviceSP src = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
    KisPaintDeviceSP dst = new KisPaintDevice(KoColorSpaceRegistry::instance()->alpha8());
    quint8 p = 128;
    src->fill(0, 0, 10000, 5000, &p);
    KisSelectionSP sel = new KisSelection();
    sel->pixelSelection()->select(PkRect(0, 0, 10000, 5000), 128);
    sel->updateProjection();

    PkElapsedTimer t;
    t.start();
    for (int i = 0; i < 10; ++i) {
        KisPainter gc(dst);
        gc.bitBlt(0, 0, src, 0, 0, 10000, 5000);
    }

    t.restart();
    for (int i = 0; i < 10; ++i) {
        KisPainter gc(dst, sel);
        gc.bitBlt(0, 0, src, 0, 0, 10000, 5000);
    }
}

void KisPainterTest::testBitBltOldData()
{
    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->alpha8();

    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    quint8 defaultPixel = 0;
    quint8 p1 = 128;
    quint8 p2 = 129;
    quint8 p3 = 130;
    KoColor defaultColor(&defaultPixel, cs);
    KoColor color1(&p1, cs);
    KoColor color2(&p2, cs);
    KoColor color3(&p3, cs);
    PkRect fillRect(0,0,5000,5000);

    src->fill(fillRect, color1);

    KisPainter srcGc(src);
    srcGc.beginTransaction();
    src->fill(fillRect, color2);

    KisPainter dstGc(dst);
    dstGc.bitBltOldData(PkPoint(), src, fillRect);

    KisSequentialConstIterator alphaIt(dst, fillRect);
    while (alphaIt.nextPixel()) {
        PK_COMPARE(alphaIt.oldRawData()[0], p1);
    }

    dstGc.end();
    srcGc.deleteTransaction();
}

#include "KisRenderedDab.h"

bool devicesAreEqual(const KisPaintDeviceSP &lhs,
                     const KisPaintDeviceSP &rhs,
                     const PkRect &rect)
{
    KisSequentialConstIterator lhsIt(lhs, rect);
    KisSequentialConstIterator rhsIt(rhs, rect);
    while (lhsIt.nextPixel()) {
        if (!rhsIt.nextPixel()) {
            return false;
        }

        const quint8 *lhsPixel = lhsIt.oldRawData();
        const quint8 *rhsPixel = rhsIt.oldRawData();
        const bool bothTransparent =
            lhs->colorSpace()->opacityU8(lhsPixel) == OPACITY_TRANSPARENT_U8 &&
            rhs->colorSpace()->opacityU8(rhsPixel) == OPACITY_TRANSPARENT_U8;

        if (!bothTransparent &&
            std::memcmp(lhsPixel, rhsPixel, lhs->pixelSize()) != 0) {
            return false;
        }
    }
    return !rhsIt.nextPixel();
}

void testMassiveBltFixedImpl(int numRects, bool varyOpacity = false, bool useSelection = false)
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    PkList<PkColor> colors;
    colors << PkColor(Qt::red);
    colors << PkColor(Qt::green);
    colors << PkColor(Qt::blue);

    PkRect devicesRect;
    PkList<KisRenderedDab> devices;

    for (int i = 0; i < numRects; i++) {
        const PkRect rc(10 + i * 10, 10 + i * 10, 30, 30);
        KisFixedPaintDeviceSP dev = new KisFixedPaintDevice(cs);
        dev->setRect(rc);
        dev->initialize();
        dev->fill(rc, KoColor(colors[i % 3], cs));
        dev->fill(kisGrowRect(rc, -5), KoColor(Qt::white, cs));

        KisRenderedDab dab;
        dab.device = dev;
        dab.offset = dev->bounds().topLeft();
        dab.opacity = varyOpacity ? qreal(1 + i) / numRects : 1.0;
        dab.flow = 1.0;

        devices << dab;
        devicesRect |= rc;
    }

    KisSelectionSP selection;

    if (useSelection) {
        selection = new KisSelection();
        selection->pixelSelection()->select(kisGrowRect(devicesRect, -7));
    }

    const PkRect fullRect = kisGrowRect(devicesRect, 10);
    KisPaintDeviceSP fullResult;

    {
        KisPainter painter(dst);
        painter.setSelection(selection);
        painter.bltFixed(fullRect, devices);
        painter.end();
        fullResult = new KisPaintDevice(*dst);
    }

    dst->clear();

    {
        KisPainter painter(dst);
        painter.setSelection(selection);

        for (int i = fullRect.x(); i <= fullRect.right(); i += 10) {
            const PkRect rc(i, fullRect.y(), 10, fullRect.height());
            painter.bltFixed(rc, devices);
        }

        painter.end();
        PK_VERIFY(devicesAreEqual(dst, fullResult, fullRect));

    }
}

void KisPainterTest::testMassiveBltFixedSingleTile()
{
    testMassiveBltFixedImpl(3);
}

void KisPainterTest::testMassiveBltFixedMultiTile()
{
    testMassiveBltFixedImpl(6);
}

void KisPainterTest::testMassiveBltFixedMultiTileWithOpacity()
{
    testMassiveBltFixedImpl(6, true);
}

void KisPainterTest::testMassiveBltFixedMultiTileWithSelection()
{
    testMassiveBltFixedImpl(6, false, true);
}

void KisPainterTest::testMassiveBltFixedCornerCases()
{
    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    PkList<KisRenderedDab> devices;

    PK_VERIFY(dst->extent().isEmpty());

    {
        // empty devices, shouldn't crash
        KisPainter painter(dst);
        painter.bltFixed(PkRect(60,60,20,20), devices);
        painter.end();
    }

    PK_VERIFY(dst->extent().isEmpty());

    const PkRect rc(10,10,20,20);
    KisFixedPaintDeviceSP dev = new KisFixedPaintDevice(cs);
    dev->setRect(rc);
    dev->initialize();
    dev->fill(rc, KoColor(Qt::white, cs));

    devices.append(KisRenderedDab(dev));

    {
        // rect outside the devices bounds, shouldn't crash
        KisPainter painter(dst);
        painter.bltFixed(PkRect(60,60,20,20), devices);
        painter.end();
    }

    PK_VERIFY(dst->extent().isEmpty());
}

void KisPainterTest::testFillPainterPathRules()
{
    const PkRect checkRect(0, 0, 38, 35);

    for (Qt::FillRule rule : {Qt::OddEvenFill, Qt::WindingFill}) {
        const PkPainterPath path = nestedRectPath(rule);
        const KisPaintDeviceSP device = renderFill(path);
        std::vector<quint8> expected(std::size_t(checkRect.width()) *
                                     std::size_t(checkRect.height()), 0);
        writeFillExpected(expected, checkRect, path, PkRect(), 255, 255, true);
        compareEveryAlphaByte(device, checkRect, expected);
    }
}

void KisPainterTest::testFillPainterPathMirroring()
{
    PkPainterPath path;
    path.moveTo(7.5, 8.25);
    path.lineTo(14.5, 9.0);
    path.lineTo(12.0, 19.5);
    path.closeSubpath();

    const PkPointF center(25.0, 17.0);
    PkPainterPath mirrored;
    mirrored.moveTo(2.0 * center.x() - 7.5, 8.25);
    mirrored.lineTo(2.0 * center.x() - 14.5, 9.0);
    mirrored.lineTo(2.0 * center.x() - 12.0, 19.5);
    mirrored.closeSubpath();

    const PkRect checkRect(0, 0, 51, 30);
    const KisPaintDeviceSP device = renderFill(path, PkRect(), 255, 255,
                                               center, true, false);
    std::vector<quint8> expected(std::size_t(checkRect.width()) *
                                 std::size_t(checkRect.height()), 0);
    writeFillExpected(expected, checkRect, path, PkRect(), 255, 255, true);
    writeFillExpected(expected, checkRect, mirrored, PkRect(), 255, 255, true);
    compareEveryAlphaByte(device, checkRect, expected);
}

void KisPainterTest::testFillPainterPathRequestedRect()
{
    PkPainterPath path;
    path.moveTo(4.5, 4.5);
    path.lineTo(31.5, 6.25);
    path.lineTo(28.0, 26.5);
    path.lineTo(6.0, 24.0);
    path.closeSubpath();

    const PkRect requestedRect(12, 9, 10, 8);
    const PkRect checkRect(0, 0, 38, 32);
    const KisPaintDeviceSP device = renderFill(path, requestedRect);
    std::vector<quint8> expected(std::size_t(checkRect.width()) *
                                 std::size_t(checkRect.height()), 0);
    writeFillExpected(expected, checkRect, path, requestedRect, 255, 255, true);
    compareEveryAlphaByte(device, checkRect, expected);
}

void KisPainterTest::testFillPainterPathChunking()
{
    PkPainterPath path;
    path.moveTo(2.25, 3.5);
    path.cubicTo(13.0, -1.5, 19.0, 24.0, 31.75, 17.25);
    path.lineTo(27.0, 27.5);
    path.lineTo(4.0, 24.5);
    path.closeSubpath();

    const PkRect checkRect(-2, -4, 40, 38);
    const KisPaintDeviceSP device = renderFill(path, PkRect(), 7, 5);
    std::vector<quint8> expected(std::size_t(checkRect.width()) *
                                 std::size_t(checkRect.height()), 0);
    writeFillExpected(expected, checkRect, path, PkRect(), 7, 5, true);
    compareEveryAlphaByte(device, checkRect, expected);
}

void KisPainterTest::testDrawPainterPathStroke()
{
    PkPainterPath path;
    path.moveTo(6.25, 23.5);
    path.cubicTo(13.0, 1.75, 29.5, 38.0, 41.25, 9.5);

    PkPen pen(Qt::white);
    pen.setWidthF(4.0);
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::MiterJoin);
    pen.setMiterLimit(6.0);

    const KoColorSpace *cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP device = new KisPaintDevice(cs);
    KisPainter painter(device);
    painter.setPaintColor(KoColor(Qt::blue, cs));
    painter.setAntiAliasPolygonFill(true);
    painter.setMaskImageSize(7, 5);
    painter.drawPainterPath(path, pen);
    painter.end();

    const PkRect checkRect(-2, -3, 52, 46);
    std::vector<quint8> expected(std::size_t(checkRect.width()) *
                                 std::size_t(checkRect.height()), 0);
    writeStrokeExpected(expected, checkRect, path, pen, PkRect(), 7, 5, true);
    compareEveryAlphaByte(device, checkRect, expected);
}


#include "kis_lod_transform.h"

inline PkRect extentifyRect(const PkRect &rc)
{
    return KisLodTransform::alignedRect(rc, 6);
}

void testOptimizedCopyingImpl(const PkRect &srcRect,
                              const PkRect &dstRect,
                              const PkRect &srcCopyRect,
                              const PkPoint &dstPt,
                              const PkRect &expectedDstBounds)
{
    const PkRect expectedDstExtent = extentifyRect(expectedDstBounds);

    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP src = new KisPaintDevice(cs);
    KisPaintDeviceSP dst = new KisPaintDevice(cs);

    const KoColor color1(Qt::red, cs);
    const KoColor color2(Qt::blue, cs);

    src->fill(srcRect, color1);
    dst->fill(dstRect, color2);

    KisPainter::copyAreaOptimized(dstPt, src, dst, srcCopyRect);

    //KIS_DUMP_DEVICE_2(dst, PkRect(0,0,5000,5000), "dst", "dd");

    PK_COMPARE(dst->exactBounds(), expectedDstBounds);
    PK_COMPARE(dst->extent(), expectedDstExtent);
}

void KisPainterTest::testOptimizedCopying()
{
    const PkRect srcRect(1000, 1000, 1000, 1000);
    const PkRect srcCopyRect(0, 0, 5000, 5000);


    testOptimizedCopyingImpl(srcRect, PkRect(6000, 500, 1000,1000),
                             srcCopyRect, srcCopyRect.topLeft(),
                             PkRect(1000, 500, 6000, 1500));

    testOptimizedCopyingImpl(srcRect, PkRect(4500, 1500, 1000, 1000),
                             srcCopyRect, srcCopyRect.topLeft(),
                             PkRect(1000, 1000, 4500, 1500));

    testOptimizedCopyingImpl(srcRect, PkRect(2500, 2500, 1000, 1000),
                             srcCopyRect, srcCopyRect.topLeft(),
                             srcRect);

    testOptimizedCopyingImpl(srcRect, PkRect(1200, 1200, 600, 1600),
                             srcCopyRect, srcCopyRect.topLeft(),
                             srcRect);

    testOptimizedCopyingImpl(srcRect, PkRect(1200, 1200, 600, 600),
                             srcCopyRect, srcCopyRect.topLeft(),
                             srcRect);

}

// Binder generated from kis_painter_test.h with pk/test/pk_test_moc.py. It is
// kept in this authorized source because this target has no separate generated
// Pk binder source yet.
template <> struct PkTestBinder<KisPainterTest> {
    static const char *className() { return "KisPainterTest"; }
    static const PkTestFunction *functions() {
        static const PkTestFunction fns[] = {
            {"testSimpleBlt", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testSimpleBlt(); }, nullptr},
            {"testSelectionBltSelectionIrregular", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testSelectionBltSelectionIrregular(); }, nullptr},
            {"testPaintDeviceBltSelectionInverted", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testPaintDeviceBltSelectionInverted(); }, nullptr},
            {"testPaintDeviceBltSelectionIrregular", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testPaintDeviceBltSelectionIrregular(); }, nullptr},
            {"testPaintDeviceBltSelection", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testPaintDeviceBltSelection(); }, nullptr},
            {"testSelectionBltSelection", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testSelectionBltSelection(); }, nullptr},
            {"testSimpleAlphaCopy", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testSimpleAlphaCopy(); }, nullptr},
            {"testSelectionBitBltFixedSelection", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testSelectionBitBltFixedSelection(); }, nullptr},
            {"testSelectionBitBltEraseCompositeOp", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testSelectionBitBltEraseCompositeOp(); }, nullptr},
            {"testBitBltOldData", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testBitBltOldData(); }, nullptr},
            {"testMassiveBltFixedSingleTile", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testMassiveBltFixedSingleTile(); }, nullptr},
            {"testMassiveBltFixedMultiTile", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testMassiveBltFixedMultiTile(); }, nullptr},
            {"testMassiveBltFixedMultiTileWithOpacity", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testMassiveBltFixedMultiTileWithOpacity(); }, nullptr},
            {"testMassiveBltFixedMultiTileWithSelection", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testMassiveBltFixedMultiTileWithSelection(); }, nullptr},
            {"testMassiveBltFixedCornerCases", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testMassiveBltFixedCornerCases(); }, nullptr},
            {"testFillPainterPathRules", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testFillPainterPathRules(); }, nullptr},
            {"testFillPainterPathMirroring", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testFillPainterPathMirroring(); }, nullptr},
            {"testFillPainterPathRequestedRect", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testFillPainterPathRequestedRect(); }, nullptr},
            {"testFillPainterPathChunking", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testFillPainterPathChunking(); }, nullptr},
            {"testDrawPainterPathStroke", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testDrawPainterPathStroke(); }, nullptr},
            {"testOptimizedCopying", [](PkTestObject *o){ static_cast<KisPainterTest *>(o)->testOptimizedCopying(); }, nullptr},
        };
        return fns;
    }
    static int count() { return 21; }
    static const PkTestFunction *dataFunctions() { return nullptr; }
    static int dataCount() { return 0; }
    static const PkTestFunction *initTestCase() { return nullptr; }
    static const PkTestFunction *cleanupTestCase() { return nullptr; }
    static const PkTestFunction *initFn() { return nullptr; }
    static const PkTestFunction *cleanupFn() { return nullptr; }
    static const PkTestFunction *initTestCaseData() { return nullptr; }
};

SIMPLE_TEST_MAIN(KisPainterTest)
