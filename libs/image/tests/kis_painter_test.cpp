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

struct ExpectedRgba {
    quint8 red;
    quint8 green;
    quint8 blue;
    quint8 alpha;
};

struct MassiveBltFixture {
    int width;
    int height;
    const ExpectedRgba *palette;
    std::size_t paletteSize;
    const char *runs;
    std::size_t runsSize;
};

// The following normalized RGBA payloads were exported once, read-only, from
// the eight tracked PNG authorities under tests/data/kispainter_test. Each run
// is six hexadecimal digits: a 16-bit big-endian length and an 8-bit palette
// index. The source PNG SHA-256 values make the export provenance auditable.

// massive_bitblt_full_update_3.png
// sha256 cb5d75304c1f8a7fde09fb5391c517fb59dac4820be79a6b391015a5d12851c0
static constexpr ExpectedRgba fullUpdate3Palette[] = {
    {0, 0, 0, 0}, {255, 0, 0, 255}, {255, 255, 255, 255},
    {0, 255, 0, 255}, {0, 0, 255, 255},
};
static constexpr char fullUpdate3Runs[] =
    "02c600001e01002800001e01002800001e01002800001e01002800001e01002800000501001402000501002800000501"
    "001402000501002800000501001402000501002800000501001402000501002800000501001402000501002800000501"
    "000502001e03001e00000501000502001e03001e00000501000502001e03001e00000501000502001e03001e00000501"
    "000502001e03001e00000501000502000503001402000503001e00000501000502000503001402000503001e00000501"
    "000502000503001402000503001e00000501000502000503001402000503001e00000501000502000503001402000503"
    "001e00000501000502000503000502001e04001400000501000502000503000502001e04001400000501000502000503"
    "000502001e04001400000501000502000503000502001e04001400000501000502000503000502001e04001400000a01"
    "000503000502000504001402000504001400000a01000503000502000504001402000504001400000a01000503000502"
    "000504001402000504001400000a01000503000502000504001402000504001400000a01000503000502000504001402"
    "000504001e00000503000502000504001402000504001e00000503000502000504001402000504001e00000503000502"
    "000504001402000504001e00000503000502000504001402000504001e00000503000502000504001402000504001e00"
    "000a03000504001402000504001e00000a03000504001402000504001e00000a03000504001402000504001e00000a03"
    "000504001402000504001e00000a03000504001402000504002800000504001402000504002800000504001402000504"
    "002800000504001402000504002800000504001402000504002800000504001402000504002800001e04002800001e04"
    "002800001e04002800001e04002800001e0402c600";

// massive_bitblt_full_update_6.png
// sha256 75c0ba4426e324d336ff56164ff94d8698f1ad1a68f8317ac486de803da5e5e9
static constexpr ExpectedRgba fullUpdate6Palette[] = {
    {0, 0, 0, 0}, {255, 0, 0, 255}, {255, 255, 255, 255},
    {0, 255, 0, 255}, {0, 0, 255, 255},
};
static constexpr char fullUpdate6Runs[] =
    "03f200001e01004600001e01004600001e01004600001e01004600001e01004600000501001402000501004600000501"
    "001402000501004600000501001402000501004600000501001402000501004600000501001402000501004600000501"
    "000502001e03003c00000501000502001e03003c00000501000502001e03003c00000501000502001e03003c00000501"
    "000502001e03003c00000501000502000503001402000503003c00000501000502000503001402000503003c00000501"
    "000502000503001402000503003c00000501000502000503001402000503003c00000501000502000503001402000503"
    "003c00000501000502000503000502001e04003200000501000502000503000502001e04003200000501000502000503"
    "000502001e04003200000501000502000503000502001e04003200000501000502000503000502001e04003200000a01"
    "000503000502000504001402000504003200000a01000503000502000504001402000504003200000a01000503000502"
    "000504001402000504003200000a01000503000502000504001402000504003200000a01000503000502000504001402"
    "000504003c00000503000502000504000502001e01003200000503000502000504000502001e01003200000503000502"
    "000504000502001e01003200000503000502000504000502001e01003200000503000502000504000502001e01003200"
    "000a03000504000502000501001402000501003200000a03000504000502000501001402000501003200000a03000504"
    "000502000501001402000501003200000a03000504000502000501001402000501003200000a03000504000502000501"
    "001402000501003c00000504000502000501000502001e03003200000504000502000501000502001e03003200000504"
    "000502000501000502001e03003200000504000502000501000502001e03003200000504000502000501000502001e03"
    "003200000a04000501000502000503001402000503003200000a04000501000502000503001402000503003200000a04"
    "000501000502000503001402000503003200000a04000501000502000503001402000503003200000a04000501000502"
    "000503001402000503003c00000501000502000503000502001e04003200000501000502000503000502001e04003200"
    "000501000502000503000502001e04003200000501000502000503000502001e04003200000501000502000503000502"
    "001e04003200000a01000503000502000504001402000504003200000a01000503000502000504001402000504003200"
    "000a01000503000502000504001402000504003200000a01000503000502000504001402000504003200000a01000503"
    "000502000504001402000504003c00000503000502000504001402000504003c00000503000502000504001402000504"
    "003c00000503000502000504001402000504003c00000503000502000504001402000504003c00000503000502000504"
    "001402000504003c00000a03000504001402000504003c00000a03000504001402000504003c00000a03000504001402"
    "000504003c00000a03000504001402000504003c00000a03000504001402000504004600000504001402000504004600"
    "000504001402000504004600000504001402000504004600000504001402000504004600000504001402000504004600"
    "001e04004600001e04004600001e04004600001e04004600001e0403f200";

// massive_bitblt_full_update_6_sel.png
// sha256 dfd7fe3f71a5a4e07bfebcf4c20481397fac61b78301eb4a7e7eee4e724e6205
static constexpr ExpectedRgba fullUpdate6SelPalette[] = {
    {0, 0, 0, 0}, {255, 0, 0, 0}, {255, 255, 255, 0},
    {255, 255, 255, 255}, {255, 0, 0, 255}, {0, 255, 0, 255},
    {0, 0, 255, 255},
};
static constexpr char fullUpdate6SelRuns[] =
    "06b000000301000202001203000504004800000301000202001203000504004800000301000202001203000504004d00"
    "000303001e05004300000303001e05004300000303001e05004300000303001e05004300000303001e05004300000303"
    "000505001403000505004300000303000505001403000505004300000303000505001403000505004300000303000505"
    "001403000505004300000303000505001403000505004300000303000505000503001e06003900000303000505000503"
    "001e06003900000303000505000503001e06003900000303000505000503001e06003900000303000505000503001e06"
    "003900000304000505000503000506001403000506003900000304000505000503000506001403000506003900000304"
    "000505000503000506001403000506003900000304000505000503000506001403000506003900000304000505000503"
    "000506001403000506003c00000505000503000506000503001e04003200000505000503000506000503001e04003200"
    "000505000503000506000503001e04003200000505000503000506000503001e04003200000505000503000506000503"
    "001e04003200000a05000506000503000504001403000504003200000a05000506000503000504001403000504003200"
    "000a05000506000503000504001403000504003200000a05000506000503000504001403000504003200000a05000506"
    "000503000504001403000504003c00000506000503000504000503001e05003200000506000503000504000503001e05"
    "003200000506000503000504000503001e05003200000506000503000504000503001e05003200000506000503000504"
    "000503001e05003200000a06000504000503000505001403000505003200000a06000504000503000505001403000505"
    "003200000a06000504000503000505001403000505003200000a06000504000503000505001403000505003200000a06"
    "000504000503000505001403000505003c00000504000503000505000503001706003900000504000503000505000503"
    "001706003900000504000503000505000503001706003900000504000503000505000503001706003900000504000503"
    "000505000503001706003900000a04000505000503000506001203003900000a04000505000503000506001203003900"
    "000a04000505000503000506001203003900000a04000505000503000506001203003900000a04000505000503000506"
    "001203004300000505000503000506001203004300000505000503000506001203004300000505000503000506001203"
    "004300000505000503000506001203004300000505000503000506001203004300000a05000506001203004300000a05"
    "000506001203004300000a05000506001203004300000a05000506001203004300000a05000506001203004d00000506"
    "001203000102004c00000506001203000102004c0000050600120300010206b400";

// massive_bitblt_full_update_6_varyop.png
// sha256 ffc48e83d030251a86468ca3be8a44c420756af70aca95430c377c11eefada00
static constexpr ExpectedRgba fullUpdate6VaryopPalette[] = {
    {0, 0, 0, 0}, {255, 0, 0, 43}, {255, 0, 0, 42},
    {255, 255, 255, 42}, {255, 255, 255, 43}, {63, 255, 63, 113},
    {63, 192, 0, 113}, {64, 191, 0, 114}, {0, 255, 0, 85},
    {255, 255, 255, 113}, {255, 192, 192, 113}, {255, 191, 191, 114},
    {255, 255, 255, 85}, {255, 255, 255, 114}, {79, 79, 255, 185},
    {79, 59, 235, 185}, {64, 64, 255, 170}, {0, 64, 191, 170},
    {0, 0, 255, 128}, {255, 235, 235, 185}, {255, 255, 255, 170},
    {191, 255, 191, 170}, {255, 255, 255, 128}, {255, 64, 64, 227},
    {239, 64, 48, 227}, {255, 51, 51, 213}, {204, 0, 51, 213},
    {255, 0, 0, 170}, {239, 255, 239, 227}, {255, 255, 255, 213},
    {204, 204, 255, 213}, {37, 255, 37, 248}, {29, 248, 37, 248},
    {30, 255, 30, 241}, {30, 225, 0, 241}, {0, 255, 0, 213},
    {248, 248, 255, 248}, {255, 255, 255, 241}, {255, 225, 225, 241},
    {0, 0, 255, 255}, {255, 255, 255, 255},
};
static constexpr char fullUpdate6VaryopRuns[] =
    "03f200000201001802000401004600000201001802000401004600000201001802000401004600000201001802000401"
    "004600000201001802000401004600000201000302001403000102000401004600000201000302001403000102000401"
    "004600000201000302001403000102000401004600000201000302001403000102000401004600000201000302001403"
    "000102000401004600000501000504000f05000106000407000a08003c00000501000504000f05000106000407000a08"
    "003c00000501000504000f05000106000407000a08003c00000501000504000f05000106000407000a08003c00000501"
    "000504000f05000106000407000a08003c00000501000504000505000a0900010a00040b00050c000508003c00000501"
    "000504000505000a0900010a00040b00050c000508003c00000501000504000505000a0900010a00040b00050c000508"
    "003c00000501000504000505000a0900010a00040b00050c000508003c00000501000504000505000a0900010a00040b"
    "00050c000508003c0000050100050400050500030900020d00050e00050f000510000511000a12003200000501000504"
    "00050500030900020d00050e00050f000510000511000a1200320000050100050400050500030900020d00050e00050f"
    "000510000511000a1200320000050100050400050500030900020d00050e00050f000510000511000a12003200000501"
    "00050400050500030900020d00050e00050f000510000511000a12003200000a0100050600030a00020b00050f000513"
    "000514000515000516000512003200000a0100050600030a00020b00050f000513000514000515000516000512003200"
    "000a0100050600030a00020b00050f000513000514000515000516000512003200000a0100050600030a00020b00050f"
    "000513000514000515000516000512003200000a0100050600030a00020b00050f000513000514000515000516000512"
    "003c0000050800050c00051000051400051700051800051900051a000a1b00320000050800050c000510000514000517"
    "00051800051900051a000a1b00320000050800050c00051000051400051700051800051900051a000a1b003200000508"
    "00050c00051000051400051700051800051900051a000a1b00320000050800050c000510000514000517000518000519"
    "00051a000a1b003200000a0800051100051500051800051c00051d00051e00051400051b003200000a08000511000515"
    "00051800051c00051d00051e00051400051b003200000a0800051100051500051800051c00051d00051e00051400051b"
    "003200000a0800051100051500051800051c00051d00051e00051400051b003200000a0800051100051500051800051c"
    "00051d00051e00051400051b003c0000051200051600051900051d00051f000520000521000522000a23003200000512"
    "00051600051900051d00051f000520000521000522000a2300320000051200051600051900051d00051f000520000521"
    "000522000a2300320000051200051600051900051d00051f000520000521000522000a23003200000512000516000519"
    "00051d00051f000520000521000522000a23003200000a1200051a00051e00052000052400052500052600051d000523"
    "003200000a1200051a00051e00052000052400052500052600051d000523003200000a1200051a00051e000520000524"
    "00052500052600051d000523003200000a1200051a00051e00052000052400052500052600051d000523003200000a12"
    "00051a00051e00052000052400052500052600051d000523003c0000051b000514000521000525001e2700320000051b"
    "000514000521000525001e2700320000051b000514000521000525001e2700320000051b000514000521000525001e27"
    "00320000051b000514000521000525001e27003200000a1b000522000526000527001428000527003200000a1b000522"
    "000526000527001428000527003200000a1b000522000526000527001428000527003200000a1b000522000526000527"
    "001428000527003200000a1b000522000526000527001428000527003c0000052300051d000527001428000527003c00"
    "00052300051d000527001428000527003c0000052300051d000527001428000527003c0000052300051d000527001428"
    "000527003c0000052300051d000527001428000527003c00000a23000527001428000527003c00000a23000527001428"
    "000527003c00000a23000527001428000527003c00000a23000527001428000527003c00000a23000527001428000527"
    "004600000527001428000527004600000527001428000527004600000527001428000527004600000527001428000527"
    "004600000527001428000527004600001e27004600001e27004600001e27004600001e27004600001e2703f200";

// massive_bitblt_partial_update_3.png
// sha256 1ce8e3a0f1422d57de2212b0d09736c2a4a2e5429b7d3a05d55e59625df56bd1
static constexpr ExpectedRgba partialUpdate3Palette[] = {
    {0, 0, 0, 0}, {255, 0, 0, 255}, {255, 255, 255, 255},
    {0, 255, 0, 255}, {0, 0, 255, 255},
};
static constexpr char partialUpdate3Runs[] =
    "02c600001e01002800001e01002800001e01002800001e01002800001e01002800000501001402000501002800000501"
    "001402000501002800000501001402000501002800000501001402000501002800000501001402000501002800000501"
    "000502001403002800000501000502001403002800000501000502001403002800000501000502001403002800000501"
    "000502001403002800000501000502000503000f02002800000501000502000503000f02002800000501000502000503"
    "000f02002800000501000502000503000f02002800000501000502000503000f02002800000501000502000503000502"
    "000a04002800000501000502000503000502000a04002800000501000502000503000502000a04002800000501000502"
    "000503000502000a04002800000501000502000503000502000a04002800000a01000503000502000504000502002800"
    "000a01000503000502000504000502002800000a01000503000502000504000502002800000a01000503000502000504"
    "000502002800000a01000503000502000504000502003200000503000502000504000502003200000503000502000504"
    "000502003200000503000502000504000502003200000503000502000504000502003200000503000502000504000502"
    "003200000a03000504000502003200000a03000504000502003200000a03000504000502003200000a03000504000502"
    "003200000a03000504000502003c00000504000502003c00000504000502003c00000504000502003c00000504000502"
    "003c00000504000502003c00000a04003c00000a04003c00000a04003c00000a04003c00000a0402da00";

// massive_bitblt_partial_update_6.png
// sha256 df967a1b6681d87d53beb261ff48681d970ce0bdee188d376ff79c435f30ac53
static constexpr ExpectedRgba partialUpdate6Palette[] = {
    {0, 0, 0, 0}, {255, 0, 0, 255}, {255, 255, 255, 255},
    {0, 255, 0, 255}, {0, 0, 255, 255},
};
static constexpr char partialUpdate6Runs[] =
    "03f200001e01004600001e01004600001e01004600001e01004600001e01004600000501001402000501004600000501"
    "001402000501004600000501001402000501004600000501001402000501004600000501001402000501004600000501"
    "000502001e03003c00000501000502001e03003c00000501000502001e03003c00000501000502001e03003c00000501"
    "000502001e03003c00000501000502000503001402000503003c00000501000502000503001402000503003c00000501"
    "000502000503001402000503003c00000501000502000503001402000503003c00000501000502000503001402000503"
    "003c00000501000502000503000502001404003c00000501000502000503000502001404003c00000501000502000503"
    "000502001404003c00000501000502000503000502001404003c00000501000502000503000502001404003c00000a01"
    "000503000502000504000f02003c00000a01000503000502000504000f02003c00000a01000503000502000504000f02"
    "003c00000a01000503000502000504000f02003c00000a01000503000502000504000f02004600000503000502000504"
    "000502000a01004600000503000502000504000502000a01004600000503000502000504000502000a01004600000503"
    "000502000504000502000a01004600000503000502000504000502000a01004600000a03000504000502000501000502"
    "004600000a03000504000502000501000502004600000a03000504000502000501000502004600000a03000504000502"
    "000501000502004600000a03000504000502000501000502005000000504000502000501000502005000000504000502"
    "000501000502005000000504000502000501000502005000000504000502000501000502005000000504000502000501"
    "000502005000000a04000501000502005000000a04000501000502005000000a04000501000502005000000a04000501"
    "000502005000000a04000501000502005a00000501000502005a00000501000502005a00000501000502005a00000501"
    "000502005a00000501000502005a00000a01005a00000a01005a00000a01005a00000a01005a00000a010bea00";

// massive_bitblt_partial_update_6_sel.png
// sha256 7c2a41c662d8cc993288a0965fc00195a8b6c67bb02134f27e7f01a72c33dacd
static constexpr ExpectedRgba partialUpdate6SelPalette[] = {
    {0, 0, 0, 0}, {255, 255, 255, 255}, {255, 0, 0, 255},
    {0, 255, 0, 255}, {0, 0, 255, 255},
};
static constexpr char partialUpdate6SelRuns[] =
    "06b500001201000502004d00001201000502004d00001201000502004d00000301001e03004300000301001e03004300"
    "000301001e03004300000301001e03004300000301001e03004300000301000503001401000503004300000301000503"
    "001401000503004300000301000503001401000503004300000301000503001401000503004300000301000503001401"
    "000503004300000301000503000501001404004300000301000503000501001404004300000301000503000501001404"
    "004300000301000503000501001404004300000301000503000501001404004300000302000503000501000504000f01"
    "004300000302000503000501000504000f01004300000302000503000501000504000f01004300000302000503000501"
    "000504000f01004300000302000503000501000504000f01004600000503000501000504000501000a02004600000503"
    "000501000504000501000a02004600000503000501000504000501000a02004600000503000501000504000501000a02"
    "004600000503000501000504000501000a02004600000a03000504000501000502000501004600000a03000504000501"
    "000502000501004600000a03000504000501000502000501004600000a03000504000501000502000501004600000a03"
    "000504000501000502000501005000000504000501000502000501005000000504000501000502000501005000000504"
    "000501000502000501005000000504000501000502000501005000000504000501000502000501005000000a04000502"
    "000501005000000a04000502000501005000000a04000502000501005000000a04000502000501005000000a04000502"
    "000501005a00000502000501005a00000502000501005a00000502000501005a00000502000501005a00000502000501"
    "005a00000a02005a00000a02005a00000a02005a00000a02005a00000a020bea00";

// massive_bitblt_partial_update_6_varyop.png
// sha256 91141fe5ba08253cbc016d9d5909c40d8f36b434157f81064da6da18b1312f31
static constexpr ExpectedRgba partialUpdate6VaryopPalette[] = {
    {0, 0, 0, 0}, {255, 0, 0, 43}, {255, 0, 0, 42},
    {255, 255, 255, 43}, {255, 255, 255, 42}, {63, 255, 63, 113},
    {64, 255, 64, 114}, {64, 191, 0, 114}, {0, 255, 0, 85},
    {255, 255, 255, 113}, {255, 255, 255, 114}, {255, 191, 191, 114},
    {255, 255, 255, 85}, {79, 79, 255, 185}, {79, 59, 235, 185},
    {64, 64, 255, 170}, {0, 64, 191, 170}, {63, 192, 0, 113},
    {255, 192, 192, 113}, {255, 235, 235, 185}, {255, 255, 255, 170},
    {191, 255, 191, 170}, {255, 64, 64, 227}, {239, 64, 48, 227},
    {239, 255, 239, 227}, {0, 0, 255, 128}, {255, 255, 255, 128},
    {255, 51, 51, 213}, {255, 255, 255, 213}, {204, 0, 51, 213},
    {204, 204, 255, 213}, {255, 0, 0, 170},
};
static constexpr char partialUpdate6VaryopRuns[] =
    "03f200000a01000802000c01004600000a01000802000c01004600000a01000802000c01004600000a01000802000c01"
    "004600000a01000802000c01004600000501000503000804000703000501004600000501000503000804000703000501"
    "004600000501000503000804000703000501004600000501000503000804000703000501004600000501000503000804"
    "000703000501004600000501000503000805000706000507000a08003c00000501000503000805000706000507000a08"
    "003c00000501000503000805000706000507000a08003c00000501000503000805000706000507000a08003c00000501"
    "000503000805000706000507000a08003c0000050100050300050500030900070a00050b00050c000508003c00000501"
    "00050300050500030900070a00050b00050c000508003c0000050100050300050500030900070a00050b00050c000508"
    "003c0000050100050300050500030900070a00050b00050c000508003c0000050100050300050500030900070a00050b"
    "00050c000508003c0000050100050300050500030900020a00050d00050e00050f000510003c00000501000503000505"
    "00030900020a00050d00050e00050f000510003c0000050100050300050500030900020a00050d00050e00050f000510"
    "003c0000050100050300050500030900020a00050d00050e00050f000510003c0000050100050300050500030900020a"
    "00050d00050e00050f000510003c00000a0100051100031200020b00050e000513000514000515003c00000a01000511"
    "00031200020b00050e000513000514000515003c00000a0100051100031200020b00050e000513000514000515003c00"
    "000a0100051100031200020b00050e000513000514000515003c00000a0100051100031200020b00050e000513000514"
    "00051500460000050800050c00050f00051400051600051700460000050800050c00050f000514000516000517004600"
    "00050800050c00050f00051400051600051700460000050800050c00050f00051400051600051700460000050800050c"
    "00050f000514000516000517004600000a08000510000515000517000518004600000a08000510000515000517000518"
    "004600000a08000510000515000517000518004600000a08000510000515000517000518004600000a08000510000515"
    "00051700051800500000051900051a00051b00051c00500000051900051a00051b00051c00500000051900051a00051b"
    "00051c00500000051900051a00051b00051c00500000051900051a00051b00051c005000000a1900051d00051e005000"
    "000a1900051d00051e005000000a1900051d00051e005000000a1900051d00051e005000000a1900051d00051e005a00"
    "00051f000514005a0000051f000514005a0000051f000514005a0000051f000514005a0000051f000514005a00000a1f"
    "005a00000a1f005a00000a1f005a00000a1f005a00000a1f0bea00";

template<std::size_t PaletteSize, std::size_t RunsSize>
constexpr MassiveBltFixture makeMassiveBltFixture(int width, int height,
                                                  const ExpectedRgba (&palette)[PaletteSize],
                                                  const char (&runs)[RunsSize])
{
    return {width, height, palette, PaletteSize, runs, RunsSize - 1};
}

static constexpr MassiveBltFixture fullUpdate3 =
    makeMassiveBltFixture(70, 70, fullUpdate3Palette, fullUpdate3Runs);
static constexpr MassiveBltFixture fullUpdate6 =
    makeMassiveBltFixture(100, 100, fullUpdate6Palette, fullUpdate6Runs);
static constexpr MassiveBltFixture fullUpdate6Sel =
    makeMassiveBltFixture(100, 100, fullUpdate6SelPalette, fullUpdate6SelRuns);
static constexpr MassiveBltFixture fullUpdate6Varyop =
    makeMassiveBltFixture(100, 100, fullUpdate6VaryopPalette, fullUpdate6VaryopRuns);
static constexpr MassiveBltFixture partialUpdate3 =
    makeMassiveBltFixture(70, 70, partialUpdate3Palette, partialUpdate3Runs);
static constexpr MassiveBltFixture partialUpdate6 =
    makeMassiveBltFixture(100, 100, partialUpdate6Palette, partialUpdate6Runs);
static constexpr MassiveBltFixture partialUpdate6Sel =
    makeMassiveBltFixture(100, 100, partialUpdate6SelPalette, partialUpdate6SelRuns);
static constexpr MassiveBltFixture partialUpdate6Varyop =
    makeMassiveBltFixture(100, 100, partialUpdate6VaryopPalette, partialUpdate6VaryopRuns);

const MassiveBltFixture &massiveBltFixture(int numRects, bool varyOpacity,
                                           bool useSelection, bool partial)
{
    if (numRects == 3) {
        return partial ? partialUpdate3 : fullUpdate3;
    }
    if (useSelection) {
        return partial ? partialUpdate6Sel : fullUpdate6Sel;
    }
    if (varyOpacity) {
        return partial ? partialUpdate6Varyop : fullUpdate6Varyop;
    }
    return partial ? partialUpdate6 : fullUpdate6;
}

int hexNibble(char value)
{
    return value <= '9' ? value - '0' : value - 'a' + 10;
}

bool withinFixtureTolerance(int actual, int expected, int tolerance)
{
    const int delta = actual - expected;
    return delta >= -tolerance && delta <= tolerance;
}

void verifyExpectedPixel(const quint8 *pixel, const KoColorSpace *colorSpace,
                         const ExpectedRgba &expected)
{
    constexpr int fuzzy = 2;
    constexpr int fuzzyAlpha = 2;
    const int alpha = int(colorSpace->opacityU8(pixel));
    PK_VERIFY(withinFixtureTolerance(alpha, int(expected.alpha), fuzzyAlpha));

    // Preserve the historical TestUtil contract: invisible RGB storage is not
    // observable when both normalized pixels have zero alpha.
    const bool bothTransparent = alpha == int(OPACITY_TRANSPARENT_U8)
        && expected.alpha == OPACITY_TRANSPARENT_U8;
    if (!bothTransparent) {
        PK_VERIFY(withinFixtureTolerance(int(pixel[0]), int(expected.blue), fuzzy));
        PK_VERIFY(withinFixtureTolerance(int(pixel[1]), int(expected.green), fuzzy));
        PK_VERIFY(withinFixtureTolerance(int(pixel[2]), int(expected.red), fuzzy));
    }
}

void verifyMassiveBltFixture(const KisPaintDeviceSP &device,
                             const PkRect &fullRect,
                             const MassiveBltFixture &fixture)
{
    PK_COMPARE(fullRect.width(), fixture.width);
    PK_COMPARE(fullRect.height(), fixture.height);
    PK_COMPARE(fixture.runsSize % 6, std::size_t(0));

    const std::size_t expectedPixelCount =
        std::size_t(fixture.width) * std::size_t(fixture.height);
    std::size_t visitedPixelCount = 0;
    KisSequentialConstIterator it(device, fullRect);

    for (std::size_t offset = 0; offset < fixture.runsSize; offset += 6) {
        const int runLength =
            (hexNibble(fixture.runs[offset]) << 12)
            | (hexNibble(fixture.runs[offset + 1]) << 8)
            | (hexNibble(fixture.runs[offset + 2]) << 4)
            | hexNibble(fixture.runs[offset + 3]);
        const int paletteIndex =
            (hexNibble(fixture.runs[offset + 4]) << 4)
            | hexNibble(fixture.runs[offset + 5]);
        PK_VERIFY(runLength > 0);
        PK_VERIFY(paletteIndex >= 0
                  && std::size_t(paletteIndex) < fixture.paletteSize);

        for (int i = 0; i < runLength; ++i) {
            PK_VERIFY(visitedPixelCount < expectedPixelCount);
            PK_VERIFY(it.nextPixel());
            verifyExpectedPixel(it.oldRawData(), device->colorSpace(),
                                fixture.palette[paletteIndex]);
            ++visitedPixelCount;
        }
    }

    PK_COMPARE(visitedPixelCount, expectedPixelCount);
    PK_VERIFY(!it.nextPixel());
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
    {
        KisPainter painter(dst);
        painter.setSelection(selection);
        painter.bltFixed(fullRect, devices);
        painter.end();
        verifyMassiveBltFixture(dst, fullRect,
                               massiveBltFixture(numRects, varyOpacity,
                                                 useSelection, false));
    }

    dst->clear();

    {
        KisPainter painter(dst);
        painter.setSelection(selection);

        for (int i = fullRect.x(); i <= fullRect.center().x(); i += 10) {
            const PkRect rc(i, fullRect.y(), 10, fullRect.height());
            painter.bltFixed(rc, devices);
        }

        painter.end();
        const int partialEnd = std::min(fullRect.right(),
                                        fullRect.x() +
                                        ((fullRect.center().x() - fullRect.x()) / 10 + 1) * 10 - 1);
        const int untouchedX = partialEnd + 1;
        for (int y = fullRect.y(); y <= fullRect.bottom(); ++y) {
            KisSequentialConstIterator it(dst, PkRect(untouchedX, y, fullRect.right() - untouchedX + 1, 1));
            while (it.nextPixel()) {
                PK_COMPARE(int(dst->colorSpace()->opacityU8(it.oldRawData())), int(OPACITY_TRANSPARENT_U8));
            }
        }
        verifyMassiveBltFixture(dst, fullRect,
                               massiveBltFixture(numRects, varyOpacity,
                                                 useSelection, true));

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
