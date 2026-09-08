/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "fill_processing_visitor_test.h"

#include <simpletest.h>

#include <KoColorSpaceRegistry.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_pixel_selection.h>
#include <kis_resources_snapshot.h>
#include <kis_default_bounds.h>
#include <kis_undo_stores.h>
#include <kis_undo_adapter.h>

#include <processing/KisEncloseAndFillProcessingVisitor.h>

void FillProcessingVisitorTest::fillsPixelsReportsDirtyRegionAndUndoes()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    auto *undoStore = new KisSurrogateUndoStore;
    KisImageSP image = new KisImage(undoStore, 8, 8, colorSpace, PkString("fill visitor"));
    KisPaintLayerSP layer = new KisPaintLayer(image, PkString("paint"), OPACITY_OPAQUE_U8);
    image->addNode(layer);

    KisPixelSelectionSP enclosingMask =
        new KisPixelSelection(new KisSelectionDefaultBounds(layer->paintDevice()));
    const PkRect enclosingRect(2, 2, 3, 3);
    enclosingMask->select(enclosingRect);

    KisResourcesSnapshotSP resources = new KisResourcesSnapshot(image, layer);
    resources->setFGColorOverride(KoColor(Pk::red, colorSpace));
    PkSharedPointer<PkRect> dirtyRect(new PkRect);

    KisEncloseAndFillProcessingVisitor visitor(
        layer->paintDevice(), enclosingMask, {}, resources,
        KisEncloseAndFillPainter::SelectAllRegions,
        KoColor(Pk::transparent, colorSpace), false, true, true,
        8, 100, 0, false, 0, false, 0, false, false, true, false,
        false, 1.0, PkString(), dirtyRect);

    KoColor before;
    layer->paintDevice()->pixel(3, 3, &before);
    QCOMPARE(before.opacityU8(), quint8(0));

    visitor.visit(layer.data(), image->undoAdapter());

    KoColor filled;
    layer->paintDevice()->pixel(3, 3, &filled);
    QCOMPARE(filled.toQColor(), PkColor(Pk::red));
    QCOMPARE(*dirtyRect, layer->paintDevice()->extent());

    image->undoAdapter()->undoLastCommand();
    KoColor undone;
    layer->paintDevice()->pixel(3, 3, &undone);
    QCOMPARE(undone.opacityU8(), quint8(0));
}

SIMPLE_TEST_MAIN(FillProcessingVisitorTest)
