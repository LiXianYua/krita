/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkFlakeBridge.h>
#include <pk/render/PkPaintCommand.h>
#include "KisAsyncColorSamplerHelperTest.h"

#include <algorithm>
#include <cmath>
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
#include <KoToolProxy.h>
#include <KoToolProxy_p.h>
#include <KoSnapGuide.h>
#include <KoPointerEvent.h>
#include <simpletest.h>

#include "KisAsyncColorSamplerHelper.h"
#include "KisCanvasFeedback.h"
#include "KisCanvasToolServices.h"
#include "KisColorSamplingCanvas.h"
#include "KisOptimizedBrushOutline.h"
#include "kis_image.h"
#include "kis_paint_layer.h"
#include "tool/kis_tool_ellipse_base.h"
#include "tool/kis_tool_polyline_base.h"
#include "kis_tool_select_polygonal.h"

Q_DECLARE_METATYPE(KoColor)

namespace {
class TestSamplingCanvas : public KoCanvasBase,
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
    explicit RecordingBackend(qreal devicePixelRatio = 1.0)
        : m_devicePixelRatio(devicePixelRatio)
    {
    }

    void submit(const PkPaintCommand &command) override
    {
        commands.push_back(command);
    }

    qreal devicePixelRatio() const override { return m_devicePixelRatio; }

    std::vector<PkPaintCommand> commands;

private:
    qreal m_devicePixelRatio;
};

void verifyPathVertices(
    const PkPainterPath &path,
    std::initializer_list<PkPointF> expectedVertices)
{
    QCOMPARE(path.elementCount(), int(expectedVertices.size()));
    int index = 0;
    for (const PkPointF &expected : expectedVertices) {
        const auto actual = path.elementAt(index++);
        QVERIFY(std::abs(actual.x - expected.x()) < 1e-9);
        QVERIFY(std::abs(actual.y - expected.y()) < 1e-9);
    }
}

class EllipsePreviewCanvas final : public TestSamplingCanvas,
                                   public KisCanvasToolServices
{
public:
    explicit EllipsePreviewCanvas(KisImageSP image = {})
        : TestSamplingCanvas(image)
    {
    }

    KisImageWSP toolImage() const override { return m_image; }
    PkPointF toolWidgetCenterInWidgetPixels() const override { return {}; }
    PkPointF toolDocumentToWidget(const PkPointF &point) const override { return point; }
    PkPointF toolDocumentToAlignedImagePixel(const PkPointF &point) const override { return point; }
    QTransform toolImageToViewTransform() const override { return {}; }
    void drawToolOutline(PkPainter *painter,
                         const KisOptimizedBrushOutline &path,
                         int) override
    {
        for (const PkPolygonF &polygon : path) {
            painter->drawPolygon(polygon);
        }
    }
    bool toolBlockUntilOperationsFinished(KisImageWSP) override { return true; }
    void toolBlockUntilOperationsFinishedForced(KisImageWSP) override {}
    bool toolSelectionEditable() const override { return true; }
    KisCanvasToolSignals *toolSignals() override { return nullptr; }
    KisPaintOpPresetSP toolCurrentPaintOpPreset() const override { return {}; }
    void toolNotifyPaintingFinished() override {}
    void toolSetControlsEnabled(bool) override {}
    KisPopupWidgetInterface *toolPopupWidget() const override { return nullptr; }
    PkSize toolCanvasWidgetSize() const override { return {}; }
    PkRect toolAvailableVirtualScreenGeometry() const override { return {}; }
    qreal toolImageScaleX() const override { return 1.0; }
    PkPointF toolImageToDocument(const PkPointF &point) const override { return point; }
    qreal toolCanvasRotation() const override { return 0.0; }
    bool toolCanvasMirroredHorizontally() const override { return false; }
    bool toolCanvasMirroredVertically() const override { return false; }
    qreal toolEffectiveZoom() const override { return 1.0; }
    qreal toolCoordinateEffectiveZoom() const override { return 1.0; }
    qreal toolEffectivePhysicalZoom() const override { return 1.0; }
    QCursor toolCursor(CursorStyle) const override { return {}; }
    QCursor toolMoveCursor() const override { return {}; }
    QCursor toolMoveSelectionCursor() const override { return {}; }
    QCursor toolSamplerCursor() const override { return {}; }
    QCursor toolOpenHandCursor() const override { return {}; }
    QCursor toolClosedHandCursor() const override { return {}; }
    QCursor toolLoadCursor(const PkString &, int, int) const override { return {}; }
    void toolSetCursorPosition(const PkPoint &) override {}
    void toolShowBrushSize(qreal) override {}
    void toolShowLockedLayerMessage(bool) override {}
    void toolShowFloatingMessage(const PkString &, bool) override {}
    PkString toolNodeEditableMessage(KisNodeSP, bool) const override { return {}; }
    QPainterPath toolShapeHoverInfoCrossLayer(const PkPointF &,
                                              PkString &,
                                              bool *,
                                              bool) const override { return {}; }
    bool toolSelectShapeCrossLayer(const PkPointF &,
                                   const PkString &,
                                   bool) override { return false; }
    void toolUpdateCanvas() override {}
    void toolSetPriorityEventFilter(QObject *, bool) override {}
    KisInputActionGroupsMaskInterface::SharedInterface
        toolInputActionGroupsMaskInterface() override { return {}; }
    void toolUpdateAssistantDecoration() override {}
    void toolUpdateOutlineDoc(const PkRectF &) override {}
    PkPointF toolAdjustAssistantPosition(const PkPointF &point,
                                         const PkPointF &,
                                         qreal,
                                         bool,
                                         bool) override { return point; }
    qreal toolAssistantPerspective(const PkPointF &) const override { return 1.0; }
    void toolEndAssistantStroke() override {}
};

class EllipsePreviewTool final : public KisToolEllipseBase
{
public:
    explicit EllipsePreviewTool(KoCanvasBase *canvas)
        : KisToolEllipseBase(canvas, SELECT, QCursor())
    {
    }

    void paintPreview(PkPainter &painter, const PkRectF &rect)
    {
        paintRectangle(painter, rect);
    }

    void setPreviewAngle(qreal angle) { m_angle = angle; }

private:
    void finishRect(const PkRectF &, qreal, qreal) override {}
};

class DecorationProxy final : public KoToolProxy
{
public:
    using KoToolProxy::KoToolProxy;
protected:
    PkPointF widgetToDocument(const PkPointF &point) const override { return point; }
    PkPointF documentToWidget(const PkPointF &point) const override { return point; }
};

class PolylinePreviewTool final : public KisToolPolylineBase
{
public:
    explicit PolylinePreviewTool(KoCanvasBase *canvas)
        : KisToolPolylineBase(canvas, SELECT, QCursor()) {}
protected:
    void finishPolyline(const PkVector<PkPointF> &) override {}
};

class SelectionPreviewTool final : public __KisToolSelectPolygonalLocal
{
public:
    using __KisToolSelectPolygonalLocal::__KisToolSelectPolygonalLocal;
protected:
    void finishPolyline(const PkVector<PkPointF> &) override {}
};

class SamplingPreviewTool final : public KisToolPaint
{
public:
    explicit SamplingPreviewTool(KoCanvasBase *canvas)
        : KisToolPaint(canvas, QCursor()) { setSupportOutline(true); }
    using KisToolPaint::activateAlternateAction;
    using KisToolPaint::deactivateAlternateAction;
    using KisToolPaint::beginAlternateAction;
    using KisToolPaint::endAlternateAction;
protected:
    // Brush shape generation is irrelevant to this sampler test. Painting remains
    // the production KisToolPaint override, including its real sampler member.
    KisOptimizedBrushOutline getOutlinePath(const PkPointF &, const KoPointerEvent *,
                                             KisPaintOpSettings::OutlineMode) override { return {}; }
};

template <typename T>
class ConfigEntryGuard
{
public:
    ConfigEntryGuard(KConfigGroup group, QString key, T fallback)
        : m_group(std::move(group))
        , m_key(std::move(key))
        , m_hadEntry(m_group.hasKey(m_key))
        , m_oldValue(m_group.readEntry(m_key, fallback))
    {
    }

    ~ConfigEntryGuard()
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
    T m_oldValue;
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
    const ConfigEntryGuard<int> styleGuard(cfg, key, 1);
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

void KisAsyncColorSamplerHelperTest::rectanglePreviewPreservesCommandsAndState()
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group("");
    const QString styleKey = QStringLiteral("colorSamplerPreviewStyle");
    const ConfigEntryGuard<int> styleGuard(cfg, styleKey, 1);
    cfg.writeEntry(styleKey, 2); // RectangleLeft

    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    TestSamplingCanvas canvas(image);
    canvas.rotation = 30.0;
    canvas.horizontalMirror = true;
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Pk::green, image->colorSpace()));

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
    const PkRectF viewRect = helper.colorPreviewDocRect(PkPointF(10.25, 20.75));

    RecordingBackend backend(2.0);
    PkPainter painter(backend);
    helper.paint(painter, *canvas.viewConverter());
    helper.deactivate();

    QCOMPARE(backend.commands.size(), std::size_t(5));
    QVERIFY(std::holds_alternative<PkSaveCommand>(backend.commands.front()));
    const auto *hint = std::get_if<PkSetRenderHintCommand>(&backend.commands[1]);
    QVERIFY(hint);
    QCOMPARE(hint->hint, unsigned(PkPainter::Antialiasing));
    QVERIFY(hint->enabled);
    const auto *currentFill = std::get_if<PkFillPathCommand>(&backend.commands[2]);
    const auto *baseFill = std::get_if<PkFillPathCommand>(&backend.commands[3]);
    QVERIFY(currentFill);
    QVERIFY(baseFill);
    QCOMPARE(currentFill->brush.color(), PkColor(Pk::red));
    QCOMPARE(baseFill->brush.color(), PkColor(Pk::green));
    QVERIFY(std::holds_alternative<PkRestoreCommand>(backend.commands.back()));
    QVERIFY(!painter.testRenderHint(PkPainter::Antialiasing));

    QCOMPARE(viewRect.toRect(), PkRect(-2, 64, 107, 90));
    // Literal vertices from the old DPR=2 raster cache: its mirrored current
    // half spans physical x=[0,96], while the base half spans x=[-96,1].
    // The shared [0,1] interval is the original one-device-pixel overlap.
    verifyPathVertices(
        currentFill->path,
        {PkPointF(63.444186046511632, 88.215390309173472),
         PkPointF(104.820060221738387, 112.215390309173472),
         PkPointF(80.931688128715138, 153.784609690826528),
         PkPointF(39.555813953488375, 129.784609690826528),
         PkPointF(63.444186046511632, 88.215390309173472)});
    verifyPathVertices(
        baseFill->path,
        {PkPointF(22.068311871284866, 64.215390309173472),
         PkPointF(63.875184735836896, 88.465390309173472),
         PkPointF(39.986812642813653, 130.034609690826528),
         PkPointF(-1.820060221738391, 105.784609690826528),
         PkPointF(22.068311871284866, 64.215390309173472)});
}

void KisAsyncColorSamplerHelperTest::circlePreviewPreservesRingCommandsAndState()
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group("");
    const QString styleKey = QStringLiteral("colorSamplerPreviewStyle");
    const QString diameterKey = QStringLiteral("colorSamplerPreviewCircleDiameter");
    const QString thicknessKey = QStringLiteral("colorSamplerPreviewCircleThickness");
    const QString outlineKey = QStringLiteral("colorSamplerPreviewCircleOutlineEnabled");
    const QString extraKey = QStringLiteral("colorSamplerPreviewCircleExtraCirclesEnabled");
    const ConfigEntryGuard<int> styleGuard(cfg, styleKey, 1);
    const ConfigEntryGuard<int> diameterGuard(cfg, diameterKey, 180);
    const ConfigEntryGuard<qreal> thicknessGuard(cfg, thicknessKey, 12.0);
    const ConfigEntryGuard<bool> outlineGuard(cfg, outlineKey, true);
    const ConfigEntryGuard<bool> extraGuard(cfg, extraKey, true);
    cfg.writeEntry(styleKey, 1); // Circle
    cfg.writeEntry(diameterKey, 180);
    cfg.writeEntry(thicknessKey, qreal(25));
    cfg.writeEntry(outlineKey, true);
    cfg.writeEntry(extraKey, false);

    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    TestSamplingCanvas canvas(image);
    canvas.rotation = 30.0;
    canvas.horizontalMirror = true;
    canvas.verticalMirror = true;
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Pk::green, image->colorSpace()));

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
    const PkRectF viewRect = helper.colorPreviewDocRect(PkPointF(10.25, 20.75));

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
    QVERIFY(!clearedDestination);
    QCOMPARE(backend.commands.size(), std::size_t(10));
    QVERIFY(std::holds_alternative<PkSaveCommand>(backend.commands.front()));
    const auto *hint = std::get_if<PkSetRenderHintCommand>(&backend.commands[1]);
    QVERIFY(hint);
    QCOMPARE(hint->hint, unsigned(PkPainter::Antialiasing));
    QVERIFY(hint->enabled);
    QVERIFY(std::holds_alternative<PkRestoreCommand>(backend.commands.back()));
    QVERIFY(!painter.testRenderHint(PkPainter::Antialiasing));

    const auto *topClip = std::get_if<PkSetClipPathCommand>(&backend.commands[2]);
    const auto *topFill = std::get_if<PkFillPathCommand>(&backend.commands[3]);
    const auto *bottomClip = std::get_if<PkSetClipPathCommand>(&backend.commands[4]);
    const auto *bottomFill = std::get_if<PkFillPathCommand>(&backend.commands[5]);
    const auto *clearClip = std::get_if<PkSetClipPathCommand>(&backend.commands[6]);
    const auto *outerStroke = std::get_if<PkStrokePathCommand>(&backend.commands[7]);
    const auto *innerStroke = std::get_if<PkStrokePathCommand>(&backend.commands[8]);
    QVERIFY(topClip);
    QVERIFY(topFill);
    QVERIFY(bottomClip);
    QVERIFY(bottomFill);
    QVERIFY(clearClip);
    QVERIFY(outerStroke);
    QVERIFY(innerStroke);
    QCOMPARE(topFill->brush.color(), PkColor(Pk::green));
    QCOMPARE(bottomFill->brush.color(), PkColor(Pk::red));
    QCOMPARE(topFill->path, bottomFill->path);
    QVERIFY(!topFill->path.contains(viewRect.center()));
    QVERIFY(topFill->path.contains(PkPointF(viewRect.center().x(),
                                            viewRect.top() + 10.0)));
    QCOMPARE(clearClip->operation, Pk::NoClip);
    QVERIFY(clearClip->path.isEmpty());
    QVERIFY(outerStroke->path != innerStroke->path);
    QCOMPARE(outerStroke->pen.widthF(), 2.0);
    QCOMPARE(innerStroke->pen.widthF(), 2.0);

    PkTransform contentTransform;
    contentTransform.translate(viewRect.center().x(), viewRect.center().y());
    contentTransform.rotate(canvas.rotation);
    contentTransform.translate(-viewRect.center().x(), -viewRect.center().y());
    PkPainterPath expectedTopClip;
    expectedTopClip.addRect(PkRectF(viewRect.left(),
                                    viewRect.top(),
                                    viewRect.width(),
                                    viewRect.height() / 2.0 + 1.0));
    PkPainterPath expectedBottomClip;
    expectedBottomClip.addRect(PkRectF(viewRect.left(),
                                       viewRect.center().y(),
                                       viewRect.width(),
                                       viewRect.height() / 2.0));
    QCOMPARE(topClip->path, contentTransform.map(expectedTopClip));
    QCOMPARE(bottomClip->path, contentTransform.map(expectedBottomClip));
}

void KisAsyncColorSamplerHelperTest::ellipsePreviewRoundsBeforeRotation()
{
    EllipsePreviewCanvas canvas;
    EllipsePreviewTool tool(&canvas);
    tool.setPreviewAngle(M_PI_2);

    RecordingBackend backend;
    PkPainter painter(backend);
    tool.paintPreview(painter, PkRectF(10.6, 20.6, 30.2, 40.2));

    const auto *ellipseCommand = std::get_if<PkDrawPolygonCommand>(
        &backend.commands.at(0));
    QVERIFY(ellipseCommand);

    const PkRect integerRect(11, 21, 30, 40);
    PkPainterPath expectedPath;
    expectedPath.addEllipse(PkRectF(integerRect));
    PkTransform rotation;
    rotation.translate(integerRect.center().x(), integerRect.center().y());
    rotation.rotateRadians(M_PI_2);
    rotation.translate(-integerRect.center().x(), -integerRect.center().y());
    const PkPolygonF expectedEllipse =
        rotation.map(expectedPath).toSubpathPolygons(PkTransform()).first();

    QCOMPARE(ellipseCommand->polygon, expectedEllipse);
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

void KisAsyncColorSamplerHelperTest::proxyDispatchesPolylineAndSelectionDecorations()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    EllipsePreviewCanvas canvas(image);
    setCurrentNode(canvas, layer);
    canvas.snapGuide()->enableSnapping(false);
    DecorationProxy proxy(&canvas);
    PolylinePreviewTool polyline(&canvas);
    SelectionPreviewTool selection(&canvas);

    for (KisToolPolylineBase *tool : {static_cast<KisToolPolylineBase *>(&polyline),
                                    static_cast<KisToolPolylineBase *>(&selection)}) {
        proxy.priv()->activeTool = tool;
        for (const PkPointF &point : {PkPointF(10, 20), PkPointF(30, 20)}) {
            QMouseEvent press(QEvent::MouseButtonPress, QPointF(point.x(), point.y()),
                              Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
            KoPointerEvent start(&press, point);
            tool->beginPrimaryAction(&start);
            QMouseEvent release(QEvent::MouseButtonRelease, QPointF(point.x(), point.y()),
                                Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
            KoPointerEvent end(&release, point);
            tool->endPrimaryAction(&end);
        }
        QMouseEvent move(QEvent::MouseMove, QPointF(50, 40),
                         Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        KoPointerEvent hover(&move, PkPointF(50, 40));
        tool->mouseMoveEvent(&hover);

        RecordingBackend backend;
        PkPainter painter(backend);
        proxy.paint(painter, *canvas.viewConverter());
        bool fixedSegment = false;
        bool draggingSegment = false;
        for (const auto &command : backend.commands) {
            if (const auto *polygon = std::get_if<PkDrawPolygonCommand>(&command)) {
                for (int i = 1; i < polygon->polygon.size(); ++i) {
                    const auto a = polygon->polygon.at(i - 1);
                    const auto b = polygon->polygon.at(i);
                    fixedSegment |= a == PkPointF(10, 20) && b == PkPointF(30, 20);
                    draggingSegment |= a == PkPointF(30, 20) && b == PkPointF(50, 40);
                }
            }
        }
        QVERIFY(fixedSegment);
        QVERIFY(draggingSegment);
        QCOMPARE(painter.transform(), PkTransform());
        tool->requestStrokeCancellation();
    }
    proxy.priv()->activeTool = nullptr;
}

void KisAsyncColorSamplerHelperTest::proxyDispatchesProductionAsyncSampler()
{
    KConfigGroup cfg = KSharedConfig::openConfig()->group("");
    const QString styleKey = QStringLiteral("colorSamplerPreviewStyle");
    const ConfigEntryGuard<int> styleGuard(cfg, styleKey, 1);
    cfg.writeEntry(styleKey, 2);

    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::red, &layer);
    EllipsePreviewCanvas canvas(image);
    setCurrentNode(canvas, layer);
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Pk::green, image->colorSpace()));
    DecorationProxy proxy(&canvas);
    SamplingPreviewTool tool(&canvas);
    proxy.priv()->activeTool = &tool;

    QMouseEvent press(QEvent::MouseButtonPress, QPointF(2, 3),
                      Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    KoPointerEvent event(&press, PkPointF(2, 3));
    tool.activateAlternateAction(KisTool::SampleFgImage);
    tool.beginAlternateAction(&event, KisTool::SampleFgImage);
    tool.endAlternateAction(&event, KisTool::SampleFgImage);
    image->waitForDone();
    QCoreApplication::processEvents();

    RecordingBackend backend;
    PkPainter painter(backend);
    proxy.paint(painter, *canvas.viewConverter());
    int fills = 0;
    for (const auto &command : backend.commands) {
        if (const auto *fill = std::get_if<PkFillPathCommand>(&command)) {
            QVERIFY(!fill->path.isEmpty());
            ++fills;
        }
    }
    QCOMPARE(fills, 2);
    QVERIFY(!painter.testRenderHint(PkPainter::Antialiasing));
    tool.deactivateAlternateAction(KisTool::SampleFgImage);
    proxy.priv()->activeTool = nullptr;
}

SIMPLE_TEST_MAIN(KisAsyncColorSamplerHelperTest)
