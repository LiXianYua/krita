/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisAsyncColorSamplerHelperTest.h"

#include <optional>
#include <utility>

#include <KConfigGroup>
#include <KSharedConfig>

#include <KoCanvasResourceProvider.h>
#include <KoCanvasResourcesIds.h>
#include <KoColorSpaceRegistry.h>
#include <KoZoomHandler.h>
#include <simpletest.h>
#include <tests/MockShapes.h>

#include "KisAsyncColorSamplerHelper.h"
#include "KisCanvasFeedback.h"
#include "KisColorSamplingCanvas.h"
#include "kis_image.h"
#include "kis_paint_layer.h"

namespace {
class TestSamplingCanvas final : public MockCanvas,
                                 public KisColorSamplingCanvas,
                                 public KisCanvasFeedback
{
public:
    explicit TestSamplingCanvas(KisImageSP image)
        : m_image(image)
    {
        m_converter.setResolution(1.0, 1.0);
        m_converter.setZoomedResolution(1.0, 1.0);
    }

    KisImageWSP samplingImage() const override
    {
        return m_image;
    }

    std::optional<KoColor>
        sampleVisibleReferenceColor(const QPoint &imagePoint) const override
    {
        ++referenceSampleCount;
        lastReferencePoint = imagePoint;
        return referenceColor;
    }

    QColor samplingPreviewColor(const KoColor &color) const override
    {
        ++previewConversionCount;
        return color.toQColor();
    }

    qreal samplingCanvasRotation() const override
    {
        ++rotationQueryCount;
        return rotation;
    }

    bool samplingCanvasMirroredHorizontally() const override
    {
        ++horizontalMirrorQueryCount;
        return horizontalMirror;
    }

    bool samplingCanvasMirroredVertically() const override
    {
        ++verticalMirrorQueryCount;
        return verticalMirror;
    }

    QCursor samplingCursor(bool sampleCurrentLayer,
                           bool pickFgColor) const override
    {
        ++cursorQueryCount;
        lastCursorSampleCurrentLayer = sampleCurrentLayer;
        lastCursorPickFgColor = pickFgColor;
        return QCursor(Qt::WaitCursor);
    }

    const KoViewConverter *viewConverter() const override
    {
        return &m_converter;
    }

    KoViewConverter *viewConverter() override
    {
        return &m_converter;
    }

    void showFloatingMessage(const QString &,
                             const QIcon &,
                             int,
                             Priority,
                             int) override
    {
        ++feedbackCount;
    }

    KisImageSP m_image;
    mutable KoZoomHandler m_converter;
    std::optional<KoColor> referenceColor;
    qreal rotation {0.0};
    bool horizontalMirror {false};
    bool verticalMirror {false};
    mutable int referenceSampleCount {0};
    mutable int previewConversionCount {0};
    mutable int rotationQueryCount {0};
    mutable int horizontalMirrorQueryCount {0};
    mutable int verticalMirrorQueryCount {0};
    mutable int cursorQueryCount {0};
    mutable bool lastCursorSampleCurrentLayer {false};
    mutable bool lastCursorPickFgColor {false};
    mutable QPoint lastReferencePoint;
    int feedbackCount {0};
};

class IntegerConfigEntryGuard
{
public:
    IntegerConfigEntryGuard(KConfigGroup group, QString key, int fallback)
        : m_group(std::move(group))
        , m_key(std::move(key))
        , m_hadEntry(m_group.hasKey(m_key))
        , m_oldValue(m_group.readEntry(m_key, fallback))
    {
    }

    ~IntegerConfigEntryGuard()
    {
        if (m_hadEntry) {
            m_group.writeEntry(m_key, m_oldValue);
        } else {
            m_group.deleteEntry(m_key);
        }
    }

private:
    KConfigGroup m_group;
    QString m_key;
    bool m_hadEntry;
    int m_oldValue;
};

KisImageSP createImageWithLayer(const QColor &color, KisPaintLayerSP *layer)
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(nullptr, 8, 8, colorSpace, "sampler test");
    *layer = new KisPaintLayer(image, "paint", OPACITY_OPAQUE_U8);
    (*layer)->paintDevice()->setPixel(2, 3, color);
    image->addNode(*layer);
    return image;
}

void setCurrentNode(TestSamplingCanvas &canvas, KisNodeSP node)
{
    QVariant value;
    value.setValue(KisNodeWSP(node));
    canvas.resourceManager()->setResource(KoCanvasResource::CurrentKritaNode,
                                          value);
}

bool invokeSamplingJob(KisAsyncColorSamplerHelper &helper)
{
    return QMetaObject::invokeMethod(&helper,
                                     "slotAddSamplingJob",
                                     Qt::DirectConnection,
                                     Q_ARG(QPointF, QPointF(2, 3)));
}
}

void KisAsyncColorSamplerHelperTest::referenceColorShortCircuitsDeviceSampling()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Qt::green, &layer);
    TestSamplingCanvas canvas(image);
    canvas.referenceColor = KoColor(Qt::red, image->colorSpace());
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Qt::black, image->colorSpace()));

    QList<KoColor> sampledColors;
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    helper.setUpdateGlobalColor(false);
    connect(&helper,
            &KisAsyncColorSamplerHelper::sigRawColorSelected,
            this,
            [&sampledColors](const KoColor &color) {
                sampledColors.append(color);
            });

    helper.activate(false, true);
    helper.startAction(QPointF(2, 3), 1, 100);
    sampledColors.clear();
    canvas.referenceSampleCount = 0;
    QVERIFY(invokeSamplingJob(helper));
    helper.endAction();
    image->waitForDone();
    QTest::qWait(120);

    QCOMPARE(canvas.referenceSampleCount, 1);
    QCOMPARE(canvas.lastReferencePoint, QPoint(2, 3));
    QCOMPARE(sampledColors.size(), 1);
    QCOMPARE(sampledColors.first().toQColor(), QColor(Qt::red));
}

void KisAsyncColorSamplerHelperTest::missingReferenceFallsBackToProjection()
{
    KisPaintLayerSP visibleLayer;
    KisImageSP image = createImageWithLayer(Qt::green, &visibleLayer);
    KisPaintLayerSP hiddenCurrentLayer =
        new KisPaintLayer(image, "hidden current", OPACITY_OPAQUE_U8);
    hiddenCurrentLayer->paintDevice()->setPixel(2, 3, QColor(Qt::red));
    hiddenCurrentLayer->setVisible(false);
    image->addNode(hiddenCurrentLayer);
    image->initialRefreshGraph();
    image->waitForDone();

    TestSamplingCanvas canvas(image);
    setCurrentNode(canvas, hiddenCurrentLayer);
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Qt::black, image->colorSpace()));

    QList<KoColor> sampledColors;
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    helper.setUpdateGlobalColor(false);
    connect(&helper,
            &KisAsyncColorSamplerHelper::sigRawColorSelected,
            this,
            [&sampledColors](const KoColor &color) {
                sampledColors.append(color);
            });

    helper.activate(false, true);
    helper.startAction(QPointF(2, 3), 1, 100);
    sampledColors.clear();
    canvas.referenceSampleCount = 0;
    QVERIFY(invokeSamplingJob(helper));
    helper.endAction();
    image->waitForDone();
    QTRY_COMPARE(sampledColors.size(), 2);

    QCOMPARE(canvas.referenceSampleCount, 1);
    QCOMPARE(sampledColors.last().toQColor(), QColor(Qt::green));
}

void KisAsyncColorSamplerHelperTest::delayedJobReadsTheCurrentNodeAgain()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(nullptr, 8, 8, colorSpace, "sampler test");
    KisPaintLayerSP firstLayer =
        new KisPaintLayer(image, "first", OPACITY_OPAQUE_U8);
    KisPaintLayerSP secondLayer =
        new KisPaintLayer(image, "second", OPACITY_OPAQUE_U8);
    firstLayer->paintDevice()->setPixel(2, 3, QColor(Qt::red));
    secondLayer->paintDevice()->setPixel(2, 3, QColor(Qt::blue));
    image->addNode(firstLayer);
    image->addNode(secondLayer);

    TestSamplingCanvas canvas(image);
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Qt::black, colorSpace));
    setCurrentNode(canvas, firstLayer);

    QList<KoColor> sampledColors;
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    helper.setUpdateGlobalColor(false);
    connect(&helper,
            &KisAsyncColorSamplerHelper::sigRawColorSelected,
            this,
            [&sampledColors](const KoColor &color) {
                sampledColors.append(color);
            });

    helper.activate(true, true);
    helper.startAction(QPointF(2, 3), 1, 100);
    sampledColors.clear();
    setCurrentNode(canvas, secondLayer);
    QVERIFY(invokeSamplingJob(helper));
    helper.endAction();
    image->waitForDone();
    QTRY_COMPARE(sampledColors.size(), 2);

    QCOMPARE(canvas.referenceSampleCount, 0);
    QCOMPARE(sampledColors.first().toQColor(), QColor(Qt::red));
    QCOMPARE(sampledColors.last().toQColor(), QColor(Qt::blue));
}

void KisAsyncColorSamplerHelperTest::previewUsesSamplingCanvasGeometry()
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group("");
    const QString key = QStringLiteral("colorSamplerPreviewStyle");
    const IntegerConfigEntryGuard styleGuard(cfg, key, 1);
    cfg.writeEntry(key, 2); // RectangleLeft

    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Qt::black, &layer);
    TestSamplingCanvas canvas(image);
    canvas.rotation = 90.0;
    canvas.horizontalMirror = true;
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Qt::black, image->colorSpace()));

    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    helper.activate(false, true);
    QVERIFY(QMetaObject::invokeMethod(&helper,
                                      "activateDelayedPreview",
                                      Qt::DirectConnection));
    const QRectF previewRect = helper.colorPreviewDocRect(QPointF(10, 20));
    helper.deactivate();

    QCOMPARE(previewRect, QRectF(-70, 52, 48, 48));
    QVERIFY(canvas.previewConversionCount > 0);
    QVERIFY(canvas.rotationQueryCount > 0);
    QVERIFY(canvas.horizontalMirrorQueryCount > 0);
    QVERIFY(canvas.verticalMirrorQueryCount > 0);

}

void KisAsyncColorSamplerHelperTest::cursorUsesSamplingCanvasPolicy()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Qt::black, &layer);
    TestSamplingCanvas canvas(image);
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);

    QCursor requestedCursor;
    connect(&helper,
            &KisAsyncColorSamplerHelper::sigRequestCursor,
            this,
            [&requestedCursor](const QCursor &cursor) {
                requestedCursor = cursor;
            });

    helper.updateCursor(true, false);

    QCOMPARE(canvas.cursorQueryCount, 1);
    QVERIFY(canvas.lastCursorSampleCurrentLayer);
    QVERIFY(!canvas.lastCursorPickFgColor);
    QCOMPARE(requestedCursor.shape(), Qt::WaitCursor);
}

SIMPLE_TEST_MAIN(KisAsyncColorSamplerHelperTest)
