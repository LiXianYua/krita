/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <PkFlakeBridge.h>
#include <PkPainterPath.h>
#include <PkPen.h>
#include <pk/render/PkPaintCommand.h>

#include <KoCanvasBase.h>
#include <KoPointerEvent.h>
#include <KoSnapGuide.h>
#include <KoToolProxy.h>
#include <KoToolProxy_p.h>
#include <KoUnit.h>
#include <KoZoomHandler.h>

#include <KisCanvasToolServices.h>
#include <KisColorSamplingCanvas.h>
#include <KisOptimizedBrushOutline.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <KoColorSpaceRegistry.h>

#include <PkInputEvent.h>

#include <functional>
#include <optional>
#include <variant>
#include <vector>

#include "kis_tool_select_polygonal.h"

namespace {

class RecordingBackend final : public PkPainterBackend
{
public:
    void submit(const PkPaintCommand &command) override { commands.push_back(command); }
    qreal devicePixelRatio() const override { return 1.0; }

    std::vector<PkPaintCommand> commands;
};

class SelectionPreviewCanvas final : public KoCanvasBase,
                                     public KisCanvasToolServices,
                                     public KisColorSamplingCanvas
{
public:
    explicit SelectionPreviewCanvas(KisImageSP image)
        : KoCanvasBase(nullptr)
        , m_image(image)
    {
        m_converter.setResolution(1.0, 1.0);
        m_converter.setZoomedResolution(1.0, 1.0);
    }

    const KoViewConverter *viewConverter() const override { return &m_converter; }
    KoViewConverter *viewConverter() override { return &m_converter; }
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

    KisImageWSP samplingImage() const override { return m_image; }
    std::optional<KoColor> sampleVisibleReferenceColor(const PkPoint &) const override
    {
        return std::nullopt;
    }
    PkColor samplingPreviewColor(const KoColor &color) const override
    {
        return color.toQColor();
    }
    PkColor samplingPaletteBaseColor() const override { return Pk::white; }
    qreal samplingCanvasRotation() const override { return 0.0; }
    bool samplingCanvasMirroredHorizontally() const override { return false; }
    bool samplingCanvasMirroredVertically() const override { return false; }
    QCursor samplingCursor(bool, bool) const override { return {}; }
    KisCanvasCursorToken samplingCursorToken(bool, bool) const override
    {
        return KisCanvasCursorToken(1);
    }

    KisImageWSP toolImage() const override { return m_image; }
    PkPointF toolWidgetCenterInWidgetPixels() const override { return {}; }
    PkPointF toolDocumentToWidget(const PkPointF &point) const override { return point; }
    PkPointF toolDocumentToAlignedImagePixel(const PkPointF &point) const override { return point; }
    PkTransform toolImageToViewTransform() const override { return {}; }
    void drawToolOutline(PkPainter *painter,
                         const KisOptimizedBrushOutline &outline,
                         int) override
    {
        for (const PkPolygonF &polygon : outline) {
            painter->drawPolygon(polygon);
        }
    }
    bool toolBlockUntilOperationsFinished(KisImageWSP) override { return true; }
    void toolBlockUntilOperationsFinishedForced(KisImageWSP) override {}
    bool toolSelectionEditable() const override { return true; }
    KisCanvasToolSignals *toolSignals() override { return &m_signals; }
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
    QCursor toolForbiddenCursor() const override { return {}; }
    QCursor toolLoadCursor(const PkString &, int, int) const override { return {}; }
    QCursor loadCursorResource(const PkString &, const PkSize &, const PkPoint &) const override
    {
        return {};
    }
    KisCanvasCursorToken toolImportCursor(const QCursor &) const override
    {
        return KisCanvasCursorToken(1);
    }
    const QCursor *toolCursorSnapshot(KisCanvasCursorToken) const override { return &m_cursor; }
    KisCanvasCursorToken toolCursorToken(CursorStyle) const override { return KisCanvasCursorToken(1); }
    KisCanvasCursorToken toolMoveCursorToken() const override { return KisCanvasCursorToken(1); }
    KisCanvasCursorToken toolMoveSelectionCursorToken() const override { return KisCanvasCursorToken(1); }
    KisCanvasCursorToken toolSamplerCursorToken() const override { return KisCanvasCursorToken(1); }
    KisCanvasCursorToken toolOpenHandCursorToken() const override { return KisCanvasCursorToken(1); }
    KisCanvasCursorToken toolClosedHandCursorToken() const override { return KisCanvasCursorToken(1); }
    KisCanvasCursorToken toolForbiddenCursorToken() const override { return KisCanvasCursorToken(1); }
    KisCanvasCursorToken toolLoadCursorToken(const PkString &, int, int) const override
    {
        return KisCanvasCursorToken(1);
    }
    void toolApplyCursor(KisCanvasCursorToken) override {}
    void toolSetCursorPosition(const PkPoint &) override {}
    void toolShowBrushSize(qreal) override {}
    void toolShowLockedLayerMessage(bool) override {}
    void toolShowFloatingMessage(const PkString &, bool) override {}
    void toolShowRectangleSize(int, int) override {}
    void toolShowRectanglePosition(qreal, qreal) override {}
    PkString toolNodeEditableMessage(KisNodeSP, bool) const override { return {}; }
    PkPainterPath toolShapeHoverInfoCrossLayer(const PkPointF &,
                                               PkString &,
                                               bool *,
                                               bool) const override { return {}; }
    bool toolSelectShapeCrossLayer(const PkPointF &, const PkString &, bool) override
    {
        return false;
    }
    void toolUpdateCanvas() override {}
    void toolSetActionCallback(const PkString &,
                               const void *,
                               PkCallLifetime,
                               std::function<void()>,
                               bool) override {}
    void toolClearActionCallbacks(const void *) override {}
    void toolSetPriorityRightClickCallback(const void *,
                                           PkCallLifetime,
                                           std::function<bool()>,
                                           bool) override {}
    void toolSetPriorityEventFilter(PkObject *, bool) override {}
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

private:
    KisImageSP m_image;
    KoZoomHandler m_converter;
    mutable QCursor m_cursor;
    KisCanvasToolSignals m_signals;
};

class SelectionPreviewTool final : public __KisToolSelectPolygonalLocal
{
public:
    using __KisToolSelectPolygonalLocal::__KisToolSelectPolygonalLocal;

private:
    void finishPolyline(const PkVector<PkPointF> &) override {}
};

class DecorationProxy final : public KoToolProxy
{
public:
    using KoToolProxy::KoToolProxy;

protected:
    PkPointF widgetToDocument(const PkPointF &point) const override { return point; }
    PkPointF documentToWidget(const PkPointF &point) const override { return point; }
};

}

class SelectionPolygonalDispatchTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void proxyPaintDispatchesFixedAndDraggingSegments();
};

void SelectionPolygonalDispatchTest::proxyPaintDispatchesFixedAndDraggingSegments()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(nullptr, 80, 80, colorSpace, "selection dispatch test");
    KisPaintLayerSP layer = new KisPaintLayer(image, "paint", OPACITY_OPAQUE_U8);
    image->addNode(layer);

    SelectionPreviewCanvas canvas(image);
    canvas.snapGuide()->enableSnapping(false);
    DecorationProxy proxy(&canvas);
    SelectionPreviewTool selection(&canvas);
    QCOMPARE(selection.objectName(), PkString("tool_select_polygonal"));
    KisToolPolylineBase *polymorphicTool = &selection;
    proxy.priv()->activeTool = polymorphicTool;

    for (const PkPointF &point : {PkPointF(10, 20), PkPointF(30, 20)}) {
        PkInputEvent press(PkInputEvent::MouseButtonPress,
                           PkPointF(point.x(), point.y()),
                           PkPointF(point.x(), point.y()),
                           PkPointF(point.x(), point.y()),
                           Pk::LeftButton,
                           Pk::LeftButton,
                           Pk::NoModifier);
        KoPointerEvent start(press, point);
        polymorphicTool->beginPrimaryAction(&start);

        PkInputEvent release(PkInputEvent::MouseButtonRelease,
                             PkPointF(point.x(), point.y()),
                             PkPointF(point.x(), point.y()),
                             PkPointF(point.x(), point.y()),
                             Pk::LeftButton,
                             Pk::NoButton,
                             Pk::NoModifier);
        KoPointerEvent end(release, point);
        polymorphicTool->endPrimaryAction(&end);
    }

    PkInputEvent move(PkInputEvent::MouseMove,
                      PkPointF(50, 40),
                      PkPointF(50, 40),
                      PkPointF(50, 40),
                      Pk::NoButton,
                      Pk::NoButton,
                      Pk::NoModifier);
    KoPointerEvent hover(move, PkPointF(50, 40));
    polymorphicTool->mouseMoveEvent(&hover);

    RecordingBackend backend;
    PkPainter painter(backend);
    proxy.paint(painter, *canvas.viewConverter());

    bool fixedSegment = false;
    bool draggingSegment = false;
    for (const PkPaintCommand &command : backend.commands) {
        if (const auto *polygon = std::get_if<PkDrawPolygonCommand>(&command)) {
            for (int i = 1; i < polygon->polygon.size(); ++i) {
                const PkPointF a = polygon->polygon.at(i - 1);
                const PkPointF b = polygon->polygon.at(i);
                fixedSegment |= a == PkPointF(10, 20) && b == PkPointF(30, 20);
                draggingSegment |= a == PkPointF(30, 20) && b == PkPointF(50, 40);
            }
        }
    }

    QVERIFY(fixedSegment);
    QVERIFY(draggingSegment);
    QCOMPARE(painter.transform(), PkTransform());

    polymorphicTool->requestStrokeCancellation();
    proxy.priv()->activeTool = nullptr;
}

SIMPLE_TEST_MAIN(SelectionPolygonalDispatchTest)

#include "selection_polygonal_dispatch_test.moc"
