/*
 * SPDX-FileCopyrightText: 2026 S-09-g
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QTest>
#include <PkInputEvent.h>
#include <QKeySequence>

#include "KoCanvasActionHost.h"
#include "KoCanvasBase.h"
#include "KoCanvasController.h"
#include "KoShapeControllerBase.h"
#include "KoToolBase.h"
#include "KoToolFactoryBase.h"
#include "KoToolManager.h"
#include "KoToolProxy.h"
#include "KoToolProxy_p.h"
#include "KoUnit.h"
#include "KoViewConverter.h"
#include "KoShapeManager.h"
#include "KoShapeFactoryBase.h"
#include "KoShapeUserData.h"
#include "KoSelectedShapesProxySimple.h"
#include "KoPathShape.h"
#include "KoPointerEvent.h"
#include "tools/KoPathTool.h"
#include "tools/KoCreatePathTool.h"
#include "tools/KoPathToolSelection.h"
#include "KoToolSelection.h"
#include "tools/KoPencilTool.h"
#include "tools/KoInteractionTool.h"
#include "tools/KoZoomTool.h"
#include "shapes/RectangleShape.h"
#include "text/KoSvgTextPropertiesInterface.h"
#include "commands/KoParameterHandleMoveCommand.h"

#include <vector>
#include <chrono>
#include <optional>
#include <thread>
#include <type_traits>

#include <PkPaintCommand.h>
#include <PkKeySequence.h>
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
    void setCursor(KisCanvasCursorToken) override {}
    void addCommand(KUndo2Command *) override {}
    KoShapeManager *shapeManager() const override { return const_cast<KoShapeManager *>(&manager); }
    KoSelectedShapesProxy *selectedShapesProxy() const override { return const_cast<KoSelectedShapesProxySimple *>(&selectedShapes); }
    void updateCanvas(const PkRectF &rect) override
    {
        lastUpdatedRect = rect;
        ++updateCanvasCalls;
    }
    KoToolProxy *toolProxy() const override { return m_toolProxy; }
    void setToolProxy(KoToolProxy *proxy) { m_toolProxy = proxy; }
    const KoViewConverter *viewConverter() const override { return &converter; }
    KoViewConverter *viewConverter() override { return &converter; }
    QWidget *canvasWidget() override { return nullptr; }
    const QWidget *canvasWidget() const override { return nullptr; }
    KoUnit unit() const override { return KoUnit(KoUnit::Millimeter); }

    KoShapeManager manager;
    KoSelectedShapesProxySimple selectedShapes;
    KoViewConverter converter;
    KoToolProxy *m_toolProxy = nullptr;
    PkRectF lastUpdatedRect;
    int updateCanvasCalls = 0;
};

class MinimalController final : public KoCanvasController
{
public:
    explicit MinimalController(QObject *actionCollection = nullptr)
        : KoCanvasController(actionCollection)
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

    /// KoToolBase keeps the text-mode switch protected; the shortcut
    /// arbitration test drives it directly.
    using KoToolBase::setTextMode;

    explicit PkOnlyTool(KoCanvasBase *canvas)
        : KoToolBase(canvas)
    {
    }

    void watchSelectedShapes(std::function<void()> callback)
    {
        watchSelectedShapesChanged(std::move(callback));
    }

    void requestUpdate(const PkRectF &rect)
    {
        requestCanvasUpdate(rect);
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

    void pkKeyPressEvent(PkToolKeyEvent *event) override
    {
        ++keyPressCalls;
        lastKey = event->key();
        lastModifiers = event->modifiers();
        lastAcceptedOnEntry = event->isAccepted();
        lastAutoRepeat = event->isAutoRepeat();
        lastText = event->text();
        event->accept();
    }

    void pkKeyReleaseEvent(PkToolKeyEvent *event) override
    {
        ++keyReleaseCalls;
        lastKey = event->key();
        lastModifiers = event->modifiers();
        lastAcceptedOnEntry = event->isAccepted();
        lastAutoRepeat = event->isAutoRepeat();
        lastText = event->text();
        event->ignore();
    }

    void inputMethodEvent(PkToolInputMethodEvent *event) override
    {
        ++inputMethodCalls;
        lastCommitString = event->commitString;
        lastPreeditString = event->preeditString;
        lastReplacementStart = event->replacementStart;
        lastReplacementLength = event->replacementLength;
        event->accept();
    }

    void focusInEvent(PkToolEvent *event) override
    {
        ++focusInCalls;
        event->accept();
    }

    void focusOutEvent(PkToolEvent *event) override
    {
        ++focusOutCalls;
        event->ignore();
    }

    void dragMoveEvent(PkToolEvent *event, const PkPointF &point) override
    {
        ++dragMoveCalls;
        lastDragPoint = point;
        event->accept();
    }

    void dropEvent(PkToolEvent *event, const PkPointF &point) override
    {
        ++dropCalls;
        lastDropPoint = point;
        event->accept();
    }

    bool reached = false;
    int mouseMoveCalls = 0;
    int mouseReleaseCalls = 0;
    int keyPressCalls = 0;
    int keyReleaseCalls = 0;
    int inputMethodCalls = 0;
    int focusInCalls = 0;
    int focusOutCalls = 0;
    int dragMoveCalls = 0;
    int dropCalls = 0;
    PkPointF lastMouseMovePoint;
    Pk::MouseButtons lastMouseMoveButtons;
    Pk::Key lastKey = static_cast<Pk::Key>(0);
    Pk::KeyboardModifiers lastModifiers = Pk::NoModifier;
    bool lastAcceptedOnEntry = false;
    bool lastAutoRepeat = false;
    PkString lastText;
    PkString lastCommitString;
    PkString lastPreeditString;
    int lastReplacementStart = 0;
    int lastReplacementLength = 0;
    PkPointF lastDragPoint;
    PkPointF lastDropPoint;
};

class DualCanvasObserver final : public QObject, public PkObject
{
};

class PencilPreviewTool final : public KoPencilTool
{
public:
    using KoPencilTool::KoPencilTool;
    using KoPencilTool::setStrokeColor;
    ~PencilPreviewTool() override { delete path(); }
};

class NativeShortcutFactory final : public KoToolFactoryBase
{
public:
    NativeShortcutFactory()
        : KoToolFactoryBase("native-shortcut-probe")
    {
    }

    KoToolBase *createTool(KoCanvasBase *) override { return nullptr; }
    void setNativeShortcut(const PkKeySequence &shortcut) { setShortcut(shortcut); }
};

class NativeTextPropertiesInterface final : public KoSvgTextPropertiesInterface
{
public:
    PkList<KoSvgTextProperties> getSelectedProperties() override { return {}; }
    PkList<KoSvgTextProperties> getCharacterProperties() override { return {}; }
    KoSvgTextProperties getInheritedProperties() override { return {}; }
    void setPropertiesOnSelected(KoSvgTextProperties,
                                 PkSet<KoSvgTextProperties::PropertyId>) override {}
    void setCharacterPropertiesOnSelected(KoSvgTextProperties,
                                          PkSet<KoSvgTextProperties::PropertyId>) override {}
    bool spanSelection() override { return false; }
    bool characterPropertiesEnabled() override { return true; }
};

// Taking the member address without a cast fails if a second dispatch overload returns.
static_assert(std::is_same_v<decltype(&KoToolProxy::paint),
                            void (KoToolProxy::*)(PkPainter &, const KoViewConverter &)>);
static_assert(std::is_same_v<decltype(&KoToolBase::paint),
                            void (KoToolBase::*)(PkPainter &, const KoViewConverter &)>);
static_assert(std::is_same_v<decltype(KoToolProxyPrivate::scrollTimer), PkTimer>);
static_assert(std::is_same_v<decltype(KoToolProxyPrivate::lastPointerEvent),
                            std::optional<KoPointerEvent>>);
static_assert(std::is_same_v<decltype(&KoToolFactoryBase::shortcut),
                            PkKeySequence (KoToolFactoryBase::*)() const>);
static_assert(std::is_same_v<decltype(&KoToolAction::shortcut),
                            PkKeySequence (KoToolAction::*)() const>);
static_assert(std::is_same_v<decltype(&KoInteractionTool::pkKeyPressEvent),
                            void (KoInteractionTool::*)(PkToolKeyEvent *)>);
static_assert(std::is_same_v<decltype(&KoInteractionTool::pkKeyReleaseEvent),
                            void (KoInteractionTool::*)(PkToolKeyEvent *)>);
static_assert(std::is_same_v<decltype(&KoPencilTool::pkKeyPressEvent),
                            void (KoPencilTool::*)(PkToolKeyEvent *)>);
static_assert(std::is_same_v<decltype(&KoZoomTool::pkKeyPressEvent),
                            void (KoZoomTool::*)(PkToolKeyEvent *)>);
static_assert(std::is_same_v<decltype(&KoZoomTool::pkKeyReleaseEvent),
                            void (KoZoomTool::*)(PkToolKeyEvent *)>);
// KoCanvasBase and KoShapeUserData were moved to a PkObject-only identity by
// 911506ce ("migrate flake lifecycle signals to Pk"), in the same direction the
// three-way Qt split takes every non-UI Qt face. They therefore no longer carry
// the Qt meta-object face, and these two assertions pin that actual state — the
// same form the neighbouring assertions already use.
static_assert(!std::is_base_of_v<QObject, KoCanvasBase>);
static_assert(std::is_base_of_v<PkObject, KoCanvasBase>);
static_assert(!std::is_base_of_v<QObject, KoShapeFactoryBase>);
static_assert(!std::is_base_of_v<QObject, KoShapeUserData>);
static_assert(std::is_base_of_v<PkObject, KoShapeUserData>);
static_assert(!std::is_base_of_v<QObject, KoToolBase>);
static_assert(!std::is_base_of_v<QObject, KoToolFactoryBase>);
static_assert(!std::is_base_of_v<QObject, KoSelectedShapesProxy>);
static_assert(std::is_base_of_v<PkObject, KoSelectedShapesProxy>);
static_assert(std::is_same_v<decltype(&KoToolSelection::qt_metacall),
                            decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KoToolProxy::qt_metacall),
                            decltype(&QObject::qt_metacall)>);
static_assert(std::is_same_v<decltype(&KoSvgTextPropertiesInterface::qt_metacall),
                            decltype(&QObject::qt_metacall)>);
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

    void factoryShortcutUsesNativeQt515ChordEncoding()
    {
        const int nativeChord = static_cast<int>(Pk::ControlModifier) |
                                static_cast<int>(Pk::Key_R);
        const QKeySequence qt515Oracle(QStringLiteral("Ctrl+R"));
        QCOMPARE(nativeChord, qt515Oracle[0]);

        NativeShortcutFactory factory;
        factory.setNativeShortcut(PkKeySequence({nativeChord}));
        const PkKeySequence shortcut = factory.shortcut();
        QCOMPARE(shortcut.size(), 1);
        QCOMPARE(shortcut[0], qt515Oracle[0]);

        KoToolAction action(&factory);
        QCOMPARE(action.shortcut().size(), 1);
        QCOMPARE(action.shortcut()[0], qt515Oracle[0]);
    }

    // 快捷键的宿主载荷 → 桶无关 encoded chord 的编码，已从管理器侧移到实现者
    // KoCanvasController::hostActions()（impact map §5 的 #19）。这里直接测宿主回报的
    // 这一面：丢掉空 chord、逐和弦取 int，且管理器侧再不出现任何 Qt 快捷键类型。
    void hostActionIdentitiesCarryNativeChordEncoding()
    {
        QObject actionCollection;
        QAction *hostAction = new QAction(&actionCollection);
        hostAction->setObjectName(QStringLiteral("native-chord-probe"));
        const QKeySequence oneChord(QStringLiteral("Ctrl+R"));
        const QKeySequence twoChords(QStringLiteral("Ctrl+K, Ctrl+C"));
        hostAction->setShortcuts({oneChord, QKeySequence(), twoChords});

        MinimalController controller(&actionCollection);
        const PkList<KisHostActionIdentity> identities = controller.hostActions();

        QCOMPARE(identities.size(), 1);
        QCOMPARE(identities.at(0).objectName, PkString("native-chord-probe"));
        QVERIFY(!identities.at(0).carriesToolAction);

        const PkList<std::vector<int>> &shortcuts = identities.at(0).shortcutChords;
        QCOMPARE(shortcuts.size(), 2);
        QCOMPARE(shortcuts.at(0).size(), std::size_t(1));
        QCOMPARE(shortcuts.at(0).at(0), oneChord[0]);
        QCOMPARE(shortcuts.at(1).size(), std::size_t(2));
        QCOMPARE(shortcuts.at(1).at(0), twoChords[0]);
        QCOMPARE(shortcuts.at(1).at(1), twoChords[1]);
    }

    void pathSelectionBulkOperationsCoalesceNativeNotification()
    {
        MinimalShapeController controller;
        MinimalCanvas canvas(&controller);
        KoPathShape shape;
        shape.moveTo(PkPointF(10.0, 20.0));
        shape.lineTo(PkPointF(30.0, 20.0));
        KoPathTool tool(&canvas);
        auto *selection = dynamic_cast<KoPathToolSelection *>(tool.selection());
        QVERIFY(selection);
        selection->setSelectedShapes({&shape});

        PkObject receiver;
        int notifications = 0;
        PkObject::connect(selection, &KoPathToolSelection::selectionChanged,
                          &receiver, [&] { ++notifications; });

        selection->selectAll();

        QCOMPARE(selection->size(), 2);
        QCOMPARE(notifications, 1);

        selection->selectPoints(PkRectF(0.0, 0.0, 100.0, 100.0), true);

        QCOMPARE(selection->size(), 2);
        QCOMPARE(notifications, 2);
    }

    void toolManagerNotificationsUseNativeSignalDelivery()
    {
        KoToolManager manager;
        PkObject receiver;
        PkString status;
        int notifications = 0;
        PkObject::connect(&manager, &KoToolManager::changedStatusText,
                          &receiver, [&](const PkString &value) {
            status = value;
            ++notifications;
        });

        manager.changedStatusText("native-status");

        QCOMPARE(status, PkString("native-status"));
        QCOMPARE(notifications, 1);
    }

    void toolBaseNotificationsUseNativeSignalDelivery()
    {
        MinimalShapeController controller;
        MinimalCanvas canvas(&controller);
        PkOnlyTool tool(&canvas);
        PkObject receiver;
        PkString status;
        bool hasSelection = false;
        int notifications = 0;
        PkObject::connect(&tool, &KoToolBase::statusTextChanged,
                          &receiver, [&](const PkString &value) {
            status = value;
            ++notifications;
        });
        PkObject::connect(&tool, &KoToolBase::selectionChanged,
                          &receiver, [&](bool value) {
            hasSelection = value;
            ++notifications;
        });

        tool.statusTextChanged("native-tool-status");
        tool.selectionChanged(true);

        QCOMPARE(status, PkString("native-tool-status"));
        QVERIFY(hasSelection);
        QCOMPARE(notifications, 2);
    }

    void toolProxyNotificationsUseNativeSignalDelivery()
    {
        MinimalShapeController controller;
        MinimalCanvas canvas(&controller);
        TestToolProxy proxy(&canvas);
        PkOnlyTool tool(&canvas);
        PkObject receiver;
        PkString toolId;
        int notifications = 0;
        PkObject::connect(&proxy, &KoToolProxy::toolChanged,
                          &receiver, [&](const PkString &value) {
            toolId = value;
            ++notifications;
        });

        proxy.setActiveTool(&tool);

        QCOMPARE(toolId, tool.toolId());
        QCOMPARE(notifications, 1);
    }

    void selectedShapesProxyDeliveryHonorsToolLifetime()
    {
        MinimalShapeController controller;
        MinimalCanvas canvas(&controller);
        KoSelectedShapesProxy *proxy = canvas.selectedShapesProxy();
        auto *tool = new PkOnlyTool(&canvas);
        int deliveries = 0;
        tool->watchSelectedShapes([&] { ++deliveries; });

        proxy->selectionChanged();
        QCOMPARE(deliveries, 1);

        delete tool;
        proxy->selectionChanged();
        QCOMPARE(deliveries, 1);
    }

    void canvasUpdateFacadeForwardsDocumentRect()
    {
        MinimalShapeController controller;
        MinimalCanvas canvas(&controller);
        PkOnlyTool tool(&canvas);
        const PkRectF rect(1.0, 2.0, 30.0, 40.0);

        tool.requestUpdate(rect);

        QCOMPARE(canvas.updateCanvasCalls, 1);
        QCOMPARE(canvas.lastUpdatedRect, rect);
    }

    void activeToolConnectionsEndAtSwitchBoundary()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        QObject actionCollection;
        MinimalController controller(&actionCollection);
        controller.setCanvas(&canvas);
        canvas.setCanvasController(&controller);
        KoToolManager manager;
        manager.initializeToolActions();
        for (KoToolAction *action : manager.toolActionList()) {
            action->toolFactory()->createActions(&actionCollection);
        }
        manager.addController(&controller);

        const PkList<KoToolAction *> actions = manager.toolActionList();
        QVERIFY(actions.size() >= 2);
        const PkString firstId = actions.at(0)->id();
        const PkString secondId = actions.at(1)->id();
        QVERIFY(firstId != secondId);

        manager.switchToolRequested(firstId);
        KoToolBase *first = manager.toolById(&canvas, firstId);
        manager.switchToolRequested(secondId);
        KoToolBase *second = manager.toolById(&canvas, secondId);
        QVERIFY(first);
        QVERIFY(second);

        PkObject receiver;
        PkList<PkString> statuses;
        PkObject::connect(&manager, &KoToolManager::changedStatusText,
                          &receiver, [&](const PkString &status) {
            if (!status.isEmpty()) {
                statuses.append(status);
            }
        });

        second->statusTextChanged("second-active");
        QCOMPARE(statuses, PkList<PkString>({"second-active"}));

        first->statusTextChanged("first-inactive");
        QCOMPARE(statuses, PkList<PkString>({"second-active"}));

        manager.switchToolRequested(firstId);
        first->statusTextChanged("first-reactivated");
        QCOMPARE(statuses, PkList<PkString>({"second-active", "first-reactivated"}));

        second->statusTextChanged("second-inactive");
        QCOMPARE(statuses, PkList<PkString>({"second-active", "first-reactivated"}));

        manager.removeCanvasController(&controller);
    }

    void canvasObserverDisconnectCoversQObjectAndPkDelivery()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        auto *proxy = new TestToolProxy(&canvas);
        canvas.setToolProxy(proxy);
        DualCanvasObserver observer;
        int qtDeliveries = 0;
        int pkDeliveries = 0;
        QObject::connect(proxy, &QObject::objectNameChanged, &observer,
                         [&](const QString &) { ++qtDeliveries; });
        PkObject::connect(proxy, &KoToolProxy::toolChanged, &observer,
                          [&](const PkString &) { ++pkDeliveries; });

        proxy->QObject::setObjectName(QStringLiteral("before"));
        proxy->toolChanged("before");
        QCOMPARE(qtDeliveries, 1);
        QCOMPARE(pkDeliveries, 1);

        canvas.disconnectCanvasObserver(&observer);
        proxy->QObject::setObjectName(QStringLiteral("after"));
        proxy->toolChanged("after");
        QCOMPARE(qtDeliveries, 1);
        QCOMPARE(pkDeliveries, 1);

        canvas.setToolProxy(nullptr);
        delete proxy;
    }

    void hostKeyAdapterDispatchesCompletePkPayloadAndAcceptance()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        TestToolProxy proxy(&canvas);
        PkOnlyTool tool(&canvas);
        proxy.priv()->activeTool = &tool;

        PkToolKeyEvent press(Pk::Key_A,
                             Pk::KeyboardModifiers(Pk::ControlModifier | Pk::ShiftModifier),
                             false, true, "A");
        proxy.keyPressEvent(press);

        QCOMPARE(tool.keyPressCalls, 1);
        QCOMPARE(tool.lastKey, Pk::Key_A);
        QCOMPARE(tool.lastModifiers,
                 Pk::KeyboardModifiers(Pk::ControlModifier | Pk::ShiftModifier));
        QVERIFY(!tool.lastAcceptedOnEntry);
        QVERIFY(tool.lastAutoRepeat);
        QCOMPARE(tool.lastText, PkString("A"));
        QVERIFY(press.isAccepted());

        PkToolKeyEvent release(Pk::Key_B, Pk::AltModifier, true, false, "b");
        proxy.keyReleaseEvent(release);

        QCOMPARE(tool.keyReleaseCalls, 1);
        QCOMPARE(tool.lastKey, Pk::Key_B);
        QCOMPARE(tool.lastModifiers, Pk::KeyboardModifiers(Pk::AltModifier));
        QVERIFY(tool.lastAcceptedOnEntry);
        QVERIFY(!tool.lastAutoRepeat);
        QCOMPARE(tool.lastText, PkString("b"));
        QVERIFY(!release.isAccepted());
    }

    void nativeKeyEventPreservesAutoRepeatState()
    {
        PkToolKeyEvent event(Pk::Key_A, Pk::ControlModifier, false, true, "a");
        QCOMPARE(event.key(), Pk::Key_A);
        QCOMPARE(event.modifiers(), Pk::ControlModifier);
        QVERIFY(event.isAutoRepeat());
        QCOMPARE(event.text(), PkString("a"));
        QVERIFY(!event.isAccepted());
    }

    void shortcutOverrideClaimsTextInputOnly()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        TestToolProxy proxy(&canvas);
        PkOnlyTool tool(&canvas);

        const auto keyEvent = [](Pk::Key key, Pk::KeyboardModifiers modifiers) {
            return PkToolKeyEvent(key, modifiers, false);
        };

        // Without a tool there is no text input to claim the key.
        QVERIFY(!proxy.shortcutOverride(keyEvent(Pk::Key_A, Pk::NoModifier)));

        proxy.priv()->activeTool = &tool;

        // Outside text mode no single key is ever claimed.
        tool.setTextMode(false);
        QVERIFY(!proxy.shortcutOverride(keyEvent(Pk::Key_A, Pk::NoModifier)));
        QVERIFY(!proxy.shortcutOverride(keyEvent(Pk::Key_A, Pk::ShiftModifier)));

        // In text mode unmodified and shift-modified keys are text input.
        tool.setTextMode(true);
        QVERIFY(proxy.shortcutOverride(keyEvent(Pk::Key_A, Pk::NoModifier)));
        QVERIFY(proxy.shortcutOverride(keyEvent(Pk::Key_A, Pk::ShiftModifier)));

        // Chorded keys stay shortcuts, so the host must let them through.
        QVERIFY(!proxy.shortcutOverride(keyEvent(Pk::Key_A, Pk::ControlModifier)));
        QVERIFY(!proxy.shortcutOverride(keyEvent(Pk::Key_A, Pk::MetaModifier)));

        // AltGr arrives as Ctrl+Alt and is claimed only under the Windows-only
        // branch of KoToolProxy::shortcutOverride. The native build defines no
        // Q_OS_* macro, so in this build the answer is always false.
        QVERIFY(!proxy.shortcutOverride(keyEvent(Pk::Key_A, Pk::ControlModifier | Pk::AltModifier)));
    }

    void textPropertyNotificationsUseNativeSignalDelivery()
    {
        NativeTextPropertiesInterface interface;
        PkObject receiver;
        int selectionChanges = 0;
        int characterChanges = 0;
        PkObject::connect(&interface, &KoSvgTextPropertiesInterface::textSelectionChanged,
                          &receiver, [&] { ++selectionChanges; });
        PkObject::connect(&interface, &KoSvgTextPropertiesInterface::textCharacterSelectionChanged,
                          &receiver, [&] { ++characterChanges; });

        interface.textSelectionChanged();
        interface.textCharacterSelectionChanged();

        QCOMPARE(selectionChanges, 1);
        QCOMPARE(characterChanges, 1);
    }

    void createPathToolGuiNotificationUsesNativeSignalDelivery()
    {
        MinimalShapeController controller;
        MinimalCanvas canvas(&controller);
        KoCreatePathTool tool(&canvas);
        PkObject receiver;
        bool deliveredValue = false;
        int notifications = 0;
        PkObject::connect(&tool, &KoCreatePathTool::sigUpdateAutoSmoothCurvesGUI,
                          &receiver, [&](bool value) {
            deliveredValue = value;
            ++notifications;
        });

        tool.sigUpdateAutoSmoothCurvesGUI(true);

        QVERIFY(deliveredValue);
        QCOMPARE(notifications, 1);
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
            PkInputEvent move(PkInputEvent::MouseMove,
                              PkPointF(7, 9), PkPointF(7, 9), PkPointF(17, 19),
                              Pk::NoButton, Pk::LeftButton, Pk::ShiftModifier);
            proxy.mouseMoveEvent(move, PkPointF(70, 90));
        }

        const KoPointerEvent *saved = proxy.lastDeliveredPointerEvent();
        QVERIFY(saved);
        QCOMPARE(saved->point, PkPointF(70, 90));
        QCOMPARE(saved->pos(), PkPoint(7, 9));
        QCOMPARE(saved->globalPos(), PkPoint(17, 19));
        QCOMPARE(saved->buttons(), Pk::LeftButton);
        QCOMPARE(saved->modifiers(), Pk::ShiftModifier);
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

        PkInputEvent move(PkInputEvent::MouseMove,
                          PkPointF(25, 35), PkPointF(25, 35), PkPointF(25, 35),
                          Pk::NoButton, Pk::LeftButton, Pk::NoModifier);
        KoPointerEvent pointer(move, PkPointF(25, 35));
        proxy.mousePressEvent(&pointer);
        proxy.mouseMoveEvent(&pointer);
        QVERIFY(proxy.priv()->scrollTimer.isActive());

        // A stalled host must not accumulate one callback per elapsed interval.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
        QCOMPARE(PkThreadCallQueue::pendingCount(), size_t(1));
        proxy.processEvent();
        QCOMPARE(PkThreadCallQueue::pendingCount(), size_t(0));
        QCOMPARE(controller.ensureVisibleCalls, 1);
        QCOMPARE(tool.mouseMoveCalls, 2);
        QCOMPARE(tool.lastMouseMovePoint, PkPointF(25, 35));
        QCOMPARE(tool.lastMouseMoveButtons, Pk::LeftButton);

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
        PkInputEvent press(PkInputEvent::MouseButtonPress,
                           PkPointF(10, 20), PkPointF(10, 20), PkPointF(10, 20),
                           Pk::LeftButton, Pk::LeftButton, Pk::NoModifier);
        KoPointerEvent start(press, PkPointF(10, 20));
        tool.mousePressEvent(&start);
        PkInputEvent move(PkInputEvent::MouseMove,
                          PkPointF(30, 20), PkPointF(30, 20), PkPointF(30, 20),
                          Pk::NoButton, Pk::LeftButton, Pk::NoModifier);
        KoPointerEvent end(move, PkPointF(30, 20));
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

    void hostEventsCrossThePkToolBoundary()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        TestToolProxy proxy(&canvas);
        PkOnlyTool tool(&canvas);
        proxy.priv()->activeTool = &tool;

        PkToolInputMethodEvent input;
        input.commitString = "commit";
        input.preeditString = "preedit";
        input.replacementStart = -2;
        input.replacementLength = 1;
        input.ignore();
        proxy.inputMethodEvent(input);
        QCOMPARE(tool.inputMethodCalls, 1);
        QCOMPARE(tool.lastCommitString, PkString("commit"));
        QCOMPARE(tool.lastPreeditString, PkString("preedit"));
        QCOMPARE(tool.lastReplacementStart, -2);
        QCOMPARE(tool.lastReplacementLength, 1);
        QVERIFY(input.isAccepted());

        PkToolEvent focusIn;
        focusIn.ignore();
        proxy.focusInEvent(focusIn);
        QCOMPARE(tool.focusInCalls, 1);
        QVERIFY(focusIn.isAccepted());

        PkToolEvent focusOut(true);
        proxy.focusOutEvent(focusOut);
        QCOMPARE(tool.focusOutCalls, 1);
        QVERIFY(!focusOut.isAccepted());

        PkToolEvent drag;
        drag.ignore();
        proxy.dragMoveEvent(drag, PkPointF(40, 50));
        QCOMPARE(tool.dragMoveCalls, 1);
        QCOMPARE(tool.lastDragPoint, PkPointF(40, 50));
        QVERIFY(drag.isAccepted());

        PkToolEvent drop;
        drop.ignore();
        proxy.dropEvent(drop, PkPointF(60, 70));
        QCOMPARE(tool.dropCalls, 1);
        QCOMPARE(tool.lastDropPoint, PkPointF(60, 70));
        QVERIFY(drop.isAccepted());
    }
};

QTEST_MAIN(KoToolProxyPkPainterTest)

#include "KoToolProxyPkPainterTest.moc"
