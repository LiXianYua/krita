/*
 * SPDX-FileCopyrightText: 2026 OpenAI
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TestLibkisSelection.h"

#include <simpletest.h>

#include <KoColor.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>

#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_paint_layer.h>
#include <kis_pixel_selection.h>
#include <kis_selection.h>

#include "KisSelectionClipStore.h"
#include "Node.h"
#include "Selection.h"

void TestLibkisSelection::testCopyPasteUsesSelectionClipStore()
{
    const KoColorSpace *const colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(nullptr, 2, 1, colorSpace, "selection clip test");

    KisPaintLayerSP sourceLayer = new KisPaintLayer(image, "source", OPACITY_OPAQUE_U8);
    sourceLayer->paintDevice()->fill(QRect(0, 0, 1, 1),
                                     KoColor(QColor(10, 20, 30, 128), colorSpace));
    image->addNode(sourceLayer);

    KisSelectionSP selection = new KisSelection();
    selection->pixelSelection()->select(QRect(0, 0, 1, 1), 128);

    QScopedPointer<Node> source(Node::createNode(image, sourceLayer));
    Selection scriptingSelection(selection);
    scriptingSelection.copy(source.data());

    KisPaintDeviceSP clip = KisSelectionClipStore::instance()->clip();
    QVERIFY(clip);
    QCOMPARE(clip->exactBounds(), QRect(0, 0, 1, 1));
    const KoColor clippedPixel = clip->pixel(QPoint(0, 0));
    QCOMPARE(colorSpace->opacityU8(clippedPixel.data()), quint8(85));

    KisPaintLayerSP destinationLayer = new KisPaintLayer(image, "destination", OPACITY_OPAQUE_U8);
    image->addNode(destinationLayer);
    QScopedPointer<Node> destination(Node::createNode(image, destinationLayer));

    selection->clear();
    selection->pixelSelection()->select(QRect(1, 0, 1, 1), OPACITY_OPAQUE_U8);
    scriptingSelection.paste(destination.data(), 1, 0);

    const KoColor pastedPixel = destinationLayer->paintDevice()->pixel(QPoint(1, 0));
    QCOMPARE(colorSpace->opacityU8(pastedPixel.data()), quint8(85));
}

SIMPLE_TEST_MAIN(TestLibkisSelection)
