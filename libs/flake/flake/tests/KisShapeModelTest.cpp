/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>
#include <QTemporaryFile>

#include <KoColorSpaceRegistry.h>
#include <KoPathShape.h>
#include <KoShapeControllerBase.h>

#include <kis_image.h>
#include <kis_paint_device.h>
#include <kis_safe_document_loader.h>
#include <KisReferenceImage.h>
#include <kis_shape_layer.h>

namespace {

class TestShapeController : public KoShapeControllerBase
{
public:
    QRectF documentRectInPixels() const override
    {
        return QRectF(0, 0, 64, 64);
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
    void ownsShapeLayerStateWithoutUi();
    void ownsReferenceImageStateWithoutUi();
    void loadsFileThroughInjectedHeadlessLoader();
};

void KisShapeModelTest::ownsShapeLayerStateWithoutUi()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(nullptr, 64, 64, colorSpace, QStringLiteral("shape-model"));
    TestShapeController controller;
    KisShapeLayer layer(&controller, image, QStringLiteral("vectors"), 255);

    QVERIFY(layer.shapeManager());
    QVERIFY(layer.antialiased());

    auto *path = new KoPathShape();
    path->moveTo(QPointF(4, 4));
    path->lineTo(QPointF(20, 4));
    path->lineTo(QPointF(20, 20));
    path->close();
    layer.addShape(path);

    QCOMPARE(layer.shapes().size(), 1);
    QCOMPARE(layer.shapes().constFirst(), path);

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
    QString loadedPath;
    KisSafeDocumentLoader loader(
        file.fileName(),
        [&](const QString &path) {
            ++loadCount;
            loadedPath = path;
            return KisSafeDocumentLoader::LoadResult {
                expectedDevice,
                2.0,
                3.0,
                QSize(17, 19),
            };
        });

    KisPaintDeviceSP loadedDevice;
    qreal loadedXRes = 0.0;
    qreal loadedYRes = 0.0;
    QSize loadedSize;
    connect(&loader,
            &KisSafeDocumentLoader::loadingFinished,
            this,
            [&](KisPaintDeviceSP device, qreal xRes, qreal yRes, const QSize &size) {
                loadedDevice = device;
                loadedXRes = xRes;
                loadedYRes = yRes;
                loadedSize = size;
            });

    loader.reloadImage();

    QCOMPARE(loadCount, 1);
    QVERIFY(!loadedPath.isEmpty());
    QVERIFY(loadedPath != file.fileName());
    QCOMPARE(loadedDevice, expectedDevice);
    QCOMPARE(loadedXRes, 2.0);
    QCOMPARE(loadedYRes, 3.0);
    QCOMPARE(loadedSize, QSize(17, 19));
}

QTEST_MAIN(KisShapeModelTest)

#include "KisShapeModelTest.moc"
