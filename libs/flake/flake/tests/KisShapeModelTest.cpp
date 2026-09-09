/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>
#include <QTemporaryFile>

#include <type_traits>

#include <KoColorSpaceRegistry.h>
#include <KoPathShape.h>
#include <KoShapeControllerBase.h>

#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_safe_document_loader.h>
#include <KisReferenceImage.h>
#include <kis_dummies_facade.h>
#include <kis_node_dummies_graph.h>
#include <kis_node_shape.h>
#include <kis_shape_controller.h>
#include <kis_shape_layer.h>
#include <kis_shape_layer_canvas.h>
#include <kis_shape_selection.h>
#include <kis_shape_selection_canvas.h>
#include <kis_shape_selection_model.h>

namespace {

static_assert(std::is_same_v<decltype(&KisDummiesFacade::qt_metacall),
                             decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KisDummiesFacadeBase::qt_metacall),
                             decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KisNodeDummy::qt_metacall),
                             decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KisNodeShape::qt_metacall),
                             decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KisShapeController::qt_metacall),
                             decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KisShapeLayerCanvasBase::qt_metacall),
                             decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KisShapeLayerCanvas::qt_metacall),
                             decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KisShapeSelection::qt_metacall),
                             decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KisShapeSelectionCanvas::qt_metacall),
                             decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KisShapeSelectionModel::qt_metacall),
                             decltype(&QObject::qt_metacall)>);

PkString localToPkString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return PkString::PkFromUtf8(utf8.constData(), utf8.size());
}

class TestShapeController : public KoShapeControllerBase
{
public:
    PkRectF documentRectInPixels() const override
    {
        return PkRectF(0, 0, 64, 64);
    }

    qreal pixelsPerInch() const override
    {
        return 72.0;
    }
};

}

class KisShapeModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void nativeFacadeNotificationsUsePkDelivery();
    void ownsShapeLayerStateWithoutUi();
    void ownsReferenceImageStateWithoutUi();
    void loadsFileThroughInjectedHeadlessLoader();
};

void KisShapeModelTest::nativeFacadeNotificationsUsePkDelivery()
{
    KisDummiesFacade facade;
    PkObject receiver;
    int notifications = 0;
    PkObject::connect(&facade, &KisDummiesFacadeBase::sigEndRemoveDummy,
                      &receiver, [&] { ++notifications; });

    facade.sigEndRemoveDummy();

    QCOMPARE(notifications, 1);
}

void KisShapeModelTest::ownsShapeLayerStateWithoutUi()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(nullptr, 64, 64, colorSpace, PkString("shape-model"));
    TestShapeController controller;
    KisShapeLayer layer(&controller, image, PkString("vectors"), 255);

    QVERIFY(layer.shapeManager());
    QVERIFY(layer.antialiased());

    auto *path = new KoPathShape();
    path->moveTo(PkPointF(4, 4));
    path->lineTo(PkPointF(20, 4));
    path->lineTo(PkPointF(20, 20));
    path->close();
    layer.addShape(path);

    QCOMPARE(layer.shapes().size(), 1);
    QCOMPARE(layer.shapes().first(), path);

    layer.setAntialiased(false);
    QVERIFY(!layer.antialiased());
}

void KisShapeModelTest::ownsReferenceImageStateWithoutUi()
{
    KisReferenceImage first;
    KisReferenceImage second;
    first.setSaturation(0.25);
    second.setSaturation(0.75);

    KisReferenceImage::SetSaturationCommand command({&first, &second}, 0.5);
    command.redo();
    QCOMPARE(first.saturation(), 0.5);
    QCOMPARE(second.saturation(), 0.5);

    command.undo();
    QCOMPARE(first.saturation(), 0.25);
    QCOMPARE(second.saturation(), 0.75);
}

void KisShapeModelTest::loadsFileThroughInjectedHeadlessLoader()
{
    QTemporaryFile file(QStringLiteral("safe-loader-model-XXXXXX.png"));
    QVERIFY(file.open());
    QCOMPARE(file.write("not-an-image"), 12);
    file.flush();

    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP expectedDevice = new KisPaintDevice(colorSpace);

    int loadCount = 0;
    PkString loadedPath;
    KisSafeDocumentLoader loader(
        localToPkString(file.fileName()),
        [&](const PkString &path) {
            ++loadCount;
            loadedPath = path;
            return KisSafeDocumentLoader::LoadResult {
                expectedDevice,
                2.0,
                3.0,
                PkSize(17, 19),
            };
        });

    KisPaintDeviceSP loadedDevice;
    qreal loadedXRes = 0.0;
    qreal loadedYRes = 0.0;
    PkSize loadedSize;
    PkObject receiver;
    PkObject::connect(&loader,
            &KisSafeDocumentLoader::loadingFinished,
            &receiver,
            [&](KisPaintDeviceSP device, qreal xRes, qreal yRes, PkSize size) {
                loadedDevice = device;
                loadedXRes = xRes;
                loadedYRes = yRes;
                loadedSize = size;
            });

    loader.reloadImage();

    QCOMPARE(loadCount, 1);
    QVERIFY(!loadedPath.isEmpty());
    QVERIFY(loadedPath != localToPkString(file.fileName()));
    QCOMPARE(loadedDevice, expectedDevice);
    QCOMPARE(loadedXRes, 2.0);
    QCOMPARE(loadedYRes, 3.0);
    QCOMPARE(loadedSize, PkSize(17, 19));
}

QTEST_MAIN(KisShapeModelTest)

#include "KisShapeModelTest.moc"
