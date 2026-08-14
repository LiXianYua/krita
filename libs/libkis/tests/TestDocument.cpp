/* SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include "TestDocument.h"
#include <simpletest.h>

#include <atomic>
#include <thread>

#include <KritaVersionWrapper.h>
#include <QColor>
#include <QDataStream>
#include <QDir>
#include <QBuffer>
#include <QSemaphore>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTextStream>
#include <QThread>

#include <Node.h>
#include <Krita.h>
#include <Document.h>

#include <KoColorSpaceRegistry.h>
#include <KoColorProfile.h>
#include <KoColor.h>

#include <KisDocument.h>
#include <KisDocumentApplicationServices.h>
#include <KisDocumentRegistry.h>
#include <kis_image.h>
#include <kis_simple_stroke_strategy.h>
#include <kis_fill_painter.h>
#include <kis_paint_layer.h>
#include <kis_transform_mask_params_factory_registry.h>
#include <kis_undo_stores.h>
#include <testui.h>

#include <KisPortingUtils.h>

namespace
{

class BlockingStrokeStrategy final : public KisSimpleStrokeStrategy
{
public:
    BlockingStrokeStrategy(QSemaphore &started,
                           QSemaphore &release,
                           std::atomic_bool &finished)
        : KisSimpleStrokeStrategy(QLatin1String("BlockingStrokeStrategy"))
        , m_started(started)
        , m_release(release)
        , m_finished(finished)
    {
        enableJob(KisSimpleStrokeStrategy::JOB_DOSTROKE);
    }

    void doStrokeCallback(KisStrokeJobData *) override
    {
        m_started.release();
        m_release.acquire();
        m_finished.store(true, std::memory_order_release);
    }

private:
    QSemaphore &m_started;
    QSemaphore &m_release;
    std::atomic_bool &m_finished;
};

class RecordingDocumentServices final : public KisDocumentApplicationServices
{
public:
    void closeDocumentViews(KisDocument *document) override
    {
        closedDocument = document;
    }

    KisDocument *closedDocument {nullptr};
};

}

void TestDocument::testDocumentRegistryLifecycle()
{
    qRegisterMetaType<KisDocument *>();

    struct ServicesRestorer {
        KisDocumentApplicationServices *services;
        ~ServicesRestorer()
        {
            KisDocumentApplicationServices::setInstance(services);
        }
    } restoreServices {KisDocumentApplicationServices::instance()};

    KisDocumentApplicationServices::setInstance(nullptr);
    KisDocumentApplicationServices *services = KisDocumentApplicationServices::instance();
#if defined(Q_OS_WIN)
    QCOMPARE(services->autoSaveLocation(), QDir::tempPath());
#elif defined(Q_OS_ANDROID)
    QCOMPARE(services->autoSaveLocation(),
             QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)
                 .append(QStringLiteral("/krita-backup")));
#else
    QCOMPARE(services->autoSaveLocation(), QDir::homePath());
#endif
    QCOMPARE(services->defaultAssistantsColor(), QColor(176, 176, 176, 255));
    QCOMPARE(services->backupFileEnabled(), true);
    QCOMPARE(services->autoSaveInterval(), 7 * 60);
    QCOMPARE(services->undoStackLimit(), 200);
    QCOMPARE(services->autoPinLayersToTimeline(), true);

    KisImageSP pendingImage = new KisImage(nullptr,
                                           10,
                                           10,
                                           KoColorSpaceRegistry::instance()->rgb8(),
                                           QStringLiteral("pending image"));
    QSemaphore strokeStarted;
    QSemaphore releaseStroke;
    std::atomic_bool strokeFinished {false};
    KisStrokeId strokeId = pendingImage->startStroke(
        new BlockingStrokeStrategy(strokeStarted, releaseStroke, strokeFinished));
    pendingImage->addJob(strokeId, nullptr);
    pendingImage->endStroke(strokeId);

    if (!strokeStarted.tryAcquire(1, 5000)) {
        releaseStroke.release();
        pendingImage->waitForDone();
        QFAIL("The test stroke did not start");
    }

    std::atomic_bool releaseTriggered {false};
    std::thread releaseThread([&releaseStroke, &releaseTriggered]() {
        QThread::msleep(50);
        releaseTriggered.store(true, std::memory_order_release);
        releaseStroke.release();
    });
    const bool waitResult = services->waitForImage(
        pendingImage, KisDocumentApplicationServices::WaitMode::Forced);
    const bool releaseHappenedBeforeReturn =
        releaseTriggered.load(std::memory_order_acquire);
    releaseThread.join();

    QVERIFY(waitResult);
    QVERIFY(releaseHappenedBeforeReturn);
    QVERIFY(strokeFinished.load(std::memory_order_acquire));
    QVERIFY(pendingImage->isIdle());

    KisDocumentRegistry registry;
    QSignalSpy addedSpy(&registry, &KisDocumentRegistry::sigDocumentAdded);
    QSignalSpy savedSpy(&registry, &KisDocumentRegistry::sigDocumentSaved);
    QSignalSpy removedSpy(&registry, &KisDocumentRegistry::sigDocumentRemoved);

    QScopedPointer<KisDocument> document(registry.createDocument());
    QVERIFY(document);
    QCOMPARE(registry.documentCount(), 0);

    registry.addDocument(document.data());
    registry.addDocument(document.data());
    QCOMPARE(registry.documentCount(), 1);
    QCOMPARE(registry.documents().size(), 1);
    QCOMPARE(addedSpy.count(), 1);
    QCOMPARE(addedSpy.at(0).at(0).value<KisDocument *>(), document.data());

    document->setPath(QStringLiteral("/tmp/registry-document.kra"));
    Q_EMIT document->sigSavingFinished(document->path());
    QCOMPARE(savedSpy.count(), 1);
    QCOMPARE(savedSpy.at(0).at(0).toString(), document->path());

    registry.removeDocument(document.data(), false);
    QCOMPARE(registry.documentCount(), 0);
    QCOMPARE(removedSpy.count(), 1);
    QCOMPARE(removedSpy.at(0).at(0).toString(), document->path());
}

void TestDocument::testHeadlessActiveNode()
{
    struct ServicesRestorer {
        KisDocumentApplicationServices *services;
        ~ServicesRestorer()
        {
            KisDocumentApplicationServices::setInstance(services);
        }
    } restoreServices {KisDocumentApplicationServices::instance()};
    KisDocumentApplicationServices::setInstance(nullptr);

    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(nullptr,
                                    100,
                                    100,
                                    KoColorSpaceRegistry::instance()->rgb8(),
                                    QStringLiteral("headless active node"));
    KisNodeSP layer = new KisPaintLayer(image, QStringLiteral("active"), 255);
    image->addNode(layer);
    kisdoc->setCurrentImage(image);

    Document document(kisdoc.data(), false);
    Document secondWrapper(kisdoc.data(), false);
    QScopedPointer<Node> requested(Node::createNode(image, layer));

    QVERIFY(!document.activeNode());
    document.setActiveNode(requested.data());
    QCOMPARE(kisdoc->preActivatedNode(), layer);

    QScopedPointer<Node> active(secondWrapper.activeNode());
    QVERIFY(active);
    QCOMPARE(active->uniqueId(), requested->uniqueId());
    QCOMPARE(active->name(), QStringLiteral("active"));
}

void TestDocument::testCloseUsesApplicationServices()
{
    RecordingDocumentServices services;
    struct ServicesRestorer {
        KisDocumentApplicationServices *services;
        ~ServicesRestorer()
        {
            KisDocumentApplicationServices::setInstance(services);
        }
    } restoreServices {KisDocumentApplicationServices::instance()};
    KisDocumentApplicationServices::setInstance(&services);

    KisDocument *kisdoc = KisDocumentRegistry::instance()->createDocument();
    KisDocumentRegistry::instance()->addDocument(kisdoc, false);
    Document document(kisdoc, true);

    document.close();
    QCOMPARE(services.closedDocument, kisdoc);
    QVERIFY(!KisDocumentRegistry::instance()->documents().contains(kisdoc));

    KisImageSP image = new KisImage(nullptr,
                                    10,
                                    10,
                                    KoColorSpaceRegistry::instance()->rgb8(),
                                    QStringLiteral("invalid wrapper"));
    KisNodeSP layer = new KisPaintLayer(image, QStringLiteral("node"), 255);
    image->addNode(layer);
    QScopedPointer<Node> node(Node::createNode(image, layer));

    document.setActiveNode(node.data());
    QVERIFY(!document.activeNode());
    QVERIFY(!document.close());
}

void TestDocument::testSetColorSpace()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(0, 100, 100, KoColorSpaceRegistry::instance()->rgb8(), "test");
    KisNodeSP layer = new KisPaintLayer(image, "test1", 255);
    image->addNode(layer);
    kisdoc->setCurrentImage(image);

    Document d(kisdoc.data(), false);
    QStringList profiles = Krita().profiles("GRAYA", "U16");
    d.setColorSpace("GRAYA", "U16", profiles.first());

    QVERIFY(layer->colorSpace()->colorModelId().id() == "GRAYA");
    QVERIFY(layer->colorSpace()->colorDepthId().id() == "U16");
    QVERIFY(layer->colorSpace()->profile()->name() == profiles.first());

    KisDocumentRegistry::instance()->removeDocument(kisdoc.data(), false);
}

void TestDocument::testSetColorProfile()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(0, 100, 100, KoColorSpaceRegistry::instance()->rgb8(), "test");
    KisNodeSP layer = new KisPaintLayer(image, "test1", 255);
    image->addNode(layer);
    kisdoc->setCurrentImage(image);

    Document d(kisdoc.data(), false);

    QStringList profiles = Krita().profiles("RGBA", "U8");
    Q_FOREACH(const QString &profileName, profiles) {
        const KoColorProfile *profile = KoColorSpaceRegistry::instance()->profileByName(profileName);

        // skip input-only profiles (e.g. for scanners)
        if (!profile->isSuitableForOutput()) continue;

        d.setColorProfile(profileName);
        QVERIFY(image->colorSpace()->profile()->name() == profileName);
    }
    KisDocumentRegistry::instance()->removeDocument(kisdoc.data(), false);
}

void TestDocument::testPixelData()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(0, 100, 100, KoColorSpaceRegistry::instance()->rgb8(), "test");
    KisNodeSP layer = new KisPaintLayer(image, "test1", 255);
    KisFillPainter gc(layer->paintDevice());
    gc.fillRect(0, 0, 100, 100, KoColor(Qt::red, layer->colorSpace()));
    image->addNode(layer);
    kisdoc->setCurrentImage(image);

    Document d(kisdoc.data(), false);
    d.refreshProjection();

    QByteArray ba = d.pixelData(0, 0, 100, 100);
    QDataStream ds(ba);
    do {
        quint8 channelvalue;
        ds >> channelvalue;
        QVERIFY(channelvalue == 0);
        ds >> channelvalue;
        QVERIFY(channelvalue == 0);
        ds >> channelvalue;
        QVERIFY(channelvalue == 255);
        ds >> channelvalue;
        QVERIFY(channelvalue == 255);
    } while (!ds.atEnd());

    KisDocumentRegistry::instance()->removeDocument(kisdoc.data(), false);
}

void TestDocument::testThumbnail()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(0, 100, 100, KoColorSpaceRegistry::instance()->rgb8(), "test");
    KisNodeSP layer = new KisPaintLayer(image, "test1", 255);
    KisFillPainter gc(layer->paintDevice());
    gc.fillRect(0, 0, 100, 100, KoColor(Qt::red, layer->colorSpace()));
    image->addNode(layer);
    kisdoc->setCurrentImage(image);

    Document d(kisdoc.data(), false);
    d.refreshProjection();

    QImage thumb = d.thumbnail(10, 10);
    thumb.save("thumb.png");
    QVERIFY(thumb.width() == 10);
    QVERIFY(thumb.height() == 10);
    // Our thumbnail calculator in KisPaintDevice cannot make a filled 10x10 thumbnail from a 100x100 device,
    // it makes it 10x10 empty, then puts 8x8 pixels in there... Not a bug in the Node class
    for (int i = 0; i < 8; ++i) {
        for (int j = 0; j < 8; ++j) {
            QVERIFY(thumb.pixelColor(i, j) == QColor(Qt::red));
        }
    }
    KisDocumentRegistry::instance()->removeDocument(kisdoc.data(), false);
}

void TestDocument::testCreateFillLayer()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(0, 50, 50, KoColorSpaceRegistry::instance()->rgb16(), "test");
    kisdoc->setCurrentImage(image);
    Document d(kisdoc.data(), false);

    const QString pattern("pattern");
    const QString color("color");
    const QString filllayer = "filllayer";
    InfoObject info;
    Selection sel(image->globalSelection());

    FillLayer *f = d.createFillLayer("test1", pattern, info, sel);
    QVERIFY(f->generatorName() == pattern);
    QVERIFY(f->type() == filllayer);
    delete f;
    f = d.createFillLayer("test1", color, info, sel);
    QVERIFY(f->generatorName() == color);
    QVERIFY(f->type() == filllayer);

    info.setProperty(pattern, "Cross01.pat");
    QVERIFY(f->setGenerator(pattern, &info));
    QVERIFY(f->filterConfig()->property(pattern).toString() == "Cross01.pat");
    QVERIFY(f->generatorName() == pattern);
    QVERIFY(f->type() == filllayer);

    info.setProperty(color, QColor(Qt::red));
    QVERIFY(f->setGenerator(color, &info));
    QVariant v = f->filterConfig()->property(color);
    QColor c = v.value<QColor>();
    QVERIFY(c == QColor(Qt::red));
    QVERIFY(f->generatorName() == color);
    QVERIFY(f->type() == filllayer);

    bool r = f->setGenerator(QString("xxx"), &info);
    QVERIFY(!r);

    delete f;

    QVERIFY(d.createFillLayer("test1", "xxx", info, sel) == 0);

    KisDocumentRegistry::instance()->removeDocument(kisdoc.data(), false);
}

void TestDocument::testCreateCloneLayer()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(0, 50, 50, KoColorSpaceRegistry::instance()->rgb16(), "test");
    kisdoc->setCurrentImage(image);
    Document d(kisdoc.data(), false);

    const QString layerType = "clonelayer";
    const QString node1Name = "node1";
    const QString node2Name = "node2";

    Node *node1 = d.createNode(node1Name,"paintlayer");
    Node *node2 = d.createNode(node2Name,"paintlayer");
    CloneLayer *layer = d.createCloneLayer("test1", node1);
    Node* rootNode = d.rootNode();

    rootNode->addChildNode(node1,0);
    rootNode->addChildNode(node2,0);
    rootNode->addChildNode(layer,0);

    QVERIFY(layer->type() == layerType);

    Node *sourceNode1 = layer->sourceNode();

    QVERIFY(sourceNode1->name() == node1Name);

    layer->setSourceNode(node2);

    Node *sourceNode2 = layer->sourceNode();

    QVERIFY(sourceNode2->name() == node2Name);

    delete layer;
    delete node1;
    delete node2;
    delete sourceNode1;
    delete sourceNode2;
    delete rootNode;

    KisDocumentRegistry::instance()->removeDocument(kisdoc.data(), false);
}

void TestDocument::testCreateTransparencyMask()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(0, 50, 50, KoColorSpaceRegistry::instance()->rgb16(), "test");
    kisdoc->setCurrentImage(image);
    Document d(kisdoc.data(), false);

    const QString layerType = "transparencymask";

    Selection sel(image->globalSelection());

    sel.select(10,10,10,10,255);

    Node *node = d.createNode("node1","paintlayer");
    TransparencyMask *mask = d.createTransparencyMask("test1");
    Node* rootNode = d.rootNode();

    rootNode->addChildNode(node,0);
    node->addChildNode(mask,0);

    Selection* selResult = mask->selection();

    QVERIFY(mask->type() == layerType);
    QVERIFY(selResult->width() == sel.width() && selResult->height() == sel.height());

    delete selResult;
    delete mask;
    delete node;
    delete rootNode;

    KisDocumentRegistry::instance()->removeDocument(kisdoc.data(), false);
}

void TestDocument::testCreateColorizeMask()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(new KisSurrogateUndoStore(),  10, 3, KoColorSpaceRegistry::instance()->rgb8(), "test");

    kisdoc->setCurrentImage(image);

    Document d(kisdoc.data(), false);
    const QString layerType = "colorizemask";

    Node *node = d.createNode("node1","paintlayer");

    ColorizeMask *mask = d.createColorizeMask("test1");
    Node* rootNode = d.rootNode();

    QByteArray nodeData = QByteArray::fromBase64("AAAAAAAAAAAAAAAAEQYMBhEGDP8RBgz/EQYMAgAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAARBgz5EQYM/xEGDAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAEQYMAhEGDAkRBgwCAAAAAAAAAAAAAAAA");

    node->setPixelData(nodeData,0,0,10,3);
    rootNode->addChildNode(node,0);

    d.waitForDone();
    d.refreshProjection();

    node->addChildNode(mask,0);

    qApp->processEvents();
    d.waitForDone();

    ManagedColor col1("RGBA","U8","");
    ManagedColor col2("RGBA","U8","");

    col1.setComponents({1.0, 0.0, 0.0, 1.0});
    col2.setComponents({0.0, 0.0, 1.0, 1.0});

    QVERIFY(mask->type() == layerType);

    mask->setEditKeyStrokes(true);
    QVERIFY(mask->editKeyStrokes() == true);

    mask->setUseEdgeDetection(true);
    QVERIFY(mask->useEdgeDetection() == true);

    mask->setEdgeDetectionSize(4.0);
    QVERIFY(mask->edgeDetectionSize() == 4.0);

    mask->setCleanUpAmount(70.0);
    QVERIFY(mask->cleanUpAmount() == 70.0);

    mask->setLimitToDeviceBounds(true);
    QVERIFY(mask->limitToDeviceBounds() == true);

    mask->initializeKeyStrokeColors({&col1, &col2});

    QByteArray pdata1 = QByteArray::fromBase64("//8AAAAAAAAAAP8AAAAAAAAAAAAAAAAAAAAAAAAA");
    QByteArray pdata2 = QByteArray::fromBase64("AAAAAAAAAAD//wAAAAAAAAAAAP8AAAAAAAAAAAAA");

    mask->setKeyStrokePixelData(pdata1,&col1,0,0,10,3);
    mask->setKeyStrokePixelData(pdata2,&col2,0,0,10,3);

    QList<ManagedColor *> checkColors(mask->keyStrokesColors());

    QVERIFY(checkColors.size() == 2);
    QVERIFY(col1.toQString() == checkColors[0]->toQString());
    QVERIFY(col2.toQString() == checkColors[1]->toQString());

    QVERIFY(mask->keyStrokePixelData(&col1,0,0,10,3) == pdata1);
    QVERIFY(mask->keyStrokePixelData(&col2,0,0,10,3) == pdata2);

    delete checkColors[0];
    delete checkColors[1];

    mask->updateMask(true);
    mask->setEditKeyStrokes(false);
    mask->setShowOutput(true);

    d.waitForDone();
    d.refreshProjection();

    QByteArray pdata = d.pixelData(0,0,10,3);

    QVERIFY(d.pixelData(3,2,1,1).toBase64() == "/wAA/w==");
    QVERIFY(d.pixelData(6,2,1,1).toBase64() == "AAD9/w==");

    mask->removeKeyStroke(&col2);
    qApp->processEvents();
    d.waitForDone();

    checkColors = mask->keyStrokesColors();

    QVERIFY(checkColors.size() == 1);
    QVERIFY(col1.toQString() == checkColors[0]->toQString());

    delete checkColors[0];
    delete mask;
    delete node;
    delete rootNode;

    KisDocumentRegistry::instance()->removeDocument(kisdoc.data(), false);
}



void TestDocument::testAnnotations()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(0, 100, 100, KoColorSpaceRegistry::instance()->rgb8(), "test");
    KisNodeSP layer = new KisPaintLayer(image, "test1", 255);
    image->addNode(layer);
    kisdoc->setCurrentImage(image);

    Document d(kisdoc.data(), false);

    QVERIFY(d.annotationTypes().isEmpty());

    QBuffer buf;
    buf.open(QBuffer::WriteOnly);
    QTextStream in(&buf);
    KisPortingUtils::setUtf8OnStream(in);
    in << "AnnotationTest";
    buf.close();

    d.setAnnotation("test", "description", buf.data());

    QVERIFY(d.annotationTypes().size() == 1);
    QVERIFY(d.annotationTypes().contains("test"));
    QVERIFY(d.annotation("test").toHex() == buf.data().toHex());
    QVERIFY(d.annotationDescription("test") == "description");

    d.saveAs("roundtriptest.kra");

    d.removeAnnotation("test");
    QVERIFY(d.annotationTypes().isEmpty());

    d.close();

    Krita *krita = Krita::instance();
    Document *d2 = krita->openDocument("roundtriptest.kra");

    QVERIFY(d2->annotationTypes().size() == 1);
    QVERIFY(d2->annotationTypes().contains("test"));
    QVERIFY(d2->annotation("test").toHex() == buf.data().toHex());
    QVERIFY(d2->annotationDescription("test") == "description");

    d2->close();
}

void TestDocument::testNodeByName()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(0, 100, 100, KoColorSpaceRegistry::instance()->rgb8(), "test");
    KisNodeSP layer = new KisPaintLayer(image, "test1", 255);
    image->addNode(layer);
    kisdoc->setCurrentImage(image);

    Document d(kisdoc.data(), false);

    QVERIFY(d.nodeByName("test1")->name() == layer->name());
    QVERIFY(d.nodeByName("test2") == 0);
}

void TestDocument::testNodeByUniqueId()
{
    QScopedPointer<KisDocument> kisdoc(KisDocumentRegistry::instance()->createDocument());
    KisImageSP image = new KisImage(0, 100, 100, KoColorSpaceRegistry::instance()->rgb8(), "test");
    KisNodeSP layer = new KisPaintLayer(image, "test1", 255);
    image->addNode(layer);
    kisdoc->setCurrentImage(image);

    Document d(kisdoc.data(), false);

    QVERIFY(d.nodeByUniqueID(layer->uuid())->name() == layer->name());

    QUuid test(QUuid::createUuid());

    while (test == layer->uuid()) {
        test = QUuid::createUuid();
    }
    QVERIFY(d.nodeByUniqueID(test) == 0);
}

class KisTransformMaskAdapter;

KISTEST_MAIN(TestDocument)
