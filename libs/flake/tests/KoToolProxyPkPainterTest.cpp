/*
 * SPDX-FileCopyrightText: 2026 S-09-g
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QTest>

#include "KoCanvasBase.h"
#include "KoCanvasController.h"
#include "KoShapeControllerBase.h"
#include "KoToolBase.h"
#include "KoToolProxy.h"
#include "KoToolProxy_p.h"
#include "KoUnit.h"
#include "KoViewConverter.h"
#include "KoShapeManager.h"
#include "KoSelectedShapesProxySimple.h"
#include "KoPathShape.h"
#include "KoPointerEvent.h"
#include "tools/KoPathTool.h"
#include "tools/KoPathToolSelection.h"
#include "tools/KoPencilTool.h"
#include "shapes/RectangleShape.h"
#include "commands/KoParameterHandleMoveCommand.h"

#include <vector>
#include <chrono>
#include <optional>
#include <thread>
#include <type_traits>

#include <PkPaintCommand.h>
#include <PkPainter.h>
#include <PkSize.h>
#include <PkThreadCallQueue.h>
#include <PkTimer.h>

namespace
{
class RecordingBackend final : public PkPainterBackend
{
public:
    void submit(const PkPaintCommand &command) override
    {
        lastCommand = command;
        ++commandCount;
        commands.push_back(command);
    }

    PkPaintCommand lastCommand {PkSaveCommand {}};
    int commandCount = 0;
    std::vector<PkPaintCommand> commands;
};

class MinimalShapeController final : public KoShapeControllerBase
{
public:
    PkRectF documentRectInPixels() const override
    {
        return PkRectF(0.0, 0.0, 100.0, 100.0);
    }

    qreal pixelsPerInch() const override
    {
        return 72.0;
    }
};

class MinimalCanvas final : public KoCanvasBase
{
public:
    explicit MinimalCanvas(KoShapeControllerBase *shapeController)
        : KoCanvasBase(shapeController)
        , manager(this)
        , selectedShapes(&manager)
    {
    }

    void gridSize(PkPointF *, PkSizeF *) const override {}
    bool snapToGrid() const override { return false; }
    void setCursor(const QCursor &) override {}
    void addCommand(KUndo2Command *) override {}
    KoShapeManager *shapeManager() const override { return const_cast<KoShapeManager *>(&manager); }
    KoSelectedShapesProxy *selectedShapesProxy() const override { return const_cast<KoSelectedShapesProxySimple *>(&selectedShapes); }
    void updateCanvas(const PkRectF &) override {}
    KoToolProxy *toolProxy() const override { return nullptr; }
    const KoViewConverter *viewConverter() const override { return &converter; }
    KoViewConverter *viewConverter() override { return &converter; }
    QWidget *canvasWidget() override { return nullptr; }
    const QWidget *canvasWidget() const override { return nullptr; }
    KoUnit unit() const override { return KoUnit(KoUnit::Millimeter); }

    KoShapeManager manager;
    KoSelectedShapesProxySimple selectedShapes;
    KoViewConverter converter;
};

class MinimalController final : public KoCanvasController
{
public:
    MinimalController()
        : KoCanvasController(nullptr)
    {
    }

    void setCanvas(KoCanvasBase *value) override { m_canvas = value; }
    KoCanvasBase *canvas() const override { return m_canvas; }
    void ensureVisibleDoc(const PkRectF &rect, bool smooth) override
    {
        lastVisibleRect = rect;
        lastSmooth = smooth;
        m_preferredCenter += PkPointF(1.0, 1.0);
        ++ensureVisibleCalls;
    }
    void zoomIn(const KoViewTransformStillPoint &) override {}
    void zoomIn() override {}
    void zoomOut(const KoViewTransformStillPoint &) override {}
    void zoomOut() override {}
    void zoomTo(const PkRect &) override {}
    void setZoom(KoZoomMode::Mode, qreal) override {}
    void setPreferredCenter(const PkPointF &point) override { m_preferredCenter = point; }
    PkPointF preferredCenter() const override { return m_preferredCenter; }
    void pan(const PkPoint &) override {}
    void panUp() override {}
    void panDown() override {}
    void panLeft() override {}
    void panRight() override {}
    PkPoint scrollBarValue() const override { return {}; }
    void setScrollBarValue(const PkPoint &) override {}
    void resetScrollBars() override {}
    PkPointF currentCursorPosition() const override { return {}; }
    KoZoomState zoomState() const override { return {}; }

    KoCanvasBase *m_canvas = nullptr;
    PkPointF m_preferredCenter;
    PkRectF lastVisibleRect;
    bool lastSmooth = false;
    int ensureVisibleCalls = 0;
};

class TestToolProxy final : public KoToolProxy
{
public:
    explicit TestToolProxy(KoCanvasBase *canvas)
        : KoToolProxy(canvas)
    {
    }

protected:
    PkPointF widgetToDocument(const PkPointF &point) const override
    {
        return point;
    }

    PkPointF documentToWidget(const PkPointF &point) const override
    {
        return point;
    }
};

class PkOnlyTool final : public KoToolBase
{
public:
    using KoToolBase::paint;

    explicit PkOnlyTool(KoCanvasBase *canvas)
        : KoToolBase(canvas)
    {
    }

    void paint(PkPainter &painter, const KoViewConverter &) override
    {
        reached = true;
        painter.drawPoint(PkPointF(3.0, 4.0));
    }

    void mousePressEvent(KoPointerEvent *) override {}
    void mouseMoveEvent(KoPointerEvent *event) override
    {
        ++mouseMoveCalls;
        lastMouseMovePoint = event->point;
        lastMouseMoveButtons = event->buttons();
    }
    void mouseReleaseEvent(KoPointerEvent *) override { ++mouseReleaseCalls; }

    bool reached = false;
    int mouseMoveCalls = 0;
    int mouseReleaseCalls = 0;
    PkPointF lastMouseMovePoint;
    Qt::MouseButtons lastMouseMoveButtons;
};

class PencilPreviewTool final : public KoPencilTool
{
public:
    using KoPencilTool::KoPencilTool;
    using KoPencilTool::setStrokeColor;
    ~PencilPreviewTool() override { delete path(); }
};

// Taking the member address without a cast fails if a second dispatch overload returns.
static_assert(std::is_same_v<decltype(&KoToolProxy::paint),
                            void (KoToolProxy::*)(PkPainter &, const KoViewConverter &)>);
static_assert(std::is_same_v<decltype(&KoToolBase::paint),
                            void (KoToolBase::*)(PkPainter &, const KoViewConverter &)>);
static_assert(std::is_same_v<decltype(KoToolProxyPrivate::scrollTimer), PkTimer>);
static_assert(std::is_same_v<decltype(KoToolProxyPrivate::lastPointerEvent),
                            std::optional<KoPointerEvent>>);
}

class KoToolProxyPkPainterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void controllerNotificationsCarryNativeGeometry()
    {
        MinimalController controller;
        PkObject receiver;
        PkSize size;
        PkPointF oldOffset, newOffset;
        int deliveries = 0;
        PkObject::connect(controller.proxyObject.data(), &KoCanvasControllerProxyObject::sizeChanged,
                          &receiver, [&](const PkSize &value) { size = value; ++deliveries; });
        PkObject::connect(controller.proxyObject.data(), &KoCanvasControllerProxyObject::moveDocumentOffset,
                          &receiver, [&](const PkPointF &oldValue, const PkPointF &newValue) {
            oldOffset = oldValue; newOffset = newValue; ++deliveries;
        });
        controller.proxyObject->emitSizeChanged(PkSize(640, 480));
        controller.proxyObject->emitMoveDocumentOffset(PkPointF(1, 2), PkPointF(30, 40));
        QCOMPARE(deliveries, 2);
        QCOMPARE(size, PkSize(640, 480));
        QCOMPARE(oldOffset, PkPointF(1, 2));
        QCOMPARE(newOffset, PkPointF(30, 40));
        receiver.disconnect();
        controller.proxyObject->emitSizeChanged(PkSize(100, 100));
        QCOMPARE(deliveries, 2);
    }

    void parameterHandleKeepsPkModifiersThroughUndo()
    {
        RectangleShape shape;
        shape.setSize(PkSizeF(100, 80));
        const PkPointF start = shape.shapeToDocument(shape.handlePosition(0));
        const PkPointF end = shape.shapeToDocument(PkPointF(75, 0));
        KoParameterHandleMoveCommand command(&shape, 0, start, end, Pk::ControlModifier);
        command.redo();
        QCOMPARE(shape.handlePosition(0), PkPointF(75, 0));
        QCOMPARE(shape.handlePosition(1), PkPointF(100, 0));
        command.undo();
        QCOMPARE(shape.handlePosition(0), PkPointF(100, 0));
        command.redo();
        QCOMPARE(shape.handlePosition(0), PkPointF(75, 0));
        QCOMPARE(shape.handlePosition(1), PkPointF(100, 0));

        KoParameterHandleMoveCommand otherModifiers(&shape, 0, start, end, Pk::NoModifier);
        QVERIFY(!command.mergeWith(&otherModifiers));
        otherModifiers.redo();
        QCOMPARE(shape.handlePosition(1), PkPointF(100, 25));
    }

    void initTestCase()
    {
        PkThreadCallQueue::warmUpCurrentThread();
    }

    void cleanup()
    {
        PkThreadCallQueue::processPendingCalls();
    }

    void detachedLastEventOutlivesHostEvent()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        TestToolProxy proxy(&canvas);
        PkOnlyTool tool(&canvas);
        proxy.priv()->activeTool = &tool;

        {
            QMouseEvent move(QEvent::MouseMove, QPointF(7, 9), QPointF(17, 19),
                             Qt::NoButton, Qt::LeftButton, Qt::ShiftModifier);
            proxy.mouseMoveEvent(&move, PkPointF(70, 90));
        }

        const KoPointerEvent *saved = proxy.lastDeliveredPointerEvent();
        QVERIFY(saved);
        QCOMPARE(saved->point, PkPointF(70, 90));
        QCOMPARE(saved->pos(), PkPoint(7, 9));
        QCOMPARE(saved->globalPos(), PkPoint(17, 19));
        QCOMPARE(saved->buttons(), Qt::LeftButton);
        QCOMPARE(saved->modifiers(), Qt::ShiftModifier);
    }

    void autoScrollPkTimerFiresOnceAndReleaseCancelsRepeat()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        MinimalController controller;
        controller.setCanvas(&canvas);
        TestToolProxy proxy(&canvas);
        PkOnlyTool tool(&canvas);
        proxy.priv()->activeTool = &tool;
        proxy.priv()->controller = &controller;

        QMouseEvent move(QEvent::MouseMove, QPointF(25, 35),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        KoPointerEvent pointer(&move, PkPointF(25, 35));
        proxy.mousePressEvent(&pointer);
        proxy.mouseMoveEvent(&pointer);
        QVERIFY(proxy.priv()->scrollTimer.isActive());

        // A stalled host must not accumulate one callback per elapsed interval.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        QCOMPARE(PkThreadCallQueue::pendingCount(), size_t(1));
        QEvent hostPulse(QEvent::User);
        proxy.processEvent(&hostPulse);
        QCOMPARE(PkThreadCallQueue::pendingCount(), size_t(0));
        QCOMPARE(controller.ensureVisibleCalls, 1);
        QCOMPARE(tool.mouseMoveCalls, 2);
        QCOMPARE(tool.lastMouseMovePoint, PkPointF(25, 35));
        QCOMPARE(tool.lastMouseMoveButtons, Qt::LeftButton);

        proxy.mouseReleaseEvent(&pointer);
        QVERIFY(!proxy.priv()->scrollTimer.isActive());
        PkThreadCallQueue::processPendingCalls();
        QCOMPARE(tool.mouseMoveCalls, 2);
        QCOMPARE(tool.mouseReleaseCalls, 1);
    }

    void pathSelectionDecorationsUsePkDispatch()
    {
        MinimalShapeController controller;
        MinimalCanvas canvas(&controller);
        TestToolProxy proxy(&canvas);
        KoPathShape shape;
        auto *point = shape.moveTo(PkPointF(10.0, 20.0));
        shape.lineTo(PkPointF(30.0, 20.0));
        KoPathTool tool(&canvas);
        auto *selection = dynamic_cast<KoPathToolSelection *>(tool.selection());
        QVERIFY(selection);
        selection->setSelectedShapes({&shape});
        selection->add(point, false);
        proxy.priv()->activeTool = &tool;

        RecordingBackend backend;
        PkPainter painter(backend);
        proxy.paint(painter, *canvas.viewConverter());
        selection->setSelectedShapes({});
        selection->clear();

        int handles = 0;
        for (const auto &command : backend.commands) {
            if (const auto *polygon = std::get_if<PkDrawPolygonCommand>(&command)) {
                if (polygon->polygon.boundingRect().center() == PkPointF(10.0, 20.0)) {
                    ++handles;
                }
            }
        }
        QVERIFY(handles > 0);
        QCOMPARE(painter.transform(), PkTransform());
    }

    void pencilStrokePreviewUsesPkDispatch()
    {
        MinimalShapeController controller;
        MinimalCanvas canvas(&controller);
        TestToolProxy proxy(&canvas);
        PencilPreviewTool tool(&canvas);
        tool.setStrokeTemplate(KoShapeStroke(2.0, Pk::red));
        tool.setStrokeColor(Pk::red);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 20),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        KoPointerEvent start(&press, PkPointF(10, 20));
        tool.mousePressEvent(&start);
        QMouseEvent move(QEvent::MouseMove, QPointF(30, 20),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        KoPointerEvent end(&move, PkPointF(30, 20));
        tool.mouseMoveEvent(&end);
        proxy.priv()->activeTool = &tool;

        RecordingBackend backend;
        PkPainter painter(backend);
        proxy.paint(painter, *canvas.viewConverter());

        int fills = 0;
        for (const auto &command : backend.commands) {
            if (const auto *fill = std::get_if<PkFillPathCommand>(&command)) {
                QVERIFY(!fill->path.isEmpty());
                QCOMPARE(fill->brush.color(), PkColor(Pk::red));
                QVERIFY(fill->path.boundingRect().contains(PkPointF(20, 20)));
                ++fills;
            }
        }
        QCOMPARE(fills, 1);
        QCOMPARE(painter.transform(), PkTransform());
    }

    void dispatchesToPkOnlyToolOverride()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        TestToolProxy proxy(&canvas);
        PkOnlyTool tool(&canvas);
        proxy.priv()->activeTool = &tool;

        RecordingBackend backend;
        PkPainter painter(backend);
        KoViewConverter converter;
        proxy.paint(painter, converter);

        QVERIFY(tool.reached);
        QCOMPARE(backend.commandCount, 1);
        QVERIFY(std::holds_alternative<PkDrawPointCommand>(backend.lastCommand));
        const PkPointF point = std::get<PkDrawPointCommand>(backend.lastCommand).point;
        QCOMPARE(point, PkPointF(3.0, 4.0));
    }
};

QTEST_MAIN(KoToolProxyPkPainterTest)

#include "KoToolProxyPkPainterTest.moc"
