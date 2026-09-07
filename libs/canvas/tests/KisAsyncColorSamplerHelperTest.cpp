/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkFlakeBridge.h>
#include <pk/render/PkPaintCommand.h>
#include "KisAsyncColorSamplerHelperTest.h"

#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include <KConfigGroup>
#include <KSharedConfig>

#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoCanvasResourcesIds.h>
#include <KoColorSpaceRegistry.h>
#include <KoUnit.h>
#include <KoZoomHandler.h>
#include <simpletest.h>

#include "KisAsyncColorSamplerHelper.h"
#include "KisCanvasFeedback.h"
#include "KisColorSamplingCanvas.h"
#include "kis_image.h"
#include "kis_paint_layer.h"

Q_DECLARE_METATYPE(KoColor)

namespace {
class TestSamplingCanvas final : public KoCanvasBase,
                                 public KisColorSamplingCanvas,
                                 public KisCanvasFeedback
{
public:
    explicit TestSamplingCanvas(KisImageSP image)
        : KoCanvasBase(nullptr)
        , m_image(image)
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
        return toQColor(color.toQColor());
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

    void gridSize(PkPointF *, PkSizeF *) const override {}
    bool snapToGrid() const override { return false; }
    void setCursor(const QCursor &) override {}
    void addCommand(KUndo2Command *) override {}
    KoShapeManager *shapeManager() const override { return nullptr; }
    KoSelectedShapesProxy *selectedShapesProxy() const override { return nullptr; }
    void updateCanvas(const PkRectF &) override {}
    KoToolProxy *toolProxy() const override { return nullptr; }
    QWidget *canvasWidget() override { return nullptr; }
    const QWidget *canvasWidget() const override { return nullptr; }
    KoUnit unit() const override { return KoUnit(KoUnit::Millimeter); }

    void showFloatingMessage(const PkString &,
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

class RecordingBackend final : public PkPainterBackend
{
public:
    void submit(const PkPaintCommand &command) override
    {
        commands.push_back(command);
    }

    std::vector<PkPaintCommand> commands;
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

KisImageSP createImageWithLayer(const PkColor &color, KisPaintLayerSP *layer)
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
    canvas.resourceManager()->setResource(KoCanvasResource::CurrentKritaNode,
                                          PkVariant::fromValue(KisNodeWSP(node)));
}

bool invokeSamplingJob(KisAsyncColorSamplerHelper &helper)
{
    return QMetaObject::invokeMethod(&helper,
                                     "slotAddSamplingJob",
                                     Qt::DirectConnection,
                                     Q_ARG(PkPointF, PkPointF(2, 3)));
}
}

void KisAsyncColorSamplerHelperTest::initTestCase()
{
    qRegisterMetaType<KoColor>("KoColor");
}

void KisAsyncColorSamplerHelperTest::referenceColorShortCircuitsDeviceSampling()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::green, &layer);
    TestSamplingCanvas canvas(image);
    canvas.referenceColor = KoColor(Pk::red, image->colorSpace());
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Pk::black, image->colorSpace()));

    QList<KoColor> sampledColors;
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    helper.setUpdateGlobalColor(false);
    QObject::connect(&helper,
            &KisAsyncColorSamplerHelper::sigRawColorSelected,
            this,
            [&sampledColors](const KoColor &color) {
                sampledColors.append(color);
            });

    helper.activate(false, true);
    helper.startAction(PkPointF(2, 3), 1, 100);
    sampledColors.clear();
    canvas.referenceSampleCount = 0;
    QVERIFY(invokeSamplingJob(helper));
    helper.endAction();
    image->waitForDone();
    QTest::qWait(120);

    QCOMPARE(canvas.referenceSampleCount, 1);
    QCOMPARE(canvas.lastReferencePoint, QPoint(2, 3));
    QCOMPARE(sampledColors.size(), 1);
    QCOMPARE(toQColor(sampledColors.first().toQColor()), QColor(Qt::red));
}

void KisAsyncColorSamplerHelperTest::missingReferenceFallsBackToProjection()
{
    KisPaintLayerSP visibleLayer;
    KisImageSP image = createImageWithLayer(Pk::green, &visibleLayer);
    KisPaintLayerSP hiddenCurrentLayer =
        new KisPaintLayer(image, "hidden current", OPACITY_OPAQUE_U8);
    hiddenCurrentLayer->paintDevice()->setPixel(2, 3, Pk::red);
    hiddenCurrentLayer->setVisible(false);
    image->addNode(hiddenCurrentLayer);
    image->initialRefreshGraph();
    image->waitForDone();

    TestSamplingCanvas canvas(image);
    setCurrentNode(canvas, hiddenCurrentLayer);
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Pk::black, image->colorSpace()));

    QList<KoColor> sampledColors;
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    helper.setUpdateGlobalColor(false);
    QObject::connect(&helper,
            &KisAsyncColorSamplerHelper::sigRawColorSelected,
            this,
            [&sampledColors](const KoColor &color) {
                sampledColors.append(color);
            });

    helper.activate(false, true);
    helper.startAction(PkPointF(2, 3), 1, 100);
    sampledColors.clear();
    canvas.referenceSampleCount = 0;
    QVERIFY(invokeSamplingJob(helper));
    helper.endAction();
    image->waitForDone();
    QTRY_COMPARE(sampledColors.size(), 2);

    QCOMPARE(canvas.referenceSampleCount, 1);
    QCOMPARE(toQColor(sampledColors.last().toQColor()), QColor(Qt::green));
}

void KisAsyncColorSamplerHelperTest::delayedJobReadsTheCurrentNodeAgain()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(nullptr, 8, 8, colorSpace, "sampler test");
    KisPaintLayerSP firstLayer =
        new KisPaintLayer(image, "first", OPACITY_OPAQUE_U8);
    KisPaintLayerSP secondLayer =
        new KisPaintLayer(image, "second", OPACITY_OPAQUE_U8);
    firstLayer->paintDevice()->setPixel(2, 3, Pk::red);
    secondLayer->paintDevice()->setPixel(2, 3, Pk::blue);
    image->addNode(firstLayer);
    image->addNode(secondLayer);

    TestSamplingCanvas canvas(image);
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Pk::black, colorSpace));
    setCurrentNode(canvas, firstLayer);
    QCOMPARE(canvas.resourceManager()
                 ->resource(KoCanvasResource::CurrentKritaNode)
                 .value<KisNodeWSP>(),
             KisNodeWSP(firstLayer));

    QList<KoColor> sampledColors;
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    helper.setUpdateGlobalColor(false);
    QObject::connect(&helper,
            &KisAsyncColorSamplerHelper::sigRawColorSelected,
            this,
            [&sampledColors](const KoColor &color) {
                sampledColors.append(color);
            });

    helper.activate(true, true);
    helper.startAction(PkPointF(2, 3), 1, 100);
    sampledColors.clear();
    setCurrentNode(canvas, secondLayer);
    QCOMPARE(canvas.resourceManager()
                 ->resource(KoCanvasResource::CurrentKritaNode)
                 .value<KisNodeWSP>(),
             KisNodeWSP(secondLayer));
    QVERIFY(invokeSamplingJob(helper));
    helper.endAction();
    image->waitForDone();
    QTRY_COMPARE(sampledColors.size(), 2);

    QCOMPARE(canvas.referenceSampleCount, 0);
    QCOMPARE(toQColor(sampledColors.first().toQColor()), QColor(Qt::red));
    QCOMPARE(toQColor(sampledColors.last().toQColor()), QColor(Qt::blue));
}

void KisAsyncColorSamplerHelperTest::previewUsesSamplingCanvasGeometry()
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group("");
    const QString key = QStringLiteral("colorSamplerPreviewStyle");
    const IntegerConfigEntryGuard styleGuard(cfg, key, 1);
    cfg.writeEntry(key, 2); // RectangleLeft

    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    TestSamplingCanvas canvas(image);
    canvas.rotation = 90.0;
    canvas.horizontalMirror = true;
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Pk::black, image->colorSpace()));

    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    helper.activate(false, true);
    QVERIFY(QMetaObject::invokeMethod(&helper,
                                      "activateDelayedPreview",
                                      Qt::DirectConnection));
    const PkRectF previewRect = helper.colorPreviewDocRect(PkPointF(10, 20));
    helper.deactivate();

    QCOMPARE(previewRect, PkRectF(-70, 52, 48, 48));
    QVERIFY(canvas.previewConversionCount > 0);
    QVERIFY(canvas.rotationQueryCount > 0);
    QVERIFY(canvas.horizontalMirrorQueryCount > 0);
    QVERIFY(canvas.verticalMirrorQueryCount > 0);

}

void KisAsyncColorSamplerHelperTest::circlePreviewDoesNotClearDestination()
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group("");
    const QString key = QStringLiteral("colorSamplerPreviewStyle");
    const IntegerConfigEntryGuard styleGuard(cfg, key, 1);
    cfg.writeEntry(key, 1); // Circle

    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    TestSamplingCanvas canvas(image);
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Pk::black, image->colorSpace()));

    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    helper.setUpdateGlobalColor(false);
    helper.activate(false, true);
    QVERIFY(QMetaObject::invokeMethod(&helper,
                                      "activateDelayedPreview",
                                      Qt::DirectConnection));
    QVERIFY(QMetaObject::invokeMethod(&helper,
                                      "slotColorSamplingFinished",
                                      Qt::DirectConnection,
                                      Q_ARG(KoColor, KoColor(Pk::red, image->colorSpace()))));
    helper.colorPreviewDocRect(PkPointF(10, 20));

    RecordingBackend backend;
    PkPainter painter(backend);
    helper.paint(painter, *canvas.viewConverter());
    helper.deactivate();

    const bool clearedDestination = std::any_of(
        backend.commands.cbegin(), backend.commands.cend(),
        [](const PkPaintCommand &command) {
            const auto *composition = std::get_if<PkSetCompositionModeCommand>(&command);
            return composition && composition->mode == Pk::CompositionMode_Clear;
        });
    const bool paintedRing = std::any_of(
        backend.commands.cbegin(), backend.commands.cend(),
        [](const PkPaintCommand &command) {
            return std::holds_alternative<PkFillPathCommand>(command);
        });

    QVERIFY(!clearedDestination);
    QVERIFY(paintedRing);
}

void KisAsyncColorSamplerHelperTest::cursorUsesSamplingCanvasPolicy()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    TestSamplingCanvas canvas(image);
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);

    QCursor requestedCursor;
    QObject::connect(&helper,
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
