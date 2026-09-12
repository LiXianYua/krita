/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <PkConnection.h>
#include <PkObject.h>
#include <PkPointer.h>

#include <array>
#include <mutex>
#include <optional>

#include <QCursor>

#include <KisCanvasCursorToken.h>

#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoCanvasResourcesIds.h>
#include <KoPointerEvent.h>
#include <KoSelectedShapesProxySimple.h>
#include <KoShapeControllerBase.h>
#include <KoShapeManager.h>
#include <KoUnit.h>
#include <KoZoomHandler.h>
#include <KisCanvasToolServices.h>
#include <KisColorSamplingCanvas.h>
#include <KoColorSpaceRegistry.h>

#include "kis_paint_layer.h"
#include "kis_paint_device.h"
#include "kis_image.h"
#include "brushengine/kis_paintop_preset.h"
#include "brushengine/kis_paintop_settings.h"
#include "tool/kis_tool.h"
#include "KisToolEncloseAndFill.h"
#include "subtools/KisDynamicDelegatedTool.h"
#include "subtools/KisBrushEnclosingProducer.h"
#include "subtools/KisEllipseEnclosingProducer.h"
#include "subtools/KisLassoEnclosingProducer.h"
#include "subtools/KisPathEnclosingProducer.h"
#include "subtools/KisRectangleEnclosingProducer.h"

namespace
{
class DynamicBase : public KisTool
{
public:
    DynamicBase(KoCanvasBase *canvas, KisCanvasCursorToken cursor)
        : KisTool(canvas, cursor)
    {
    }

    void paint(PkPainter &, const KoViewConverter &) override {}
    void mousePressEvent(KoPointerEvent *) override {}
    void mouseMoveEvent(KoPointerEvent *) override {}
    void mouseReleaseEvent(KoPointerEvent *) override {}
    void pkKeyPressEvent(PkToolKeyEvent *event) override
    {
        ++keyPressCount;
        event->accept();
    }
    void pkKeyReleaseEvent(PkToolKeyEvent *event) override
    {
        ++keyReleaseCount;
        event->ignore();
    }
    void canvasResourceChanged(int key, const PkVariant &value) override
    {
        resourceKey = key;
        resourceValue = value;
        ++resourceChangeCount;
    }
    void useCursor(KisCanvasCursorToken cursor) { cursorTokenChanged(cursor); }
    virtual void requestUpdateOutline(const PkPointF &, const KoPointerEvent *) {}
    int getOutlinePath() const { return 0; }

    int keyPressCount = 0;
    int keyReleaseCount = 0;
    int resourceChangeCount = 0;
    int resourceKey = 0;
    PkVariant resourceValue;
};

using DynamicTool = KisDynamicDelegatedTool<DynamicBase>;
using DelegateTool = DynamicTool::DelegateType;

class MinimalPaintOpSettings final : public KisPaintOpSettings
{
public:
    MinimalPaintOpSettings()
        : KisPaintOpSettings({})
    {
        setProperty("paintop", PkString("test"));
    }

    MinimalPaintOpSettings(const MinimalPaintOpSettings &rhs)
        : KisPaintOpSettings(rhs)
        , m_size(rhs.m_size)
        , m_angle(rhs.m_angle)
    {
    }

    KisPaintOpSettingsSP clone() const override
    {
        return KisPaintOpSettingsSP(new MinimalPaintOpSettings(*this));
    }

    void setPaintOpSize(qreal value) override { m_size = value; }
    qreal paintOpSize() const override { return m_size; }
    void setPaintOpAngle(qreal value) override { m_angle = value; }
    qreal paintOpAngle() const override { return m_angle; }

private:
    qreal m_size {10.0};
    qreal m_angle {0.0};
};

class EncloseShapeController final : public KoShapeControllerBase
{
public:
    PkRectF documentRectInPixels() const override { return {0, 0, 32, 32}; }
    qreal pixelsPerInch() const override { return 72.0; }
};

KoShapeControllerBase *testShapeController()
{
    static EncloseShapeController controller;
    return &controller;
}

class EncloseTestCanvas final : public KoCanvasBase,
                                public KisCanvasToolServices,
                                public KisColorSamplingCanvas
{
public:
    struct RightClickRecord {
        const void *receiverIdentity {nullptr};
        PkCallLifetime receiverLifetime;
        std::function<bool()> callback;
        bool attached {false};
    };

    EncloseTestCanvas()
        : KoCanvasBase(testShapeController())
        , m_shapeManager(new KoShapeManager(this))
        , m_selectedShapesProxy(new KoSelectedShapesProxySimple(m_shapeManager.data()))
    {
        m_converter.setResolution(1.0, 1.0);
        m_converter.setZoomedResolution(1.0, 1.0);
        const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
        m_image = KisImageSP(new KisImage(nullptr, 32, 32, colorSpace, "enclose test"));
        m_preset = KisPaintOpPresetSP(new KisPaintOpPreset());
        m_preset->setSettings(KisPaintOpSettingsSP(new MinimalPaintOpSettings()));
        resourceManager()->setResource(KoCanvasResource::ForegroundColor,
                                       KoColor(Pk::black, colorSpace));
        resourceManager()->setResource(KoCanvasResource::BackgroundColor,
                                       KoColor(Pk::white, colorSpace));
        resourceManager()->setResource(KoCanvasResource::CurrentPaintOpPreset,
                                       PkVariant::fromValue(m_preset));
    }

    void setCurrentNode(KisNodeSP node)
    {
        resourceManager()->setResource(KoCanvasResource::CurrentKritaNode,
                                       PkVariant::fromValue(KisNodeWSP(node)));
    }

    bool dispatchRightClick(bool *invoked)
    {
        if (!rightClick.attached || !rightClick.callback ||
            !rightClick.receiverLifetime.claim || !rightClick.receiverLifetime.alive) {
            return false;
        }
        std::lock_guard<std::recursive_mutex> guard(*rightClick.receiverLifetime.claim);
        if (!rightClick.receiverLifetime.alive->load(std::memory_order_acquire)) {
            return false;
        }
        *invoked = true;
        return rightClick.callback();
    }

    void gridSize(PkPointF *, PkSizeF *) const override {}
    bool snapToGrid() const override { return false; }
    void setCursor(KisCanvasCursorToken) override {}
    void addCommand(KUndo2Command *) override {}
    KoShapeManager *shapeManager() const override { return m_shapeManager.data(); }
    KoSelectedShapesProxy *selectedShapesProxy() const override
    {
        return m_selectedShapesProxy.data();
    }
    void updateCanvas(const PkRectF &) override {}
    KoToolProxy *toolProxy() const override { return nullptr; }
    const KoViewConverter *viewConverter() const override { return &m_converter; }
    KoViewConverter *viewConverter() override { return &m_converter; }
    QWidget *canvasWidget() override { return nullptr; }
    const QWidget *canvasWidget() const override { return nullptr; }
    KoUnit unit() const override { return KoUnit(KoUnit::Millimeter); }

    KisImageWSP toolImage() const override { return m_image; }
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
    PkPointF toolWidgetCenterInWidgetPixels() const override { return {}; }
    PkPointF toolDocumentToWidget(const PkPointF &point) const override { return point; }
    PkPointF toolDocumentToAlignedImagePixel(const PkPointF &point) const override { return point; }
    PkTransform toolImageToViewTransform() const override { return {}; }
    void drawToolOutline(PkPainter *, const KisOptimizedBrushOutline &, int) override {}
    bool toolBlockUntilOperationsFinished(KisImageWSP) override { return true; }
    void toolBlockUntilOperationsFinishedForced(KisImageWSP) override {}
    bool toolSelectionEditable() const override { return true; }
    KisCanvasToolSignals *toolSignals() override { return &toolSignalBus; }
    KisPaintOpPresetSP toolCurrentPaintOpPreset() const override { return m_preset; }
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
    bool toolSelectShapeCrossLayer(const PkPointF &,
                                   const PkString &,
                                   bool) override { return false; }
    void toolUpdateCanvas() override {}
    void toolSetActionCallback(const PkString &,
                               const void *,
                               PkCallLifetime,
                               std::function<void()>,
                               bool) override {}
    void toolClearActionCallbacks(const void *) override {}
    void toolSetPriorityRightClickCallback(const void *receiverIdentity,
                                           PkCallLifetime receiverLifetime,
                                           std::function<bool()> callback,
                                           bool attached) override
    {
        rightClick = {receiverIdentity, std::move(receiverLifetime),
                      std::move(callback), attached};
        if (attached) {
            ++rightClickAttachCount;
        } else {
            ++rightClickDetachCount;
        }
    }
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

    RightClickRecord rightClick;
    int rightClickAttachCount {0};
    int rightClickDetachCount {0};
    KisCanvasToolSignals toolSignalBus;

private:
    mutable KoZoomHandler m_converter;
    KisImageSP m_image;
    KisPaintOpPresetSP m_preset;
    PkScopedPointer<KoShapeManager> m_shapeManager;
    PkScopedPointer<KoSelectedShapesProxySimple> m_selectedShapesProxy;
};

class PathProducerProbe final : public KisPathEnclosingProducer
{
public:
    using KisPathEnclosingProducer::KisPathEnclosingProducer;
};

class EncloseToolProbe final : public KisToolEncloseAndFill
{
public:
    using KisToolEncloseAndFill::KisToolEncloseAndFill;

    void slot_delegateTool_enclosingMaskProduced(KisPixelSelectionSP mask) override
    {
        ++maskDeliveryCount;
        lastMask = mask;
    }

    int maskDeliveryCount {0};
    KisPixelSelectionSP lastMask;
};
}

class KisDynamicDelegatedToolTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void forwardsAllNotificationsAcrossReplacement();
    void forwardsKeyAndResourceEventsAcrossReplacement();
    void disconnectsWhenSenderOrReceiverDies();
    void ownerNamesAndProducerCursorDeliveryUsePkPaths();
    void currentNodeAndColorSpaceSubscriptionFollowsActivation();
    void concreteDelegateReplacementPreservesMaskAndResourceDelivery();
    void pathPriorityRightClickRegistrationFollowsToolLifetime();
};

void KisDynamicDelegatedToolTest::forwardsAllNotificationsAcrossReplacement()
{
    DynamicTool tool(nullptr);
    PkObject observer;
    PkString activationId;
    KisCanvasCursorToken cursor;
    bool hasSelection = false;
    PkString statusText;
    int activationCount = 0;
    int cursorCount = 0;
    int selectionCount = 0;
    int statusCount = 0;

    PkObject::connect(&tool, &KoToolBase::activateTool,
                      &observer, [&](const PkString &value) {
                          activationId = value;
                          ++activationCount;
                      });
    PkObject::connect(&tool, &KoToolBase::cursorTokenChanged,
                      &observer, [&](KisCanvasCursorToken value) {
                          cursor = value;
                          ++cursorCount;
                      });
    PkObject::connect(&tool, &KoToolBase::selectionChanged,
                      &observer, [&](bool value) {
                          hasSelection = value;
                          ++selectionCount;
                      });
    PkObject::connect(&tool, &KoToolBase::statusTextChanged,
                      &observer, [&](const PkString &value) {
                          statusText = value;
                          ++statusCount;
                      });

    auto *first = new DelegateTool(nullptr, {});
    PkPointer<DelegateTool> firstGuard(first);
    tool.setDelegateTool(first);
    first->activateTool("first-tool");
    first->cursorTokenChanged(KisCanvasCursorToken(1));
    first->selectionChanged(true);
    first->statusTextChanged("first-status");

    QCOMPARE(activationId, PkString("first-tool"));
    QCOMPARE(cursor.value(), std::uint64_t(1));
    QVERIFY(hasSelection);
    QCOMPARE(statusText, PkString("first-status"));
    QCOMPARE(activationCount, 1);
    QCOMPARE(cursorCount, 1);
    QCOMPARE(selectionCount, 1);
    QCOMPARE(statusCount, 1);

    auto *second = new DelegateTool(nullptr, {});
    PkPointer<DelegateTool> secondGuard(second);
    tool.setDelegateTool(second);
    QVERIFY(firstGuard.isNull());
    second->activateTool("second-tool");
    second->cursorTokenChanged(KisCanvasCursorToken(2));
    second->selectionChanged(false);
    second->statusTextChanged("second-status");

    QCOMPARE(activationId, PkString("second-tool"));
    QCOMPARE(cursor.value(), std::uint64_t(2));
    QVERIFY(!hasSelection);
    QCOMPARE(statusText, PkString("second-status"));
    QCOMPARE(activationCount, 2);
    QCOMPARE(cursorCount, 2);
    QCOMPARE(selectionCount, 2);
    QCOMPARE(statusCount, 2);

    tool.setDelegateTool(nullptr);
    QVERIFY(secondGuard.isNull());
}

void KisDynamicDelegatedToolTest::forwardsKeyAndResourceEventsAcrossReplacement()
{
    DynamicTool tool(nullptr);

    auto *first = new DelegateTool(nullptr, {});
    tool.setDelegateTool(first);

    PkToolKeyEvent pressEvent(Pk::Key_A, Pk::ControlModifier, false);
    tool.pkKeyPressEvent(&pressEvent);
    QVERIFY(pressEvent.isAccepted());
    QCOMPARE(first->keyPressCount, 1);

    PkToolKeyEvent releaseEvent(Pk::Key_A, Pk::ControlModifier, true);
    tool.pkKeyReleaseEvent(&releaseEvent);
    QVERIFY(!releaseEvent.isAccepted());
    QCOMPARE(first->keyReleaseCount, 1);

    tool.canvasResourceChanged(17, PkVariant(PkString("first-resource")));
    QCOMPARE(first->resourceChangeCount, 1);
    QCOMPARE(first->resourceKey, 17);
    QCOMPARE(first->resourceValue.toString(), PkString("first-resource"));

    auto *second = new DelegateTool(nullptr, {});
    tool.setDelegateTool(second);

    PkToolKeyEvent replacementPress(Pk::Key_Return, Pk::ShiftModifier, false);
    tool.pkKeyPressEvent(&replacementPress);
    QVERIFY(replacementPress.isAccepted());
    QCOMPARE(second->keyPressCount, 1);

    tool.canvasResourceChanged(23, PkVariant(PkString("second-resource")));
    QCOMPARE(second->resourceChangeCount, 1);
    QCOMPARE(second->resourceKey, 23);
    QCOMPARE(second->resourceValue.toString(), PkString("second-resource"));
}

void KisDynamicDelegatedToolTest::disconnectsWhenSenderOrReceiverDies()
{
    auto *sender = new DelegateTool(nullptr, {});
    auto *receiver = new DynamicTool(nullptr);
    PkConnection senderLifetime =
        PkObject::connect(sender, &KoToolBase::statusTextChanged,
                          receiver, &KoToolBase::statusTextChanged);
    QVERIFY(senderLifetime.isValid());
    delete sender;
    QVERIFY(!senderLifetime.isValid());

    sender = new DelegateTool(nullptr, {});
    PkConnection receiverLifetime =
        PkObject::connect(sender, &KoToolBase::statusTextChanged,
                          receiver, &KoToolBase::statusTextChanged);
    QVERIFY(receiverLifetime.isValid());
    delete receiver;
    QVERIFY(!receiverLifetime.isValid());

    sender->statusTextChanged("ignored-after-receiver-destruction");
    delete sender;
}

void KisDynamicDelegatedToolTest::ownerNamesAndProducerCursorDeliveryUsePkPaths()
{
    EncloseTestCanvas canvas;
    KisToolEncloseAndFill tool(&canvas);
    KisRectangleEnclosingProducer rectangle(&canvas);
    KisEllipseEnclosingProducer ellipse(&canvas);
    KisPathEnclosingProducer path(&canvas);
    KisLassoEnclosingProducer lasso(&canvas);
    KisBrushEnclosingProducer brush(&canvas);

    QCOMPARE(tool.objectName(), PkString("tool_enclose_and_fill"));
    QCOMPARE(rectangle.objectName(), PkString("enclosing_tool_rectangle"));
    QCOMPARE(ellipse.objectName(), PkString("enclosing_tool_rectangle"));
    QCOMPARE(path.objectName(), PkString("enclosing_tool_path"));
    QCOMPARE(lasso.objectName(), PkString("enclosing_tool_lasso"));
    QCOMPARE(brush.objectName(), PkString("enclosing_tool_brush"));

    PkObject observer;
    std::array<int, 5> cursorDeliveries {};
    std::array<KoToolBase *, 5> producers {
        &rectangle, &ellipse, &path, &lasso, &brush
    };
    for (std::size_t i = 0; i < producers.size(); ++i) {
        PkObject::connect(producers[i], &KoToolBase::cursorTokenChanged,
                          &observer, [&, i](KisCanvasCursorToken) {
                              ++cursorDeliveries[i];
                          });
    }

    canvas.toolSignalBus.effectiveCompositeOpChanged();
    QCOMPARE(cursorDeliveries, (std::array<int, 5> {1, 1, 1, 1, 1}));
}

void KisDynamicDelegatedToolTest::currentNodeAndColorSpaceSubscriptionFollowsActivation()
{
    EncloseTestCanvas canvas;
    KisToolEncloseAndFill tool(&canvas);
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();

    KisPaintLayerSP inactiveNode = new KisPaintLayer(nullptr, "inactive", OPACITY_OPAQUE_U8, colorSpace);
    canvas.setCurrentNode(inactiveNode);
    QVERIFY(tool.m_previousNode.isNull());
    QVERIFY(!PkObject::disconnect(inactiveNode->paintDevice().data(),
                                  &KisPaintDevice::colorSpaceChanged,
                                  &tool,
                                  &KisToolEncloseAndFill::slot_colorSpaceChanged));

    KisPaintLayerSP activeNode = new KisPaintLayer(nullptr, "active", OPACITY_OPAQUE_U8, colorSpace);
    canvas.setCurrentNode(activeNode);
    tool.activate({});
    QCOMPARE(tool.m_previousNode, KisNodeSP(activeNode));
    QVERIFY(PkObject::disconnect(activeNode->paintDevice().data(),
                                 &KisPaintDevice::colorSpaceChanged,
                                 &tool,
                                 &KisToolEncloseAndFill::slot_colorSpaceChanged));

    KisPaintLayerSP replacementNode =
        new KisPaintLayer(nullptr, "active-replacement", OPACITY_OPAQUE_U8, colorSpace);
    canvas.setCurrentNode(replacementNode);
    QCOMPARE(tool.m_previousNode, KisNodeSP(replacementNode));
    QVERIFY(PkObject::disconnect(replacementNode->paintDevice().data(),
                                 &KisPaintDevice::colorSpaceChanged,
                                 &tool,
                                 &KisToolEncloseAndFill::slot_colorSpaceChanged));
    tool.deactivate();
    QVERIFY(tool.m_previousNode.isNull());

    KisPaintLayerSP postDeactivateNode =
        new KisPaintLayer(nullptr, "post-deactivate", OPACITY_OPAQUE_U8, colorSpace);
    canvas.setCurrentNode(postDeactivateNode);
    QVERIFY(tool.m_previousNode.isNull());
    QVERIFY(!PkObject::disconnect(postDeactivateNode->paintDevice().data(),
                                  &KisPaintDevice::colorSpaceChanged,
                                  &tool,
                                  &KisToolEncloseAndFill::slot_colorSpaceChanged));
}

void KisDynamicDelegatedToolTest::concreteDelegateReplacementPreservesMaskAndResourceDelivery()
{
    EncloseTestCanvas canvas;
    EncloseToolProbe tool(&canvas);
    tool.m_enclosingMethod = KisToolEncloseAndFill::Lasso;
    tool.setupEnclosingSubtool();

    auto *firstProducer = reinterpret_cast<KisLassoEnclosingProducer *>(tool.delegateTool());
    QVERIFY(firstProducer);
    PkPointer<KisLassoEnclosingProducer> firstProducerGuard(firstProducer);
    PkObject observer;
    int firstResourceDeliveryCount = 0;
    PkObject::connect(firstProducer, &KoToolBase::cursorTokenChanged,
                      &observer, [&](KisCanvasCursorToken) {
                          ++firstResourceDeliveryCount;
                      });

    KisPixelSelectionSP firstMask(new KisPixelSelection());
    firstProducer->enclosingMaskProduced(firstMask);
    KisPaintLayerSP firstResourceNode =
        new KisPaintLayer(nullptr, "first-resource", OPACITY_OPAQUE_U8,
                          KoColorSpaceRegistry::instance()->rgb8());
    tool.canvasResourceChanged(
        KoCanvasResource::CurrentKritaNode,
        PkVariant::fromValue(KisNodeWSP(firstResourceNode)));

    QCOMPARE(tool.maskDeliveryCount, 1);
    QCOMPARE(tool.lastMask, firstMask);
    QVERIFY(firstResourceDeliveryCount > 0);

    tool.m_enclosingMethod = KisToolEncloseAndFill::Rectangle;
    tool.setupEnclosingSubtool();
    QVERIFY(firstProducerGuard.isNull());
    const int firstResourceDeliveryCountAfterReplacement =
        firstResourceDeliveryCount;

    auto *replacementProducer =
        reinterpret_cast<KisRectangleEnclosingProducer *>(tool.delegateTool());
    QVERIFY(replacementProducer);
    int replacementResourceDeliveryCount = 0;
    PkObject::connect(replacementProducer, &KoToolBase::cursorTokenChanged,
                      &observer, [&](KisCanvasCursorToken) {
                          ++replacementResourceDeliveryCount;
                      });

    KisPixelSelectionSP replacementMask(new KisPixelSelection());
    replacementProducer->enclosingMaskProduced(replacementMask);
    KisPaintLayerSP replacementResourceNode =
        new KisPaintLayer(nullptr, "replacement-resource", OPACITY_OPAQUE_U8,
                          KoColorSpaceRegistry::instance()->rgb8());
    tool.canvasResourceChanged(
        KoCanvasResource::CurrentKritaNode,
        PkVariant::fromValue(KisNodeWSP(replacementResourceNode)));

    QCOMPARE(tool.maskDeliveryCount, 2);
    QCOMPARE(tool.lastMask, replacementMask);
    QCOMPARE(firstResourceDeliveryCount,
             firstResourceDeliveryCountAfterReplacement);
    QVERIFY(replacementResourceDeliveryCount > 0);
}

void KisDynamicDelegatedToolTest::pathPriorityRightClickRegistrationFollowsToolLifetime()
{
    EncloseTestCanvas canvas;
    KisPaintLayerSP pathNode =
        new KisPaintLayer(nullptr, "path", OPACITY_OPAQUE_U8,
                          KoColorSpaceRegistry::instance()->rgb8());
    canvas.setCurrentNode(pathNode);
    PathProducerProbe producer(&canvas);
    producer.activate({});

    QCOMPARE(canvas.rightClickAttachCount, 1);
    QVERIFY(canvas.rightClick.attached);
    QCOMPARE(canvas.rightClick.receiverIdentity, static_cast<const void *>(&producer));

    bool invoked = false;
    QVERIFY(!canvas.dispatchRightClick(&invoked));
    QVERIFY(invoked);

    KoPointerEvent firstPress(PkPoint(0, 0), PkPointF(0, 0),
                              Pk::LeftButton, Pk::LeftButton, Pk::NoModifier);
    KoPointerEvent firstRelease(PkPoint(0, 0), PkPointF(0, 0),
                                Pk::LeftButton, Pk::NoButton, Pk::NoModifier);
    producer.beginPrimaryAction(&firstPress);
    producer.endPrimaryAction(&firstRelease);

    KoPointerEvent peakMove(PkPoint(100, 100), PkPointF(100, 100),
                            Pk::NoButton, Pk::NoButton, Pk::NoModifier);
    producer.continuePrimaryAction(&peakMove);
    KoPointerEvent peakPress(PkPoint(100, 100), PkPointF(100, 100),
                             Pk::LeftButton, Pk::LeftButton, Pk::NoModifier);
    KoPointerEvent peakRelease(PkPoint(100, 100), PkPointF(100, 100),
                               Pk::LeftButton, Pk::NoButton, Pk::NoModifier);
    producer.beginPrimaryAction(&peakPress);
    producer.endPrimaryAction(&peakRelease);

    KoPointerEvent endMove(PkPoint(200, 0), PkPointF(200, 0),
                           Pk::NoButton, Pk::NoButton, Pk::NoModifier);
    producer.continuePrimaryAction(&endMove);
    QVERIFY(producer.hasUserInteractionRunning());
    const PkRectF geometryBeforeRightClick =
        producer.localTool()->decorationsRect();
    QVERIFY(geometryBeforeRightClick.contains(PkPointF(100, 100)));

    invoked = false;
    QVERIFY(canvas.dispatchRightClick(&invoked));
    QVERIFY(invoked);
    const PkRectF geometryAfterRightClick =
        producer.localTool()->decorationsRect();
    QVERIFY(!geometryAfterRightClick.contains(PkPointF(100, 100)));

    producer.deactivate();
    QCOMPARE(canvas.rightClickDetachCount, 1);
    QVERIFY(!canvas.rightClick.attached);
    invoked = false;
    QVERIFY(!canvas.dispatchRightClick(&invoked));
    QVERIFY(!invoked);

    auto *ephemeral = new PathProducerProbe(&canvas);
    ephemeral->activate({});
    QCOMPARE(canvas.rightClick.receiverIdentity, static_cast<const void *>(ephemeral));
    delete ephemeral;

    invoked = false;
    QVERIFY(!canvas.dispatchRightClick(&invoked));
    QVERIFY(!invoked);
}

SIMPLE_TEST_MAIN(KisDynamicDelegatedToolTest)

#include "KisDynamicDelegatedToolTest.moc"
