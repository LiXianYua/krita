/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>

#include <KoColorSpaceRegistry.h>
#include <KisDocument.h>
#include <PkVersionNumber.h>
#include <PkXmlDocument.h>
#include <kis_kra_loader.h>
#include <kis_kra_savexml_visitor.h>
#include <kis_file_layer.h>
#include <kis_image.h>

namespace {

class TestDocument : public KisDocument
{
public:
    TestDocument()
        : KisDocument(false)
    {
    }
};

}

class KisFileLayerRoundTripTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void savesAndLoadsPkFileLayerFields();
};

void KisFileLayerRoundTripTest::savesAndLoadsPkFileLayerFields()
{
    const PkString sourceName("missing-file-layer-source.png");
    const PkString layerName("linked-file-layer");
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();

    KisImageSP image = new KisImage(nullptr, 32, 24,
                                    colorSpace,
                                    PkString("file-layer-roundtrip"));
    KisFileLayerSP fileLayer = new KisFileLayer(
        image,
        PkString("/tmp"),
        sourceName,
        KisFileLayer::ToImageSize,
        PkString("Bilinear"),
        layerName,
        203);
    fileLayer->setCompositeOpId("normal");
    image->addNode(fileLayer, image->root(), KisNodeSP());
    PkXmlDocument document;
    PkXmlElement root = document.createElement("root");
    document.appendChild(root);
    quint32 nodeCount = 0;
    KisSaveXmlVisitor saver(document, root, nodeCount, PkString("/tmp/document.kra"), false);
    QVERIFY(saver.visit(static_cast<KisExternalLayer *>(fileLayer.data())));
    QCOMPARE(nodeCount, quint32(1));

    const PkXmlElement layerElement = root.firstChildElement("layer");
    QVERIFY(!layerElement.isNull());
    TestDocument ownerDocument;
    ownerDocument.setPath("/tmp/document.kra");
    KisKraLoader loader(&ownerDocument, 0, PkVersionNumber());
    KisNodeSP loadedNode = loader.loadFileLayer(
        layerElement, image, layerName, 203, colorSpace);
    QVERIFY(loadedNode);
    KisFileLayer *loadedLayer = dynamic_cast<KisFileLayer *>(loadedNode.data());
    QVERIFY(loadedLayer);
    QVERIFY(loadedLayer->fileName() == sourceName);
    QCOMPARE(loadedLayer->scalingMethod(), KisFileLayer::ToImageSize);
    QVERIFY(loadedLayer->scalingFilter() == PkString("Bilinear"));
    QVERIFY(loadedLayer->name() == layerName);
    QCOMPARE(loadedLayer->opacity(), quint8(203));
}

QTEST_GUILESS_MAIN(KisFileLayerRoundTripTest)

#include "kis_file_layer_roundtrip_test.moc"
