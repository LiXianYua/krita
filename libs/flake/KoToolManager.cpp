/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2005-2010 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2006-2008 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
// flake
#include <PkFlakeBridge.h>
#include "KoToolManager.h"
#include "KoToolManager_p.h"
#include "KoToolManagerShortcuts_p.h"
#include "KoToolRegistry.h"
#include "KoToolProxy.h"
#include "KoToolProxy_p.h"
#include "KoSelection.h"
#include "KoCanvasController.h"
#include "KoShape.h"
#include "KoShapeLayer.h"
#include "KoShapeRegistry.h"
#include "KoShapeManager.h"
#include "KoSelectedShapesProxy.h"
#include "KoCanvasBase.h"
#include "KoPointerEvent.h"
#include "tools/KoZoomTool.h"
#include "KoToolFactoryBase.h"
#include "kis_assert.h"
#include "KoCanvasResourceProvider.h"

#include <krita_container_utils.h>

// Qt + kde
#include <PkStringList.h>
#include <FlakeDebug.h>

#include <QAction>
#include <stack>

Q_GLOBAL_STATIC(KoToolManager, s_instance)


class CanvasData
{
public:
    CanvasData(KoCanvasController *cc, const KoInputDevice &id)
        : activeTool(0),
          canvas(cc),
          inputDevice(id)
    {
    }

    void activateToolActions()
    {
        toolActions.clear();
        disabledGlobalActions.clear();

        QObject *windowActionCollection = canvas->actionCollection();

        if (!windowActionCollection) {
            qWarning() << "We haven't got an action collection";
            return;
        }

        PkStringList globalActions;

        PkMap<KoToolManagerShortcuts::EncodedShortcut, PkStringList> shortcutMap;

//        qDebug() << "................... activating tool" << activeToolId;

        Q_FOREACH(QAction *action, windowActionCollection->findChildren<QAction *>()) {

            if (action->property("tool_action").isValid()) {
                PkStringList tools = toPkStringList(action->property("tool_action").toStringList());

                if (KoToolRegistry::instance()->keys().contains(toPkString(action->objectName()))) {
                    //qDebug() << "This action needs to be enabled!";
                    action->setEnabled(true);
                    toolActions << toPkString(action->objectName());
                }
                else {
                    if (tools.contains(activeToolId) || action->property("always_enabled").toBool()) {
                        //qDebug() << "\t\tenabling";
                        action->setEnabled(true);
                        toolActions << toPkString(action->objectName());
                    }
                    else {
                        //qDebug() << "\t\tDISabling";
                        action->setDisabled(true);
                    }
                }
            }
            else {
                globalActions << toPkString(action->objectName());
            }

            for (const auto &shortcut : KoToolManagerShortcuts::fromHostAction(*action)) {
                if (shortcutMap.contains(shortcut)) {
                    shortcutMap[shortcut].append(toPkString(action->objectName()));
                }
                else {
                    shortcutMap[shortcut] = PkStringList() << toPkString(action->objectName());
                }
            }
        }

        // Make sure the tool's actions override the global actions that aren't associated with the tool.
        for (const auto &shortcut : shortcutMap.keys()) {
            if (shortcutMap[shortcut].size() > 1) {
                PkStringList actions = shortcutMap[shortcut];
                bool toolActionFound = false;
                Q_FOREACH(const PkString &action, actions) {
                    if (toolActions.contains(action)) {
                        toolActionFound = true;
                    }
                }
                Q_FOREACH(const PkString &action, actions) {
                    if (toolActionFound && globalActions.contains(action)) {
                        //qDebug() << "\tdisabling global action" << action;
                        windowActionCollection->findChild<QAction *>(toQString(action))->setEnabled(false);
                        disabledGlobalActions << action;
                    }
                }
                //qDebug() << shortcutMap[shortcut];
            }
        }

        // The shortcuts might have been configured in the meantime. Shortcut re-read no longer performed (KActionCollection removed).
    }

    void deactivateToolActions()
    {
        if (!activeTool)
            return;

        //qDebug() << "............... deactivating previous tool because activating" << activeToolId;

        QObject *windowActionCollection = canvas->actionCollection();

        Q_FOREACH(const PkString &action, toolActions) {
            //qDebug() << "disabling" << action;
            windowActionCollection->findChild<QAction *>(toQString(action))->setDisabled(true);
        }
        Q_FOREACH(const PkString &action, disabledGlobalActions) {
            //qDebug() << "enabling" << action;
            windowActionCollection->findChild<QAction *>(toQString(action))->setEnabled(true);
        }
    }

    KoToolBase *activeTool;     // active Tool
    PkString activeToolId;   // the id of the active Tool
    PkString activationShapeId; // the shape-type (KoShape::shapeId()) the activeTool 'belongs' to.
    PkHash<PkString, KoToolBase*> allTools; // all the tools that are created for this canvas.
    PkList<KoToolBase*> mostRecentTools; // ordered unique list of tools starting from the most recently used, except for the active tool.
    KoCanvasController *const canvas;
    const KoInputDevice inputDevice;
    PkStringList toolActions;
    PkStringList disabledGlobalActions;
};


// ******** KoToolManager **********
KoToolManager::KoToolManager()
#if defined(QT_CORE_LIB)
    : QObject(),
      PkObject(),
#else
    // Under the compat routing `QObject` is `PkObject`, so that base is named once.
    : PkObject(),
#endif
      d(new Private(this))
{
}

KoToolManager::~KoToolManager()
{
    delete d;
}

PkList<KoToolAction*> KoToolManager::toolActionList() const
{
    return d->toolActionList;
}

void KoToolManager::requestToolActivation(KoCanvasController * controller)
{
    if (d->canvasses.contains(controller)) {
        d->switchTool(d->canvasses.value(controller).first()->activeToolId);
    }
}

KoInputDevice KoToolManager::currentInputDevice() const
{
    return d->inputDevice;
}

void KoToolManager::initializeToolActions()
{
    d->setup();
}

void KoToolManager::addController(KoCanvasController *controller)
{
    Q_ASSERT(controller);
    if (d->canvasses.contains(controller))
        return;
    d->setup();
    d->attachCanvas(controller);
    PkObject::connect(controller->proxyObject.data(), &KoCanvasControllerProxyObject::canvasRemoved, &d->controllerConnections,
            [this](KoCanvasController *canvasController) { d->detachCanvas(canvasController); });
    PkObject::connect(controller->proxyObject.data(), &KoCanvasControllerProxyObject::canvasSet, &d->controllerConnections,
            [this](KoCanvasController *canvasController) { d->attachCanvas(canvasController); });
}

void KoToolManager::removeCanvasController(KoCanvasController *controller)
{
    Q_ASSERT(controller);
    PkObject::disconnect(controller->proxyObject.data(), nullptr, &d->controllerConnections, nullptr);
    d->detachCanvas(controller);
}

void KoToolManager::switchToolRequested(const PkString & id)
{
    d->switchTool(id);
}

void KoToolManager::switchInputDeviceRequested(const KoInputDevice &id)
{
    if (!d->canvasData) return;
    d->switchInputDevice(id);
}

void KoToolManager::switchBackRequested()
{
    if (!d->canvasData) return;
    if (d->canvasData->mostRecentTools.isEmpty()) return;
    d->switchTool(d->canvasData->mostRecentTools.first()->toolId());
}

KoToolBase *KoToolManager::toolById(KoCanvasBase *canvas, const PkString &id) const
{
    Q_ASSERT(canvas);
    Q_FOREACH (KoCanvasController *controller, d->canvasses.keys()) {
        if (controller->canvas() == canvas)
            return d->canvasData->allTools.value(id);
    }
    return 0;
}

KoCanvasController *KoToolManager::activeCanvasController() const
{
    if (! d->canvasData) return 0;
    return d->canvasData->canvas;
}

PkString KoToolManager::preferredToolForSelection(const PkList<KoShape*> &shapes)
{
    PkSet<PkString> shapeTypes;
    Q_FOREACH (KoShape *shape, shapes) {
        shapeTypes.insert(shape->shapeId());
    }
    //KritaUtils::makeContainerUnique(types);

    PkString toolType = KoInteractionTool_ID;
    int prio = INT_MAX;
    Q_FOREACH (KoToolAction *helper, d->toolActionList) {
        if (helper->priority() >= prio)
            continue;

        bool toolWillWork = false;
        foreach (const PkString &type, shapeTypes) {
            if (helper->toolFactory()->activationShapeId().split(',').contains(type)) {
                toolWillWork = true;
                break;
            }
        }

        if (toolWillWork) {
            toolType = helper->id();
            prio = helper->priority();
        }
    }
    return toolType;
}

void KoToolManager::initializeCurrentToolForCanvas()
{
    KIS_ASSERT_RECOVER_RETURN(d->canvasData);

    // make a full reconnect cycle for the currently active tool
    d->disconnectActiveTool();
    d->connectActiveTool();
    d->postSwitchTool();
}

void KoToolManager::themeChanged()
{
    Q_FOREACH (const PkList<CanvasData*> &canvasDataList, d->canvasses) {
        Q_FOREACH (CanvasData *canvasData, canvasDataList) {
            Q_FOREACH (KoToolBase *tool, canvasData->allTools) {
                tool->updateOptionsWidgetIcons();
            }
        }
    }
}

KoToolManager* KoToolManager::instance()
{
    return s_instance;
}

PkString KoToolManager::activeToolId() const
{
    if (!d->canvasData) return PkString();
    return d->canvasData->activeToolId;
}

void KoToolManager::setConverter(KoDerivedResourceConverterSP converter, KoToolBase *tool)
{
    tool->setConverter(converter);
}

void KoToolManager::setAbstractResource(KoAbstractCanvasResourceInterfaceSP abstractResource, KoToolBase *tool)
{
    tool->setAbstractResource(abstractResource);
}


KoToolManager::Private *KoToolManager::priv()
{
    return d;
}

void KoToolManager::aboutToChangeTool(KoCanvasController *canvas)
{
    activateSignal<KoCanvasController *>(
        this, PkMemberFnKey::from(&KoToolManager::aboutToChangeTool), canvas);
}

void KoToolManager::changedTool(KoCanvasController *canvas)
{
    activateSignal<KoCanvasController *>(
        this, PkMemberFnKey::from(&KoToolManager::changedTool), canvas);
}

void KoToolManager::toolCodesSelected(const PkList<PkString> &types)
{
    activateSignal<const PkList<PkString> &>(
        this, PkMemberFnKey::from(&KoToolManager::toolCodesSelected), types);
}

void KoToolManager::currentLayerChanged(const KoCanvasController *canvas,
                                        const KoShapeLayer *layer)
{
    activateSignal<const KoCanvasController *, const KoShapeLayer *>(
        this, PkMemberFnKey::from(&KoToolManager::currentLayerChanged), canvas, layer);
}

void KoToolManager::inputDeviceChanged(const KoInputDevice &device)
{
    activateSignal<const KoInputDevice &>(
        this, PkMemberFnKey::from(&KoToolManager::inputDeviceChanged), device);
}

void KoToolManager::changedCanvas(const KoCanvasBase *canvas)
{
    activateSignal<const KoCanvasBase *>(
        this, PkMemberFnKey::from(&KoToolManager::changedCanvas), canvas);
}

void KoToolManager::changedStatusText(const PkString &statusText)
{
    activateSignal<const PkString &>(
        this, PkMemberFnKey::from(&KoToolManager::changedStatusText), statusText);
}

void KoToolManager::addedTool(KoToolAction *toolAction, KoCanvasController *canvas)
{
    activateSignal<KoToolAction *, KoCanvasController *>(
        this, PkMemberFnKey::from(&KoToolManager::addedTool), toolAction, canvas);
}

void KoToolManager::textModeChanged(bool text)
{
    activateSignal<bool>(
        this, PkMemberFnKey::from(&KoToolManager::textModeChanged), text);
}

void KoToolManager::createOpacityResource(bool isOpacityPresetMode, KoToolBase *tool)
{
    activateSignal<bool, KoToolBase *>(
        this, PkMemberFnKey::from(&KoToolManager::createOpacityResource), isOpacityPresetMode, tool);
}


/**** KoToolManager::Private ****/

KoToolManager::Private::Private(KoToolManager *qq)
    : q(qq),
      canvasData(0),
      layerExplicitlyDisabled(false)
{
}

KoToolManager::Private::~Private()
{
    qDeleteAll(toolActionList);
}

// helper method.
CanvasData *KoToolManager::Private::createCanvasData(KoCanvasController *controller, const KoInputDevice &device)
{
    PkHash<PkString, KoToolBase*> toolsHash;
    Q_FOREACH (KoToolAction *toolAction, toolActionList) {
        KoToolBase* tool = createTool(controller, toolAction);
        if (tool) { // only if a real tool was created
            toolsHash.insert(tool->toolId(), tool);
            q->createOpacityResource(tool->isOpacityPresetMode(), tool);
        }
    }

    CanvasData *cd = new CanvasData(controller, device);
    cd->allTools = toolsHash;
    return cd;
}

KoToolBase *KoToolManager::Private::createTool(KoCanvasController *controller, KoToolAction *toolAction)
{
    PkHash<PkString, KoToolBase*> origHash;

    if (canvasses.contains(controller)) {
        origHash = canvasses.value(controller).first()->allTools;
    }

    if (origHash.contains(toolAction->id())) {
        return origHash.value(toolAction->id());
    }

    debugFlake << "Creating tool" << toolAction->id() << ". Activated on:" << toolAction->visibilityCode() << ", prio:" << toolAction->priority();

    KoToolBase *tool = toolAction->toolFactory()->createTool(controller->canvas());
    if (tool) {
        tool->setFactory(toolAction->toolFactory());
    }

    KoZoomTool *zoomTool = dynamic_cast<KoZoomTool*>(tool);
    if (zoomTool) {
        zoomTool->setCanvasController(controller);
    }

    return tool;
}

void KoToolManager::Private::setup()
{
    if (toolActionList.size() > 0)
        return;

    KoShapeRegistry::instance();
    KoToolRegistry *registry = KoToolRegistry::instance();
    Q_FOREACH (const PkString & id, registry->keys()) {
        toolActionList.append(new KoToolAction(registry->value(id)));
    }
}

void KoToolManager::Private::connectActiveTool()
{
    if (canvasData->activeTool) {
        PkObject::connect(canvasData->activeTool, &KoToolBase::cursorChanged, q,
                [this](const QCursor &cursor) { this->updateCursor(cursor); });
        PkObject::connect(canvasData->activeTool, &KoToolBase::activateTool, q,
                [this](const PkString &id) { q->switchToolRequested(id); });
        PkObject::connect(canvasData->activeTool, &KoToolBase::statusTextChanged, q,
                [this](const PkString &statusText) { q->changedStatusText(statusText); });
        PkObject::connect(canvasData->activeTool, &KoToolBase::textModeChanged, q,
                [this](bool inTextMode) { q->textModeChanged(inTextMode); });

        {
            KoCanvasResourceProvider *resourceManager = canvasData->canvas->canvas()->resourceManager();

            const PkHash<int, KoAbstractCanvasResourceInterfaceSP> abstractResources =
                canvasData->activeTool->toolAbstractResources();
            const PkHash<int, KoDerivedResourceConverterSP> converters = canvasData->activeTool->toolConverters();
            for (KoAbstractCanvasResourceInterfaceSP abstractResource : abstractResources) {
                resourceManager->setAbstractResource(abstractResource);
            }
            for (KoDerivedResourceConverterSP converter : converters) {
                resourceManager->addDerivedResourceConverter(converter);
            }
        }
    }

    // we expect the tool to Q_EMIT a cursor on activation.
    updateCursor(QCursor(Pk::BlankCursor));
}



void KoToolManager::Private::disconnectActiveTool()
{
    if (canvasData->activeTool) {
        {
            KoCanvasResourceProvider *resourceManager = canvasData->canvas->canvas()->resourceManager();

            const PkList<int> abstractKeys = canvasData->activeTool->toolAbstractResources().keys();
            const PkList<int> derivedKeys = canvasData->activeTool->toolConverters().keys();
            for (int key : abstractKeys) {
                if (resourceManager->hasAbstractResource(key))
                    resourceManager->removeAbstractResource(key);
            }
            for (int key : derivedKeys) {
                if (resourceManager->hasDerivedResourceConverter(key))
                    resourceManager->removeDerivedResourceConverter(key);
            }
        }

        canvasData->deactivateToolActions();
        // repaint the decorations before we deactivate the tool as it might deleted
        // data needed for the repaint
        q->aboutToChangeTool(canvasData->canvas);
        canvasData->activeTool->deactivate();
        PkObject::disconnect(canvasData->activeTool, nullptr, q, nullptr);
    }

    // Q_EMIT a empty status text to clear status text from last active tool
    q->changedStatusText(PkString());
}

void KoToolManager::Private::switchTool(const PkString &id)
{
    if (!canvasData) return;

    canvasData->activeToolId = id;
    KoToolBase *tool = canvasData->allTools.value(id);
    if (! tool) {
        return;
    }

    canvasData->activationShapeId = tool->factory()->activationShapeId();

    if (canvasData->activeTool == tool && tool->toolId() != KoInteractionTool_ID)
        return;

    disconnectActiveTool();

    if (canvasData->activeTool) {
        canvasData->mostRecentTools.prepend(canvasData->activeTool);
    }
    canvasData->activeTool = tool;
    canvasData->mostRecentTools.removeOne(tool);

    connectActiveTool();
    postSwitchTool();
}

void KoToolManager::Private::postSwitchTool()
{
#ifndef NDEBUG
    int canvasCount = 1;
    Q_FOREACH (PkList<CanvasData*> list, canvasses) {
        bool first = true;
        Q_FOREACH (CanvasData *data, list) {
            if (first) {
                debugFlake << "Canvas" << canvasCount++;
            }
            debugFlake << "  +- Tool:" << data->activeToolId  << (data == canvasData ? " *" : "");
            first = false;
        }
    }
#endif
    Q_ASSERT(canvasData);
    if (!canvasData) return;

    PkSet<KoShape*> shapesToOperateOn;
    if (canvasData->activeTool
            && canvasData->activeTool->canvas()
            && canvasData->activeTool->canvas()->shapeManager()) {
        KoSelection *selection = canvasData->activeTool->canvas()->shapeManager()->selection();
        Q_ASSERT(selection);
        PkList<KoShape *> shapesDelegatesList = selection->selectedEditableShapesAndDelegates();
        if (!shapesDelegatesList.isEmpty()) {
            shapesToOperateOn = PkSet<KoShape*>();
            for (KoShape *s : shapesDelegatesList) shapesToOperateOn.insert(s);
        }
    }

    if (canvasData->canvas->canvas()) {
        // Caller of postSwitchTool expect this to be called to update the selected tool
        updateToolForProxy();

        // Activate the actions for the currently active tool
        //
        // We should do that **before** calling tool->activate(),
        // because the tool may have its own logic on activation
        // of the actions.
        canvasData->activateToolActions();

        canvasData->activeTool->activate(shapesToOperateOn);
    } else {

        // Activate the actions for the currently active tool
        //
        // We should do that **before** calling tool->activate(),
        // because the tool may have its own logic on activation
        // of the actions.
        canvasData->activateToolActions();

        canvasData->activeTool->activate(shapesToOperateOn);
    }

    q->changedTool(canvasData->canvas);
}


void KoToolManager::Private::switchCanvasData(CanvasData *cd)
{
    Q_ASSERT(cd);

    KoCanvasBase *oldCanvas = 0;
    KoInputDevice oldInputDevice;

    if (canvasData) {
        oldCanvas = canvasData->canvas->canvas();
        oldInputDevice = canvasData->inputDevice;

        if (canvasData->activeTool) {
            disconnectActiveTool();
        }

        KoToolProxy *proxy = proxies.value(oldCanvas);
        Q_ASSERT(proxy);
        proxy->setActiveTool(0);
    }

    canvasData = cd;
    inputDevice = canvasData->inputDevice;

    if (canvasData->activeTool) {
        connectActiveTool();
        postSwitchTool();
    }

    if (oldInputDevice != canvasData->inputDevice) {
        q->inputDeviceChanged(canvasData->inputDevice);
    }

    if (oldCanvas != canvasData->canvas->canvas()) {
        q->changedCanvas(canvasData->canvas->canvas());
    }
}

void KoToolManager::Private::detachCanvas(KoCanvasController *controller)
{
    Q_ASSERT(controller);
    // check if we are removing the active canvas controller
    if (canvasData && canvasData->canvas == controller) {
        KoCanvasController *newCanvas = 0;
        // try to find another canvas controller beside the one we are removing
        Q_FOREACH (KoCanvasController* canvas, canvasses.keys()) {
            if (canvas != controller) {
                // yay found one
                newCanvas = canvas;
                break;
            }
        }
        if (newCanvas) {
            switchCanvasData(canvasses.value(newCanvas).first());
        } else {
            disconnectActiveTool();
            // as a last resort just set a blank one
            canvasData = 0;
        }
    }

    KoToolProxy *proxy = proxies.value(controller->canvas());
    if (proxy)
        proxy->setActiveTool(0);

    PkList<KoToolBase *> tools;
    Q_FOREACH (CanvasData *canvasData, canvasses.value(controller)) {
        Q_FOREACH (KoToolBase *tool, canvasData->allTools) {
            if (! tools.contains(tool)) {
                tools.append(tool);
            }
        }
        delete canvasData;
    }
    Q_FOREACH (KoToolBase *tool, tools) {
        delete tool;
    }
    canvasses.remove(controller);
    q->changedCanvas(canvasData ? canvasData->canvas->canvas() : 0);
}

void KoToolManager::Private::attachCanvas(KoCanvasController *controller)
{
    Q_ASSERT(controller);
    CanvasData *cd = createCanvasData(controller, KoInputDevice::mouse());

    // switch to new canvas as the active one.
    switchCanvasData(cd);

    inputDevice = cd->inputDevice;
    PkList<CanvasData*> canvasses_;
    canvasses_.append(cd);
    canvasses[controller] = canvasses_;

    KoToolProxy *tp = proxies[controller->canvas()];
    if (tp)
        tp->priv()->setCanvasController(controller);

    if (cd->activeTool == 0) {
        // no active tool, so we activate the highest priority main tool
        int highestPriority = INT_MAX;
        KoToolAction * helper = 0;
        Q_FOREACH (KoToolAction * th, toolActionList) {
            if (th->section() == ToolBoxSection::Main) {
                if (th->priority() < highestPriority) {
                    highestPriority = qMin(highestPriority, th->priority());
                    helper = th;
                }
            }
        }
        if (helper)
            switchTool(helper->id());
    }

    KoShapeManager *shapeManager = controller->canvas()->shapeManager();
    PkObject::connect(shapeManager, &KoShapeManager::selectionChanged, q,
            [this, shapeManager] { this->selectionChanged(shapeManager->selection()->selectedShapes()); });
    PkObject::connect(controller->canvas()->selectedShapesProxy(), &KoSelectedShapesProxy::currentLayerChanged, q,
            [this](const KoShapeLayer *layer) { this->currentLayerChanged(layer); });

    q->changedCanvas(canvasData ? canvasData->canvas->canvas() : 0);
}

void KoToolManager::Private::updateCursor(const QCursor &cursor)
{
    Q_ASSERT(canvasData);
    Q_ASSERT(canvasData->canvas);
    Q_ASSERT(canvasData->canvas->canvas());
    canvasData->canvas->canvas()->setCursor(cursor);
}

void KoToolManager::Private::selectionChanged(const PkList<KoShape*> &shapes)
{
    PkList<PkString> types;
    Q_FOREACH (KoShape *shape, shapes) {
        PkSet<KoShape*> delegates = shape->toolDelegates();
        if (delegates.isEmpty()) { // no delegates, just the orig shape
            delegates.insert(shape);
        }

        foreach (KoShape *shape2, delegates) {
            Q_ASSERT(shape2);
            if (! types.contains(shape2->shapeId())) {
                types.append(shape2->shapeId());
            }
        }
    }

    // check if there is still a shape selected the active tool can work on
    // there needs to be at least one shape for a tool without an activationShapeId
    // to work
    // if not change the current tool to the default tool

    const PkStringList activationShapeIds = canvasData->activationShapeId.split(',');

    if (!(canvasData->activationShapeId.isNull() && shapes.size() > 0)
            && !activationShapeIds.contains("flake/always")
            && !activationShapeIds.contains("flake/edit")) {

        bool currentToolWorks = false;
        foreach (const PkString &type, types) {
            if (activationShapeIds.contains(type)) {
                currentToolWorks = true;
                break;
            }
        }
        if (!currentToolWorks) {
            switchTool(KoInteractionTool_ID);
        }
    }

    q->toolCodesSelected(types);
}

void KoToolManager::Private::currentLayerChanged(const KoShapeLayer *layer)
{
    q->currentLayerChanged(canvasData->canvas, layer);
    layerExplicitlyDisabled = layer && !layer->isShapeEditable();
    updateToolForProxy();

    debugFlake << "Layer changed to" << layer << "explicitly disabled:" << layerExplicitlyDisabled;
}

void KoToolManager::Private::updateToolForProxy()
{
    KoToolProxy *proxy = proxies.value(canvasData->canvas->canvas());
    if(!proxy) return;

    bool canUseTool = !layerExplicitlyDisabled || canvasData->activationShapeId.endsWith(toPkString(QLatin1String("/always")));
    proxy->setActiveTool(canUseTool ? canvasData->activeTool : 0);
}

void KoToolManager::Private::switchInputDevice(const KoInputDevice &device)
{
    Q_ASSERT(canvasData);
    if (!canvasData) return;
    if (inputDevice == device) return;
    if (inputDevice.isMouse() && device.isMouse()) return;
    if (device.isMouse() && !inputDevice.isMouse()) {
        // we never switch back to mouse from a tablet input device, so the user can use the
        // mouse to edit the settings for a tool activated by a tablet. See bugs
        // https://bugs.kde.org/show_bug.cgi?id=283130 and https://bugs.kde.org/show_bug.cgi?id=285501.
        // We do continue to switch between tablet devices, thought.
        return;
    }

    PkList<CanvasData*> items = canvasses[canvasData->canvas];

    // search for a canvasdata object for the current input device
    Q_FOREACH (CanvasData *cd, items) {
        if (cd->inputDevice == device) {
            switchCanvasData(cd);

            if (!canvasData->activeTool) {
                switchTool(KoInteractionTool_ID);
            }

            return;
        }
    }

    // still here?  That means we need to create a new CanvasData instance with the current InputDevice.
    CanvasData *cd = createCanvasData(canvasData->canvas, device);
    // switch to new canvas as the active one.
    PkString oldTool = canvasData->activeToolId;

    items.append(cd);
    canvasses[cd->canvas] = items;

    switchCanvasData(cd);

    switchTool(oldTool);
}

void KoToolManager::Private::registerToolProxy(KoToolProxy *proxy, KoCanvasBase *canvas)
{
    proxies.insert(canvas, proxy);
    Q_FOREACH (KoCanvasController *controller, canvasses.keys()) {
        if (controller->canvas() == canvas) {
            proxy->priv()->setCanvasController(controller);
            break;
        }
    }
}
