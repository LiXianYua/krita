/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_file_layer_test.h"

#include <QImage>
#include <simpletest.h>

#include <KoColorSpaceRegistry.h>

#include <kis_file_layer.h>
#include <kis_safe_document_loader.h>
#include <kis_transform_mask.h>
#include <kis_transform_mask_params_interface.h>

#include <pk/geometry/PkRect.h>
#include <pk/geometry/PkTransform.h>
#include <PkImage.h>
#include <cstdint>
#include <cstring>
#include <vector>

#include <testutil.h>
#include <kistest.h>

#include "KritaTransformMaskStubs.h"
#include "KisDumbTransformMaskParams.h"

#include "config-limit-long-tests.h"

namespace {

// 真 Qt QImage -> PkImage 桥接（同 libs/canvas 匿名命名空间里的 toPkImage，
// 此处因测试只链 kritaimage/kritatestsdk、够不到 canvas 而本地复刻）。
PkImage toPkImage(const QImage &image)
{
    PkImage result(image.width(), image.height(),
                   static_cast<PkImage::Format>(image.format()));
    for (int y = 0; y < image.height(); ++y) {
        std::memcpy(result.scanLine(y), image.constScanLine(y),
                    static_cast<std::size_t>(image.bytesPerLine()));
    }
    if (image.colorCount() > 0) {
        std::vector<std::uint32_t> colorTable;
        colorTable.reserve(static_cast<std::size_t>(image.colorCount()));
        for (int i = 0; i < image.colorCount(); ++i) {
            colorTable.push_back(static_cast<std::uint32_t>(image.color(i)));
        }
        result.setColorTable(colorTable);
    }
    return result;
}

KisSafeDocumentLoader::LoadResult loadImage(const QString &path)
{
    const QImage image(path);
    if (image.isNull()) {
        return {};
    }

    KisPaintDeviceSP device(new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8()));
    device->convertFromQImage(toPkImage(image), 0);

    return {device, 1.0, 1.0, image.size()};
}

void waitForMaskUpdates(KisNodeSP root)
{
#ifdef LIMIT_LONG_TESTS
    KisLayerUtils::forceAllDelayedNodesUpdate(root);
    QTest::qWait(100);
#else
    Q_UNUSED(root);
    QTest::qWait(100);
#endif
}

}

void KisFileLayerTest::initTestCase()
{
    TestUtil::registerTransformMaskStubs();
    KisSafeDocumentLoader::setDefaultImageLoader(loadImage);
}

void KisFileLayerTest::cleanupTestCase()
{
    KisSafeDocumentLoader::setDefaultImageLoader({});
}

void KisFileLayerTest::testFileLayerPlusTransformMaskOffImage()
{
    TestUtil::ReferenceImageChecker chk("flayer_tmask_offimage", "file_layer");

    const QRect refRect(0, 0, 640, 441);
    TestUtil::MaskParent p(refRect);

    const QString refName(TestUtil::fetchDataFileLazy("hakonepa.png"));
    KisLayerSP flayer = new KisFileLayer(p.image, "", refName, KisFileLayer::None,
                                        "Bicubic", "flayer", OPACITY_OPAQUE_U8);
    p.image->addNode(flayer, p.image->root(), KisNodeSP());

    waitForMaskUpdates(p.image->root());
    p.image->waitForDone();

    KisTransformMaskSP mask1 = new KisTransformMask(p.image, "mask1");
    p.image->addNode(mask1, flayer);

    flayer->setDirty(PkRect(refRect.x(), refRect.y(), refRect.width(), refRect.height()));
    p.image->waitForDone();
    chk.checkImage(p.image, "00_initial_layer_update");

    waitForMaskUpdates(p.image->root());
    p.image->waitForDone();
    chk.checkImage(p.image, "00X_initial_layer_update");

    flayer->setX(580);
    flayer->setY(400);
    flayer->setDirty(PkRect(refRect.x(), refRect.y(), refRect.width(), refRect.height()));
    p.image->waitForDone();
    chk.checkImage(p.image, "01_file_layer_moved");

    waitForMaskUpdates(p.image->root());
    p.image->waitForDone();
    chk.checkImage(p.image, "01X_file_layer_moved");

    const PkTransform transform = PkTransform::fromTranslate(-580, -400);
    mask1->setTransformParams(KisTransformMaskParamsInterfaceSP(
        new KisDumbTransformMaskParams(transform)));

    mask1->setDirty(PkRect(refRect.x(), refRect.y(), refRect.width(), refRect.height()));
    p.image->waitForDone();
    chk.checkImage(p.image, "02_mask1_moved_mask_update");

    waitForMaskUpdates(p.image->root());
    p.image->waitForDone();
    chk.checkImage(p.image, "02X_mask1_moved_mask_update");

    QVERIFY(chk.testPassed());
}

void KisFileLayerTest::testFileLayerPlusTransformMaskSmallFileBigOffset()
{
    TestUtil::ReferenceImageChecker chk("flayer_tmask_huge_offset", "file_layer");

    const QRect refRect(0, 0, 2000, 1500);
    TestUtil::MaskParent p(refRect);

    const QString refName = QStringLiteral(FILES_DATA_DIR "../../animation/tests/data/file_layer_source.png");
    KisLayerSP flayer = new KisFileLayer(p.image, "", refName, KisFileLayer::None,
                                        "Bicubic", "flayer", OPACITY_OPAQUE_U8);
    p.image->addNode(flayer, p.image->root(), KisNodeSP());

    waitForMaskUpdates(p.image->root());
    p.image->waitForDone();
    QCOMPARE(flayer->original()->defaultBounds()->bounds(), p.image->bounds());

    KisTransformMaskSP mask1 = new KisTransformMask(p.image, "mask1");
    p.image->addNode(mask1, flayer);

    flayer->setDirty(PkRect(refRect.x(), refRect.y(), refRect.width(), refRect.height()));
    p.image->waitForDone();
    chk.checkImage(p.image, "00_initial_layer_update");

    waitForMaskUpdates(p.image->root());
    p.image->waitForDone();
    chk.checkImage(p.image, "00X_initial_layer_update");

    const PkTransform transform = PkTransform::fromTranslate(1200, 300);
    mask1->setTransformParams(KisTransformMaskParamsInterfaceSP(
        new KisDumbTransformMaskParams(transform)));

    mask1->setDirty(PkRect(refRect.x(), refRect.y(), refRect.width(), refRect.height()));
    p.image->waitForDone();
    chk.checkImage(p.image, "01_mask1_moved_mask_update");

    waitForMaskUpdates(p.image->root());
    p.image->waitForDone();
    chk.checkImage(p.image, "01X_mask1_moved_mask_update");

    QVERIFY(chk.testPassed());
}

KISTEST_MAIN(KisFileLayerTest)
