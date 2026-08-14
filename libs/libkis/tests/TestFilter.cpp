/* SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "TestFilter.h"
#include <simpletest.h>

#include <KritaVersionWrapper.h>
#include <QColor>
#include <QDataStream>
#include <QSet>

#include <Node.h>
#include <Krita.h>
#include <Document.h>
#include <Filter.h>

#include <KoColorSpaceRegistry.h>
#include <KoColorProfile.h>
#include <KoColor.h>
#include <KoCanvasResourcesIds.h>
#include <KoCanvasResourcesInterface.h>

#include <KisDocument.h>
#include <KisDocumentApplicationServices.h>
#include <kis_image.h>
#include <kis_fill_painter.h>
#include <kis_paint_layer.h>
#include <KisDocumentRegistry.h>

#include <testui.h>

namespace
{

class RecordingCanvasResources final : public KoCanvasResourcesInterface
{
public:
    QVariant resource(int key) const override
    {
        requestedResources.insert(key);
        return {};
    }

    mutable QSet<int> requestedResources;
};

class FilterDocumentServices final : public KisDocumentApplicationServices
{
public:
    KoCanvasResourcesInterfaceSP canvasResourcesForImage(KisImageSP image) override
    {
        requestedImage = image;
        return resources;
    }

    KisImageSP requestedImage;
    KoCanvasResourcesInterfaceSP resources =
        KoCanvasResourcesInterfaceSP(new RecordingCanvasResources);
};

class DocumentServicesRestorer
{
public:
    DocumentServicesRestorer()
        : m_services(KisDocumentApplicationServices::instance())
    {
    }

    ~DocumentServicesRestorer()
    {
        KisDocumentApplicationServices::setInstance(m_services);
    }

private:
    KisDocumentApplicationServices *m_services;
};

}

void TestFilter::testApply()
{
    KisDocument *kisdoc = KisDocumentRegistry::instance()->createDocument();
    KisImageSP image = new KisImage(0, 100, 100, KoColorSpaceRegistry::instance()->rgb8(), "test");
    KisNodeSP layer = new KisPaintLayer(image, "test1", 255);
    KisFillPainter gc(layer->paintDevice());
    gc.fillRect(0, 0, 100, 100, KoColor(Qt::black, layer->colorSpace()));
    image->addNode(layer);
    kisdoc->setCurrentImage(image);
    Document d(kisdoc, false);
    NodeSP node = NodeSP(Node::createNode(image, layer));

    Filter f;
    f.setName("invert");
    QVERIFY(f.configuration());

    d.lock();
    f.apply(node.data(), 0, 0, 100, 100);
    d.unlock();
    d.refreshProjection();

    for (int i = 0; i < 100 ; i++) {
        for (int j = 0; j < 100 ; j++) {
            QColor pixel;
            layer->paintDevice()->pixel(i, j, &pixel);
            QVERIFY(pixel == QColor(Qt::white));
        }
    }

}

void TestFilter::testStartFilter()
{
    KisDocument *kisdoc = KisDocumentRegistry::instance()->createDocument();
    KisImageSP image = new KisImage(0, 100, 100, KoColorSpaceRegistry::instance()->rgb8(), "test");
    KisNodeSP layer = new KisPaintLayer(image, "test1", 255);
    KisFillPainter gc(layer->paintDevice());
    gc.fillRect(0, 0, 100, 100, KoColor(Qt::black, layer->colorSpace()));
    image->addNode(layer);
    kisdoc->setCurrentImage(image);
    Document d(kisdoc, false);
    NodeSP node = NodeSP(Node::createNode(image, layer));

    Filter f;
    f.setName("invert");
    QVERIFY(f.configuration());

    f.startFilter(node.data(), 0, 0, 100, 100);
    image->waitForDone();

    for (int i = 0; i < 100 ; i++) {
        for (int j = 0; j < 100 ; j++) {
            QColor pixel;
            layer->paintDevice()->pixel(i, j, &pixel);
            QVERIFY(pixel == QColor(Qt::white));
        }
    }
}

void TestFilter::testStartFilterUsesCanvasResources()
{
    DocumentServicesRestorer restoreServices;
    FilterDocumentServices services;
    KisDocumentApplicationServices::setInstance(&services);

    KisDocument *kisdoc = KisDocumentRegistry::instance()->createDocument();
    KisImageSP image = new KisImage(0, 10, 10, KoColorSpaceRegistry::instance()->rgb8(), "test");
    KisNodeSP layer = new KisPaintLayer(image, "test1", 255);
    KisFillPainter gc(layer->paintDevice());
    gc.fillRect(0, 0, 10, 10, KoColor(Qt::black, layer->colorSpace()));
    image->addNode(layer);
    kisdoc->setCurrentImage(image);
    Document document(kisdoc, false);
    NodeSP node = NodeSP(Node::createNode(image, layer));

    Filter filter;
    filter.setName("invert");
    QVERIFY(filter.startFilter(node.data(), 0, 0, 10, 10));

    QCOMPARE(services.requestedImage, image);
    const auto resources = services.resources.dynamicCast<RecordingCanvasResources>();
    QVERIFY(resources);
    QVERIFY(resources->requestedResources.contains(KoCanvasResource::CurrentPattern));
    QVERIFY(resources->requestedResources.contains(KoCanvasResource::CurrentGradient));
    QVERIFY(resources->requestedResources.contains(KoCanvasResource::CurrentPaintOpPreset));

    QColor pixel;
    layer->paintDevice()->pixel(0, 0, &pixel);
    QCOMPARE(pixel, QColor(Qt::white));
}

KISTEST_MAIN(TestFilter)
