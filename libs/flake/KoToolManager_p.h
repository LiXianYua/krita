/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KO_TOOL_MANAGER_P
#define KO_TOOL_MANAGER_P

#include <PkList.h>
#include <QObject>
#include <PkString.h>
#include <PkHash.h>
#include <PkObject.h>

#include "KoInputDevice.h"
#include "KoToolManager.h"

class KoToolFactoryBase;
class KoShapeManager;
class KoCanvasBase;
class KoToolBase;
class KoShape;
class KoToolManager;
class KoCanvasController;
class KoShapeLayer;
class CanvasData;
class KoToolProxy;
// Upstream relied on the real `<QObject>` to declare QEvent transitively. The
// compat routing (pk/signal/compat/QObject) does not, and `eventFilter()` only
// ever takes a pointer, so a forward declaration is all the native compile
// interface needs. Same form as `libs/flake/KoToolProxy.h` and
// `libs/flake/KoPointerEvent.h`.
class QEvent;

class Q_DECL_HIDDEN KoToolManager::Private
{
public:
    PkObject controllerConnections;
    Private(KoToolManager *qq);
    ~Private();

    void setup();

    void connectActiveTool();
    void disconnectActiveTool();
    void switchTool(const PkString &id);
    void postSwitchTool();
    void switchCanvasData(CanvasData *cd);

    bool eventFilter(QObject *object, QEvent *event);

    void detachCanvas(KoCanvasController *controller);
    void attachCanvas(KoCanvasController *controller);
    void movedFocus(QWidget *from, QWidget *to);
    void updateCursor(const QCursor &cursor);
    void switchBackRequested();
    void selectionChanged(const PkList<KoShape*> &shapes);
    void currentLayerChanged(const KoShapeLayer *layer);
    void updateToolForProxy();
    void switchToolTemporaryRequested(const PkString &id);
    CanvasData *createCanvasData(KoCanvasController *controller, const KoInputDevice &device);
    KoToolBase* createTool(KoCanvasController *controller, KoToolAction *toolAction);

    /**
     * Request a switch from to the param input device.
     * This will cause the tool for that device to be selected.
     */
    void switchInputDevice(const KoInputDevice &device);

    /**
     * Whenever a new tool proxy class is instantiated, it will use this method to register itself
     * so the toolManager can update it to the latest active tool.
     * @param proxy the proxy to register.
     * @param canvas which canvas the proxy is associated with; whenever a new tool is selected for that canvas,
     *        the proxy gets an update.
     */
    void registerToolProxy(KoToolProxy *proxy, KoCanvasBase *canvas);

    KoToolManager *q;

    PkList<KoToolAction*> toolActionList; // list of all available tools via their actions.

    PkHash<KoCanvasController*, PkList<CanvasData*> > canvasses;
    PkHash<KoCanvasBase*, KoToolProxy*> proxies;

    CanvasData *canvasData; // data about the active canvas.

    KoInputDevice inputDevice;

    bool layerExplicitlyDisabled;
};

#endif
