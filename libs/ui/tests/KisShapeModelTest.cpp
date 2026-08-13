/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>

#include <KoColorSpaceRegistry.h>
#include <KoPathShape.h>
#include <KoShapeControllerBase.h>

#include <kis_image.h>
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

QTEST_MAIN(KisShapeModelTest)

#include "KisShapeModelTest.moc"
