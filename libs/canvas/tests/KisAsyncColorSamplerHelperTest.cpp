/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkFlakeBridge.h>
#include <pk/render/PkPaintCommand.h>
#include "KisAsyncColorSamplerHelperTest.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <deque>
#include <memory>
#include <optional>
#include <thread>
#include <utility>
#include <vector>

#include <QKeyEvent>

#include <PkInputEvent.h>
#include <PkThreadCallQueue.h>

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>

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
#include "KisCanvasCursorToken.h"
#include "KisCanvasFeedback.h"
#include "KisCanvasToolServices.h"
#include "KisColorSamplingCanvas.h"
#include "KisOptimizedBrushOutline.h"
#include "kis_image.h"
#include "kis_paint_layer.h"
#include "brushengine/kis_no_size_paintop_settings.h"
#include "brushengine/kis_paintop_preset.h"
#include "tool/kis_tool_ellipse_base.h"
#include "tool/kis_tool_freehand.h"
#include "tool/kis_tool_polyline_base.h"
#include "tool/strokes/kis_color_sampler_stroke_strategy.h"

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
        sampleVisibleReferenceColor(const PkPoint &imagePoint) const override
    {
        ++referenceSampleCount;
        lastReferencePoint = imagePoint;
        return referenceColor;
    }

    PkColor samplingPreviewColor(const KoColor &color) const override
    {
        ++previewConversionCount;
        return color.toQColor();
    }

    PkColor samplingPaletteBaseColor() const override
    {
        return paletteBaseColor;
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

    KisCanvasCursorToken samplingCursorToken(bool sampleCurrentLayer,
                                             bool pickFgColor) const override
    {
        ++cursorQueryCount;
        lastCursorSampleCurrentLayer = sampleCurrentLayer;
        lastCursorPickFgColor = pickFgColor;
        return KisCanvasCursorToken(0x51);
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
    void setCursor(KisCanvasCursorToken) override {}
    void addCommand(KUndo2Command *) override {}
    KoShapeManager *shapeManager() const override { return nullptr; }
    KoSelectedShapesProxy *selectedShapesProxy() const override { return nullptr; }
    void updateCanvas(const PkRectF &) override {}
    KoToolProxy *toolProxy() const override { return nullptr; }
    QWidget *canvasWidget() override { return nullptr; }
    const QWidget *canvasWidget() const override { return nullptr; }
    KoUnit unit() const override { return KoUnit(KoUnit::Millimeter); }

    void showFloatingMessage(const PkString &,
                             int,
                             Priority,
                             int) override
    {
        ++feedbackCount;
    }

    KisImageSP m_image;
    mutable KoZoomHandler m_converter;
    std::optional<KoColor> referenceColor;
    PkColor paletteBaseColor {Pk::white};
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
    mutable PkPoint lastReferencePoint;
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
    struct ActionCallbackRecord {
        PkString name;
        const void *receiverIdentity {nullptr};
        PkCallLifetime receiverLifetime;
        std::function<void()> callback;
        bool unique {false};
    };

    explicit EllipsePreviewCanvas(KisImageSP image = {})
        : TestSamplingCanvas(image)
    {
    }

    KisImageWSP toolImage() const override { return m_image; }
    PkPointF toolWidgetCenterInWidgetPixels() const override { return {}; }
    PkPointF toolDocumentToWidget(const PkPointF &point) const override { return point; }
    PkPointF toolDocumentToAlignedImagePixel(const PkPointF &point) const override { return point; }
    PkTransform toolImageToViewTransform() const override { return {}; }
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
    KisCanvasToolSignals *toolSignals() override { return &toolSignalBus; }
    KisPaintOpPresetSP toolCurrentPaintOpPreset() const override { return {}; }
    void toolNotifyPaintingFinished() override {}
    void toolSetControlsEnabled(bool) override {}
    KisPopupWidgetInterface *toolPopupWidget() const override { return nullptr; }
    PkSize toolCanvasWidgetSize() const override { return {}; }
    PkRect toolAvailableVirtualScreenGeometry() const override { return PkRect(0, 0, 1000, 1000); }
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
    KisCanvasCursorToken loadCursorResource(const PkString &, const PkSize &,
                                            const PkPoint &) const override { return {}; }
    KisCanvasCursorToken toolShapeCursorToken(Pk::CursorShape shape) const override
    {
        return toolImportCursor(QCursor(static_cast<Qt::CursorShape>(shape)));
    }
    KisCanvasCursorToken toolImportCursor(const QCursor &cursor) const override
    {
        ++cursorImportCount;
        if (std::this_thread::get_id() != cursorOwnerThread) {
            ++cursorThreadFailureCount;
            return {};
        }
        const auto existing = std::find_if(cursorSnapshots.begin(), cursorSnapshots.end(),
                                           [&cursor](const auto &entry) {
                                               return entry.second == cursor;
                                           });
        if (existing != cursorSnapshots.end()) {
            return existing->first;
        }
        static std::atomic<std::uint64_t> nextToken {1};
        const KisCanvasCursorToken token(nextToken.fetch_add(1));
        cursorSnapshots.emplace_back(token, cursor);
        return token;
    }
    const QCursor *toolCursorSnapshot(KisCanvasCursorToken cursor) const override
    {
        if (!cursor) return &defaultCursor;
        const auto it = std::find_if(cursorSnapshots.begin(), cursorSnapshots.end(),
                                     [cursor](const auto &entry) {
                                         return entry.first == cursor;
                                     });
        return it == cursorSnapshots.end() ? nullptr : &it->second;
    }
    KisCanvasCursorToken toolCursorToken(CursorStyle style) const override
    {
        return KisCanvasCursorToken(0x100 + std::uint64_t(style));
    }
    KisCanvasCursorToken toolMoveCursorToken() const override { return KisCanvasCursorToken(0x201); }
    KisCanvasCursorToken toolMoveSelectionCursorToken() const override { return KisCanvasCursorToken(0x202); }
    KisCanvasCursorToken toolSamplerCursorToken() const override { return KisCanvasCursorToken(0x203); }
    KisCanvasCursorToken toolOpenHandCursorToken() const override { return KisCanvasCursorToken(0x204); }
    KisCanvasCursorToken toolClosedHandCursorToken() const override { return KisCanvasCursorToken(0x205); }
    KisCanvasCursorToken toolForbiddenCursorToken() const override { return KisCanvasCursorToken(0x206); }
    KisCanvasCursorToken toolLoadCursorToken(const PkString &, int, int) const override
    {
        return KisCanvasCursorToken(0x207);
    }
    void toolApplyCursor(KisCanvasCursorToken cursor) override
    {
        const auto it = std::find_if(cursorSnapshots.begin(), cursorSnapshots.end(),
                                     [cursor](const auto &entry) {
                                         return entry.first == cursor;
                                     });
        if (cursor && it == cursorSnapshots.end()) {
            ++cursorRejectCount;
            return;
        }
        lastAppliedCursor = cursor;
        lastAppliedCursorShape = cursor ? it->second.shape() : defaultCursor.shape();
        ++cursorApplyCount;
    }
    bool toolOwnsCursor(KisCanvasCursorToken cursor) const override
    {
        ++cursorOwnershipQueryCount;
        return KisCanvasToolServices::toolOwnsCursor(cursor);
    }
    void toolSetCursorPosition(const PkPoint &position) override
    {
        cursorPositions.push_back(position);
    }
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
    bool toolSelectShapeCrossLayer(const PkPointF &,
                                   const PkString &,
                                   bool) override { return false; }
    void toolUpdateCanvas() override {}
    void toolSetActionCallback(const PkString &name,
                               const void *receiverIdentity,
                               PkCallLifetime receiverLifetime,
                               std::function<void()> callback,
                               bool unique) override
    {
        actionCallbacks.push_back({name, receiverIdentity, std::move(receiverLifetime),
                                   std::move(callback), unique});
    }
    void toolClearActionCallbacks(const void *receiverIdentity) override
    {
        clearedActionReceiver = receiverIdentity;
        actionCallbacks.clear();
    }
    void toolSetPriorityRightClickCallback(const void *receiverIdentity,
                                           PkCallLifetime receiverLifetime,
                                           std::function<bool()> callback,
                                           bool attached) override
    {
        rightClickReceiver = receiverIdentity;
        rightClickLifetime = std::move(receiverLifetime);
        rightClickCallback = std::move(callback);
        rightClickAttached = attached;
    }
    void toolSetPriorityEventFilter(PkObject *filter, bool attached) override
    {
        priorityEventFilter = filter;
        priorityEventFilterAttached = attached;
    }
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

    bool dispatchAction(const PkString &name)
    {
        const auto it = std::find_if(actionCallbacks.begin(), actionCallbacks.end(),
                                     [&name](const ActionCallbackRecord &record) {
                                         return record.name == name;
                                     });
        if (it == actionCallbacks.end() || !it->receiverLifetime.claim ||
            !it->receiverLifetime.alive) {
            return false;
        }
        std::lock_guard<std::recursive_mutex> guard(*it->receiverLifetime.claim);
        if (!it->receiverLifetime.alive->load(std::memory_order_acquire)) return false;
        it->callback();
        return true;
    }

    bool dispatchRightClick(bool *invoked)
    {
        if (!rightClickAttached || !rightClickCallback || !rightClickLifetime.claim ||
            !rightClickLifetime.alive) {
            return false;
        }
        std::lock_guard<std::recursive_mutex> guard(*rightClickLifetime.claim);
        if (!rightClickLifetime.alive->load(std::memory_order_acquire)) return false;
        *invoked = true;
        return rightClickCallback();
    }

    std::vector<ActionCallbackRecord> actionCallbacks;
    const void *clearedActionReceiver {nullptr};
    const void *rightClickReceiver {nullptr};
    PkCallLifetime rightClickLifetime;
    std::function<bool()> rightClickCallback;
    bool rightClickAttached {false};
    PkObject *priorityEventFilter {nullptr};
    bool priorityEventFilterAttached {false};
    mutable int cursorImportCount {0};
    mutable int cursorThreadFailureCount {0};
    mutable int cursorOwnershipQueryCount {0};
    int cursorApplyCount {0};
    int cursorRejectCount {0};
    KisCanvasCursorToken lastAppliedCursor;
    Qt::CursorShape lastAppliedCursorShape {Qt::BlankCursor};
    std::vector<PkPoint> cursorPositions;
    mutable std::deque<std::pair<KisCanvasCursorToken, QCursor>> cursorSnapshots;
    QCursor defaultCursor {Qt::ArrowCursor};
    const std::thread::id cursorOwnerThread {std::this_thread::get_id()};
    KisCanvasToolSignals toolSignalBus;
};

class EllipsePreviewTool final : public KisToolEllipseBase
{
public:
    explicit EllipsePreviewTool(KoCanvasBase *canvas)
        : KisToolEllipseBase(canvas, SELECT, {})
    {
    }

    void paintPreview(PkPainter &painter, const PkRectF &rect)
    {
        paintRectangle(painter, rect);
    }

    void setPreviewAngle(qreal angle) { m_angle = angle; }
    void applyStoredCursor() { resetCursorStyle(); }
    bool applyCursorToken(KisCanvasCursorToken token) { return KoToolBase::useCursor(token); }
    void applyShapeCursor(Pk::CursorShape shape) { KoToolBase::useCursor(shape); }
    KisCanvasCursorToken storedCursorToken() const { return cursor(); }

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
        : KisToolPolylineBase(canvas, SELECT, {}) {}
protected:
    void finishPolyline(const PkVector<PkPointF> &) override {}
};

class SamplingPreviewTool final : public KisToolPaint
{
public:
    explicit SamplingPreviewTool(KoCanvasBase *canvas)
        : KisToolPaint(canvas, {}) { setSupportOutline(true); }
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

class ResizeTestSettings final : public KisNoSizePaintOpSettings
{
public:
    ResizeTestSettings()
        : KisNoSizePaintOpSettings({})
    {
    }

    void setPaintOpSize(qreal value) override { m_size = value; }
    qreal paintOpSize() const override { return m_size; }
    KisPaintOpSettingsSP clone() const override
    {
        return KisPaintOpSettingsSP(new ResizeTestSettings(*this));
    }

private:
    qreal m_size {25.0};
};

struct PointerEventObservation {
    PkPointF point;
    PkPoint position;
    PkPoint globalPosition;
    Pk::MouseButton button {Pk::NoButton};
    Pk::MouseButtons buttons;
    Pk::KeyboardModifiers modifiers;
    double pressure {0.0};
    double rotation {0.0};
    double tangentialPressure {0.0};
    double xTilt {0.0};
    double yTilt {0.0};
    int z {0};
    std::uint64_t timestamp {0};
    bool accepted {false};
    bool spontaneous {false};
    bool tablet {false};
    bool touch {false};
};

class FreehandEventProbeTool final : public KisToolFreehand
{
public:
    explicit FreehandEventProbeTool(KoCanvasBase *canvas)
        : KisToolFreehand(canvas, {}, KUndo2MagicString(), false)
    {
    }

    using KisToolFreehand::beginAlternateAction;
    using KisToolFreehand::continueAlternateAction;
    using KisToolFreehand::endAlternateAction;
    using KisToolFreehand::requestStrokeCancellation;

    bool gestureActive() const { return mode() == GESTURE_MODE; }
    std::vector<std::optional<PointerEventObservation>> observations;

protected:
    void requestUpdateOutline(const PkPointF &, const KoPointerEvent *event) override
    {
        if (!event) {
            observations.emplace_back(std::nullopt);
            return;
        }
        observations.emplace_back(PointerEventObservation {
            event->point,
            event->pos(),
            event->globalPos(),
            event->button(),
            event->buttons(),
            event->modifiers(),
            event->pressure(),
            event->rotation(),
            event->tangentialPressure(),
            event->xTilt(),
            event->yTilt(),
            event->z(),
            event->time(),
            event->isAccepted(),
            event->spontaneous(),
            event->isTabletEvent(),
            event->isTouchEvent()
        });
    }
};

class KeyEventProbeTool final : public KisToolPaint
{
public:
    explicit KeyEventProbeTool(KoCanvasBase *canvas)
        : KisToolPaint(canvas, {}) {}

    Pk::Key lastKey {static_cast<Pk::Key>(0)};
    Pk::KeyboardModifiers lastModifiers;
    int pressCount {0};
    int releaseCount {0};

    void pkKeyPressEvent(PkToolKeyEvent *event) override
    {
        lastKey = event->key();
        lastModifiers = event->modifiers();
        ++pressCount;
        event->accept();
    }

    void pkKeyReleaseEvent(PkToolKeyEvent *event) override
    {
        lastKey = event->key();
        lastModifiers = event->modifiers();
        ++releaseCount;
        event->ignore();
    }

protected:
    KisOptimizedBrushOutline getOutlinePath(const PkPointF &, const KoPointerEvent *,
                                             KisPaintOpSettings::OutlineMode) override { return {}; }
};

template <typename T>
class ConfigEntryGuard
{
public:
    ConfigEntryGuard(PkConfigGroup group, PkString key, T fallback)
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
    PkConfigGroup m_group;
    PkString m_key;
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

}

void KisAsyncColorSamplerHelperTest::initTestCase()
{
    PkThreadCallQueue::warmUpCurrentThread();
}

void KisAsyncColorSamplerHelperTest::cleanup()
{
    PkThreadCallQueue::processPendingCalls();
}

void KisAsyncColorSamplerHelperTest::delayedPreviewWaitsForPkTimerPump()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    TestSamplingCanvas canvas(image);
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Pk::green, image->colorSpace()));
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);

    helper.activate(false, true);
    QVERIFY(helper.colorPreviewDocRect(PkPointF(2, 3)).isEmpty());

    std::this_thread::sleep_for(std::chrono::milliseconds(40));
    QCOMPARE(PkThreadCallQueue::pendingCount(), std::size_t(0));
    QVERIFY(helper.colorPreviewDocRect(PkPointF(2, 3)).isEmpty());

    std::this_thread::sleep_for(std::chrono::milliseconds(180));
    QCOMPARE(PkThreadCallQueue::pendingCount(), std::size_t(1));
    QVERIFY(helper.colorPreviewDocRect(PkPointF(2, 3)).isEmpty());
    QCOMPARE(PkThreadCallQueue::processPendingCalls(), 1);
    QVERIFY(!helper.colorPreviewDocRect(PkPointF(2, 3)).isEmpty());
    QCOMPARE(canvas.previewConversionCount, 1);
    helper.deactivate();
}

void KisAsyncColorSamplerHelperTest::deactivationCancelsDelayedPreview()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    TestSamplingCanvas canvas(image);
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);

    helper.activate(false, true);
    helper.deactivate();
    std::this_thread::sleep_for(std::chrono::milliseconds(140));

    QCOMPARE(PkThreadCallQueue::pendingCount(), std::size_t(0));
    QCOMPARE(canvas.previewConversionCount, 0);
    QVERIFY(helper.colorPreviewDocRect(PkPointF(2, 3)).isEmpty());
}

void KisAsyncColorSamplerHelperTest::destructionInvalidatesQueuedPreview()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    TestSamplingCanvas canvas(image);

    {
        KisAsyncColorSamplerHelper helper(&canvas, &canvas);
        helper.activate(false, true);
        std::this_thread::sleep_for(std::chrono::milliseconds(140));
        QCOMPARE(PkThreadCallQueue::pendingCount(), std::size_t(1));
    }

    QCOMPARE(PkThreadCallQueue::processPendingCalls(), 1);
    QCOMPARE(canvas.previewConversionCount, 0);
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
    PkObject::connect(&helper,
            &KisAsyncColorSamplerHelper::sigRawColorSelected,
            &helper,
            [&sampledColors](const KoColor &color) {
                sampledColors.append(color);
            });

    helper.activate(false, true);
    helper.startAction(PkPointF(2, 3), 1, 100);
    sampledColors.clear();
    canvas.referenceSampleCount = 0;
    helper.slotAddSamplingJob(PkPointF(2, 3));
    helper.endAction();
    image->waitForDone();
    QTest::qWait(120);

    QCOMPARE(canvas.referenceSampleCount, 1);
    QCOMPARE(canvas.lastReferencePoint.x(), 2);
    QCOMPARE(canvas.lastReferencePoint.y(), 3);
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
    PkObject::connect(&helper,
            &KisAsyncColorSamplerHelper::sigRawColorSelected,
            &helper,
            [&sampledColors](const KoColor &color) {
                sampledColors.append(color);
            });

    helper.activate(false, true);
    helper.startAction(PkPointF(2, 3), 1, 100);
    sampledColors.clear();
    canvas.referenceSampleCount = 0;
    helper.slotAddSamplingJob(PkPointF(2, 3));
    helper.endAction();
    image->waitForDone();
    QTRY_COMPARE((PkThreadCallQueue::processPendingCalls(), sampledColors.size()), 2);

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
    PkObject::connect(&helper,
            &KisAsyncColorSamplerHelper::sigRawColorSelected,
            &helper,
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
    helper.slotAddSamplingJob(PkPointF(2, 3));
    helper.endAction();
    image->waitForDone();
    QTRY_COMPARE((PkThreadCallQueue::processPendingCalls(), sampledColors.size()), 2);

    QCOMPARE(canvas.referenceSampleCount, 0);
    QCOMPARE(toQColor(sampledColors.first().toQColor()), QColor(Qt::red));
    QCOMPARE(toQColor(sampledColors.last().toQColor()), QColor(Qt::blue));
}

void KisAsyncColorSamplerHelperTest::previewUsesSamplingCanvasGeometry()
{
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group("");
    const PkString key("colorSamplerPreviewStyle");
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
    helper.activateDelayedPreview();
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
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group("");
    const PkString styleKey("colorSamplerPreviewStyle");
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
    helper.activateDelayedPreview();
    helper.slotColorSamplingFinished(KoColor(Pk::red, image->colorSpace()));
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
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group("");
    const PkString styleKey("colorSamplerPreviewStyle");
    const PkString diameterKey("colorSamplerPreviewCircleDiameter");
    const PkString thicknessKey("colorSamplerPreviewCircleThickness");
    const PkString outlineKey("colorSamplerPreviewCircleOutlineEnabled");
    const PkString extraKey("colorSamplerPreviewCircleExtraCirclesEnabled");
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
    canvas.paletteBaseColor = PkColor(10, 20, 30);
    canvas.rotation = 30.0;
    canvas.horizontalMirror = true;
    canvas.verticalMirror = true;
    canvas.resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                          KoColor(Pk::green, image->colorSpace()));

    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    helper.setUpdateGlobalColor(false);
    helper.activate(false, true);
    helper.activateDelayedPreview();
    helper.slotColorSamplingFinished(KoColor(Pk::red, image->colorSpace()));
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
    PkColor expectedOutlineColor = canvas.paletteBaseColor;
    expectedOutlineColor.setAlpha(OPACITY_OPAQUE_U8 / 2 + 1);
    QCOMPARE(outerStroke->pen.color(), expectedOutlineColor);
    QCOMPARE(innerStroke->pen.color(), expectedOutlineColor);

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

    KisCanvasCursorToken requestedCursor;
    PkObject::connect(&helper,
            &KisAsyncColorSamplerHelper::sigRequestCursor,
            &helper,
            [&requestedCursor](KisCanvasCursorToken cursor) {
                requestedCursor = cursor;
            });

    helper.updateCursor(true, false);

    QCOMPARE(canvas.cursorQueryCount, 1);
    QVERIFY(canvas.lastCursorSampleCurrentLayer);
    QVERIFY(!canvas.lastCursorPickFgColor);
    QCOMPARE(requestedCursor.value(), std::uint64_t(0x51));
}

void KisAsyncColorSamplerHelperTest::toolCursorTokenPersistsAndApplies()
{
    EllipsePreviewCanvas canvas;
    EllipsePreviewTool tool(&canvas);
    KisCanvasCursorToken notifiedToken;
    int tokenNotificationCount = 0;
    PkObject::connect(&tool, &KoToolBase::cursorTokenChanged, &tool,
                      [&](KisCanvasCursorToken token) {
                          notifiedToken = token;
                          ++tokenNotificationCount;
                      });

    // Task 3: the tool constructor now stores the passed KisCanvasCursorToken
    // verbatim (KisTool::KisTool(KoCanvasBase*, KisCanvasCursorToken)) instead
    // of importing a default QCursor into a token. A {} argument therefore means
    // the zero token == platform default cursor, and nothing is imported at
    // construction time.
    QCOMPARE(canvas.cursorImportCount, 0);
    QCOMPARE(canvas.cursorApplyCount, 0);
    QCOMPARE(canvas.cursorOwnershipQueryCount, 0);
    QVERIFY(!tool.storedCursorToken());

    tool.applyStoredCursor();

    // One successful change publishes exactly one token notification and makes
    // exactly one host ownership decision followed by exactly one host apply.
    // The applied shape is observed on the host: the cursor value no longer
    // exists on the tool side.
    QCOMPARE(canvas.cursorApplyCount, 1);
    QCOMPARE(canvas.cursorOwnershipQueryCount, 1);
    QCOMPARE(canvas.lastAppliedCursor, tool.storedCursorToken());
    QCOMPARE(canvas.lastAppliedCursorShape, QCursor().shape());
    QCOMPARE(notifiedToken, tool.storedCursorToken());
    QCOMPARE(tokenNotificationCount, 1);
}

void KisAsyncColorSamplerHelperTest::toolShapeCursorReachesHostAsThatShape()
{
    EllipsePreviewCanvas canvas;
    EllipsePreviewTool tool(&canvas);

    const int importsBefore = canvas.cursorImportCount;
    int tokenNotificationCount = 0;
    PkObject::connect(&tool, &KoToolBase::cursorTokenChanged, &tool,
                      [&](KisCanvasCursorToken) { ++tokenNotificationCount; });

    // KoToolBase::useCursor(Pk::CursorShape) is the entry point the retained
    // call sites use; it can only reach the canvas through
    // KoCanvasCursorHost::toolShapeCursorToken(). ForbiddenCursor is
    // deliberately not ArrowCursor: a host that fails to answer for the shape
    // returns token zero, which restores the platform default arrow, and both
    // the token asserted below and the observed shape would then be the
    // default one instead.
    tool.applyShapeCursor(Pk::ForbiddenCursor);

    const KisCanvasCursorToken token = tool.cursorToken();
    QVERIFY(token);
    QCOMPARE(canvas.cursorImportCount, importsBefore + 1);
    QCOMPARE(canvas.lastAppliedCursor, token);
    QCOMPARE(canvas.lastAppliedCursorShape, Qt::ForbiddenCursor);
    QCOMPARE(tokenNotificationCount, 1);

    // The host's own snapshot oracle must agree about what it holds.
    QVERIFY(canvas.toolCursorSnapshot(token));
    QCOMPARE(canvas.toolCursorSnapshot(token)->shape(), Qt::ForbiddenCursor);

    // The answer is per shape, not one cached default: a second shape yields a
    // different token that is observable as that other shape.
    tool.applyShapeCursor(Pk::CrossCursor);
    const KisCanvasCursorToken secondToken = tool.cursorToken();
    QVERIFY(secondToken);
    QVERIFY(secondToken != token);
    QCOMPARE(canvas.lastAppliedCursor, secondToken);
    QCOMPARE(canvas.lastAppliedCursorShape, Qt::CrossCursor);
}

void KisAsyncColorSamplerHelperTest::toolCursorRejectsForeignTokenWithoutObservableMutation()
{
    EllipsePreviewCanvas canvas;
    EllipsePreviewTool tool(&canvas);
    EllipsePreviewCanvas foreignHost;
    const KisCanvasCursorToken foreignToken =
        foreignHost.toolImportCursor(QCursor(Qt::CrossCursor));

    const KisCanvasCursorToken retainedToken = tool.storedCursorToken();
    const KisCanvasCursorToken retainedAppliedToken = canvas.lastAppliedCursor;
    const Qt::CursorShape retainedAppliedShape = canvas.lastAppliedCursorShape;
    int tokenNotificationCount = 0;
    PkObject::connect(&tool, &KoToolBase::cursorTokenChanged, &tool,
                      [&](KisCanvasCursorToken) { ++tokenNotificationCount; });

    QVERIFY(!tool.applyCursorToken(foreignToken));

    // One failed change makes its host ownership decision and stops there: no
    // apply, no rejection of an owned token, no token notification, and nothing
    // observable on the host changed.
    QCOMPARE(canvas.cursorOwnershipQueryCount, 1);
    QVERIFY(!canvas.toolOwnsCursor(foreignToken));
    QVERIFY(canvas.toolOwnsCursor(retainedToken));
    QCOMPARE(tool.storedCursorToken(), retainedToken);
    QCOMPARE(canvas.lastAppliedCursor, retainedAppliedToken);
    QCOMPARE(canvas.lastAppliedCursorShape, retainedAppliedShape);
    QCOMPARE(canvas.cursorApplyCount, 0);
    QCOMPARE(canvas.cursorRejectCount, 0);
    QCOMPARE(tokenNotificationCount, 0);
}

void KisAsyncColorSamplerHelperTest::cursorTokenContractCoversZeroIdentityScopeAndThreadAffinity()
{
    auto first = std::make_unique<EllipsePreviewCanvas>();

    first->toolApplyCursor({});
    QCOMPARE(first->lastAppliedCursor, KisCanvasCursorToken());
    QCOMPARE(first->lastAppliedCursorShape, Qt::ArrowCursor);

    QCursor source(Qt::CrossCursor);
    const KisCanvasCursorToken firstToken = first->toolImportCursor(source);
    const KisCanvasCursorToken repeatedToken = first->toolImportCursor(source);
    QVERIFY(firstToken);
    QCOMPARE(repeatedToken, firstToken);
    source.setShape(Qt::WaitCursor);
    QVERIFY(first->toolCursorSnapshot(firstToken));
    QCOMPARE(first->toolCursorSnapshot(firstToken)->shape(), Qt::CrossCursor);
    const KisCanvasCursorToken changedToken = first->toolImportCursor(source);
    QVERIFY(changedToken != firstToken);

    EllipsePreviewCanvas second;
    const KisCanvasCursorToken secondToken = second.toolImportCursor(QCursor(Qt::CrossCursor));
    QVERIFY(secondToken != firstToken);
    second.toolApplyCursor(firstToken);
    QCOMPARE(second.cursorRejectCount, 1);
    QCOMPARE(second.cursorApplyCount, 0);

    KisCanvasCursorToken workerToken;
    std::thread worker([&] {
        workerToken = first->toolImportCursor(QCursor(Qt::BusyCursor));
    });
    worker.join();
    QVERIFY(!workerToken);
    QCOMPARE(first->cursorThreadFailureCount, 1);

    const KisCanvasCursorToken destroyedHostToken = firstToken;
    first.reset();
    EllipsePreviewCanvas replacement;
    const KisCanvasCursorToken replacementToken =
        replacement.toolImportCursor(QCursor(Qt::CrossCursor));
    QVERIFY(replacementToken != destroyedHostToken);
}

void KisAsyncColorSamplerHelperTest::freehandAlternateActionRetainsDetachedEventAndResets()
{
    EllipsePreviewCanvas canvas;
    KisPaintOpPresetSP preset(new KisPaintOpPreset());
    preset->setSettings(new ResizeTestSettings());
    canvas.resourceManager()->setResource(KoCanvasResource::CurrentPaintOpPreset,
                                          PkVariant::fromValue(preset));
    FreehandEventProbeTool tool(&canvas);

    {
        // The host classifies its own platform event; the native carrier has no
        // device/pointer pair and no acceptance flag (see KoPointerEvent).
        PkTabletEvent hostEvent(PkInputEvent::TabletPress,
                                PkPointF(12, 13), PkPointF(112, 113),
                                Pk::LeftButton, Pk::LeftButton,
                                Pk::ShiftModifier,
                                0.42, 17, -11, 0.25, 33.0, 7,
                                99, std::uint64_t(1234));
        KoPointerEvent beginEvent(hostEvent, PkPointF(20, 30));
        beginEvent.ignore();
        tool.beginAlternateAction(&beginEvent, KisTool::ChangeSize);
    }

    QVERIFY(tool.gestureActive());
    QCOMPARE(tool.observations.size(), std::size_t(1));
    QVERIFY(tool.observations.back().has_value());
    const PointerEventObservation retained = *tool.observations.back();
    QCOMPARE(retained.point, PkPointF(20, 30));
    QCOMPARE(retained.position, PkPoint(12, 13));
    QCOMPARE(retained.globalPosition, PkPoint(112, 113));
    QCOMPARE(retained.button, Pk::LeftButton);
    QCOMPARE(int(retained.buttons), int(Pk::LeftButton));
    QCOMPARE(int(retained.modifiers), int(Pk::ShiftModifier));
    QCOMPARE(retained.pressure, 0.42);
    QCOMPARE(retained.rotation, 33.0);
    QCOMPARE(retained.tangentialPressure, 0.625);
    QCOMPARE(retained.xTilt, 17.0);
    QCOMPARE(retained.yTilt, -11.0);
    QCOMPARE(retained.z, 7);
    QCOMPARE(retained.timestamp, std::uint64_t(1234));
    QVERIFY(!retained.accepted);
    QVERIFY(!retained.spontaneous);
    QVERIFY(retained.tablet);
    QVERIFY(!retained.touch);

    KoPointerEvent continuation(PkPoint(32, 13), PkPointF(40, 30),
                                Pk::LeftButton, Pk::LeftButton, Pk::ShiftModifier);
    tool.continueAlternateAction(&continuation, KisTool::ChangeSize);
    QCOMPARE(tool.observations.size(), std::size_t(2));
    QVERIFY(tool.observations.back().has_value());
    const PointerEventObservation continued = *tool.observations.back();
    QCOMPARE(continued.point, retained.point);
    QCOMPARE(continued.position, retained.position);
    QCOMPARE(continued.globalPosition, retained.globalPosition);
    QCOMPARE(continued.button, retained.button);
    QCOMPARE(continued.buttons, retained.buttons);
    QCOMPARE(continued.modifiers, retained.modifiers);
    QCOMPARE(continued.pressure, retained.pressure);
    QCOMPARE(continued.rotation, retained.rotation);
    QCOMPARE(continued.tangentialPressure, retained.tangentialPressure);
    QCOMPARE(continued.xTilt, retained.xTilt);
    QCOMPARE(continued.yTilt, retained.yTilt);
    QCOMPARE(continued.z, retained.z);
    QCOMPARE(continued.timestamp, retained.timestamp);
    QCOMPARE(continued.accepted, retained.accepted);
    QCOMPARE(continued.spontaneous, retained.spontaneous);
    QCOMPARE(continued.tablet, retained.tablet);
    QCOMPARE(continued.touch, retained.touch);

    tool.endAlternateAction(&continuation, KisTool::ChangeSize);
    QVERIFY(!tool.gestureActive());
    QVERIFY(!tool.observations.back().has_value());
    QCOMPARE(canvas.cursorPositions,
             std::vector<PkPoint>({PkPoint(112, 113)}));

    tool.beginAlternateAction(&continuation, KisTool::ChangeSizeSnap);
    QVERIFY(tool.gestureActive());
    QVERIFY(tool.observations.back().has_value());
    tool.requestStrokeCancellation();
    QVERIFY(!tool.gestureActive());
    QVERIFY(!tool.observations.back().has_value());
    QCOMPARE(canvas.cursorPositions,
             std::vector<PkPoint>({PkPoint(112, 113), PkPoint(32, 13)}));
}

void KisAsyncColorSamplerHelperTest::proxyDispatchesPolylineDecorations()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    EllipsePreviewCanvas canvas(image);
    setCurrentNode(canvas, layer);
    canvas.snapGuide()->enableSnapping(false);
    DecorationProxy proxy(&canvas);
    PolylinePreviewTool polyline(&canvas);
    KisToolPolylineBase *tool = &polyline;
    proxy.priv()->activeTool = tool;
    for (const PkPointF &point : {PkPointF(10, 20), PkPointF(30, 20)}) {
        PkInputEvent press(PkInputEvent::MouseButtonPress,
                           PkPointF(point.x(), point.y()), PkPointF(point.x(), point.y()), PkPointF(point.x(), point.y()),
                           Pk::LeftButton, Pk::LeftButton, Pk::NoModifier);
        KoPointerEvent start(press, point);
        tool->beginPrimaryAction(&start);
        PkInputEvent release(PkInputEvent::MouseButtonRelease,
                             PkPointF(point.x(), point.y()), PkPointF(point.x(), point.y()), PkPointF(point.x(), point.y()),
                             Pk::LeftButton, Pk::NoButton, Pk::NoModifier);
        KoPointerEvent end(release, point);
        tool->endPrimaryAction(&end);
    }
    PkInputEvent move(PkInputEvent::MouseMove,
                      PkPointF(50, 40), PkPointF(50, 40), PkPointF(50, 40),
                      Pk::NoButton, Pk::NoButton, Pk::NoModifier);
    KoPointerEvent hover(move, PkPointF(50, 40));
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
    proxy.priv()->activeTool = nullptr;
}

void KisAsyncColorSamplerHelperTest::hostCallbacksPreserveActionAndRightClickLifecycle()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    EllipsePreviewCanvas canvas(image);
    setCurrentNode(canvas, layer);
    PolylinePreviewTool tool(&canvas);

    tool.activate({});

    QCOMPARE(canvas.actionCallbacks.size(), std::size_t(7));
    QCOMPARE(canvas.actionCallbacks.back().name, PkString("undo_polygon_selection"));
    QCOMPARE(canvas.actionCallbacks.back().receiverIdentity,
             static_cast<const void *>(&tool));
    QVERIFY(canvas.actionCallbacks.back().unique);
    QVERIFY(canvas.rightClickAttached);
    QCOMPARE(canvas.rightClickReceiver, static_cast<const void *>(&tool));
    QVERIFY(canvas.rightClickCallback);
    QVERIFY(!canvas.rightClickCallback());

    PkInputEvent press(PkInputEvent::MouseButtonPress,
                       PkPointF(10, 20), PkPointF(10, 20), PkPointF(10, 20),
                       Pk::LeftButton, Pk::LeftButton, Pk::NoModifier);
    KoPointerEvent start(press, PkPointF(10, 20));
    tool.beginPrimaryAction(&start);
    QVERIFY(canvas.rightClickCallback());
    QVERIFY(!canvas.rightClickCallback());

    tool.deactivate();
    QCOMPARE(canvas.clearedActionReceiver, static_cast<const void *>(&tool));
    QVERIFY(canvas.actionCallbacks.empty());
    QVERIFY(!canvas.rightClickAttached);
    QVERIFY(!canvas.rightClickCallback);
}

void KisAsyncColorSamplerHelperTest::hostCallbacksDropDispatchAfterDirectToolDestruction()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    EllipsePreviewCanvas canvas(image);
    setCurrentNode(canvas, layer);

    auto *tool = new PolylinePreviewTool(&canvas);
    tool->activate({});
    QVERIFY(canvas.dispatchAction(PkString("undo_polygon_selection")));
    delete tool;

    bool rightClickInvoked = false;
    QVERIFY(!canvas.dispatchAction(PkString("undo_polygon_selection")));
    QVERIFY(!canvas.dispatchRightClick(&rightClickInvoked));
    QVERIFY(!rightClickInvoked);
}

void KisAsyncColorSamplerHelperTest::priorityEventFilterUsesPkIdentity()
{
    EllipsePreviewCanvas canvas;
    PkObject filter;

    canvas.toolSetPriorityEventFilter(&filter, true);
    QCOMPARE(canvas.priorityEventFilter, &filter);
    QVERIFY(canvas.priorityEventFilterAttached);

    canvas.toolSetPriorityEventFilter(&filter, false);
    QCOMPARE(canvas.priorityEventFilter, &filter);
    QVERIFY(!canvas.priorityEventFilterAttached);
}

void KisAsyncColorSamplerHelperTest::hostKeyAdapterDispatchesPkPayload()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    EllipsePreviewCanvas canvas(image);
    KeyEventProbeTool tool(&canvas);
    DecorationProxy hostProxy(&canvas);
    hostProxy.priv()->activeTool = &tool;

    PkToolKeyEvent press(Pk::Key_Control,
                         Pk::KeyboardModifiers(Pk::ShiftModifier | Pk::AltModifier),
                         false);
    hostProxy.keyPressEvent(press);
    QCOMPARE(tool.pressCount, 1);
    QCOMPARE(tool.lastKey, Pk::Key_Control);
    QCOMPARE(int(tool.lastModifiers), int(press.modifiers()));
    QVERIFY(press.isAccepted());

    PkToolKeyEvent release(Pk::Key_Shift, Pk::ControlModifier, true);
    hostProxy.keyReleaseEvent(release);
    QCOMPARE(tool.releaseCount, 1);
    QCOMPARE(tool.lastKey, Pk::Key_Shift);
    QCOMPARE(int(tool.lastModifiers), int(release.modifiers()));
    QVERIFY(!release.isAccepted());
    hostProxy.priv()->activeTool = nullptr;
}

void KisAsyncColorSamplerHelperTest::testWorkerThreadSampleDelivery()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    TestSamplingCanvas canvas(image);
    KisAsyncColorSamplerHelper helper(&canvas, &canvas);
    KisColorSamplerStrokeStrategy strategy(1, 100);
    int deliveries = 0;
    PkObject::connect(&helper, &KisAsyncColorSamplerHelper::sigFinalColorSelected,
                      &helper, [&deliveries](const KoColor &) { ++deliveries; });
    helper.connectSamplerStrategy(&strategy);

    const KoColor sample(Pk::red, image->colorSpace());
    std::thread worker([&strategy, sample] { strategy.sigFinalColorSelected(sample); });
    worker.join();

    QCOMPARE(deliveries, 0);
    QCOMPARE(PkThreadCallQueue::processPendingCalls(), 1);
    QCOMPARE(deliveries, 1);
}

void KisAsyncColorSamplerHelperTest::testWorkerThreadSampleDeliveryAfterHelperDestruction()
{
    KisPaintLayerSP layer;
    KisImageSP image = createImageWithLayer(Pk::black, &layer);
    TestSamplingCanvas canvas(image);
    KisColorSamplerStrokeStrategy strategy(1, 100);
    int deliveries = 0;
    auto *helper = new KisAsyncColorSamplerHelper(&canvas, &canvas);
    PkObject::connect(helper, &KisAsyncColorSamplerHelper::sigFinalColorSelected,
                      helper, [&deliveries](const KoColor &) { ++deliveries; });
    helper->connectSamplerStrategy(&strategy);

    const KoColor sample(Pk::red, image->colorSpace());
    std::thread worker([&strategy, sample] { strategy.sigFinalColorSelected(sample); });
    worker.join();
    delete helper;

    QCOMPARE(PkThreadCallQueue::processPendingCalls(), 1);
    QCOMPARE(deliveries, 0);
}

void KisAsyncColorSamplerHelperTest::proxyDispatchesProductionAsyncSampler()
{
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group("");
    const PkString styleKey("colorSamplerPreviewStyle");
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

    PkInputEvent press(PkInputEvent::MouseButtonPress,
                       PkPointF(2, 3), PkPointF(2, 3), PkPointF(2, 3),
                       Pk::LeftButton, Pk::LeftButton, Pk::NoModifier);
    KoPointerEvent event(press, PkPointF(2, 3));
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
