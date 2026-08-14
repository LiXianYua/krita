/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_document_desktop_services_test.h"

#include <KoCanvasResourceProvider.h>
#include <KoCanvasResourcesIds.h>
#include <KoColorSpaceRegistry.h>

#include <KisDocumentDesktop.h>
#include <kis_image.h>

#include <simpletest.h>

void KisDocumentDesktopServicesTest::testCanvasResourcesForActiveImage()
{
    KoCanvasResourceProvider manager;
    manager.setResource(KoCanvasResource::CurrentPattern, 101);
    manager.setResource(KoCanvasResource::CurrentGradient, 202);
    manager.setResource(KoCanvasResource::CurrentPaintOpPreset, 303);

    KisImageSP image = new KisImage(nullptr, 8, 8, KoColorSpaceRegistry::instance()->rgb8(), "active");
    KisImageSP unrelatedImage = new KisImage(nullptr, 8, 8, image->colorSpace(), "unrelated");

    KoCanvasResourcesInterfaceSP resources = KisDocumentDesktop::canvasResourcesForImage(
        image, image, &manager);
    QVERIFY(resources);
    QCOMPARE(resources->resource(KoCanvasResource::CurrentPattern).toInt(), 101);
    QCOMPARE(resources->resource(KoCanvasResource::CurrentGradient).toInt(), 202);
    QCOMPARE(resources->resource(KoCanvasResource::CurrentPaintOpPreset).toInt(), 303);

    QVERIFY(!KisDocumentDesktop::canvasResourcesForImage(unrelatedImage, image, &manager));
    QVERIFY(!KisDocumentDesktop::canvasResourcesForImage(image, image, nullptr));
}

SIMPLE_TEST_MAIN(KisDocumentDesktopServicesTest)
