/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006-2011 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KoToolProxy.h"
#include "KoToolProxy_p.h"

#include <PkThreadCallQueue.h>
#include <PkInputEvent.h>

#include <chrono>

#include <kundo2command.h>
#include <KoProperties.h>

#include <FlakeDebug.h>
#include <klocalizedstring.h>

#include "KoToolBase.h"
#include "KoCanvasPlatformHost.h"
#include "KoPointerEvent.h"
#include "KoInputDevice.h"
#include "KoToolManager_p.h"
#include "KoToolSelection.h"
#include "KoCanvasBase.h"
#include "KoCanvasController.h"
#include "KoShapeManager.h"
#include "KoSelection.h"
#include "KoShapeLayer.h"
#include "KoShapeRegistry.h"
#include "KoShapeController.h"
#include "KoViewConverter.h"
#include "KoShapeFactoryBase.h"
#include "kis_assert.h"
#include "kis_global.h"
#include "kis_algebra_2d.h"

namespace {

/**
 * The platform's double-click interval, read from the canvas platform host.
 * A canvas with no host attached (bare or headless) must not crash, and the
 * only definition of the default lives in KoCanvasPlatformHost.h.
 */
int platformDoubleClickInterval(KoCanvasController *controller)
{
    auto *host = dynamic_cast<KoCanvasPlatformHost *>(
        controller ? controller->canvas() : nullptr);
    static const KoCanvasPlatformHost kDefaultPlatformHost;
    return (host ? *host : kDefaultPlatformHost).doubleClickInterval();
}

}


KoToolProxyPrivate::KoToolProxyPrivate(KoToolProxy *p)
    : scrollTimer(PkThreadCallQueue::warmUpCurrentThread())
    , parent(p)
{
}

void KoToolProxyPrivate::timeout() // Auto scroll the canvas
{
    Q_ASSERT(controller);

    const PkPoint originalWidgetPoint = parent->documentToWidget(widgetScrollPointDoc).toPoint();

    const PkPointF margin(10.0, 10.0);

    const PkPointF mouseAreaTopLeftWidget = parent->documentToWidget(widgetScrollPointDoc) - margin;
    const PkPointF mouseAreaBottomRightWidget = mouseAreaTopLeftWidget + 2 * margin;

    const PkPointF mouseAreaTopLeftDoc = parent->widgetToDocument(mouseAreaTopLeftWidget);
    const PkPointF mouseAreaBottomRightDoc = parent->widgetToDocument(mouseAreaBottomRightWidget);
    PkRectF mouseAreaDoc(mouseAreaTopLeftDoc, mouseAreaBottomRightDoc);


    const PkPointF oldPreferredCenter = controller->preferredCenter();

    controller->ensureVisibleDoc(mouseAreaDoc, true);

    const PkPointF newPreferredCenter = controller->preferredCenter();

    // if scrolling has happened, then just return!
    if (oldPreferredCenter == newPreferredCenter) {
        return;
    }

    widgetScrollPointDoc = parent->widgetToDocument(originalWidgetPoint);

    KoPointerEvent ev(originalWidgetPoint,
                      widgetScrollPointDoc,
                      Pk::LeftButton,
                      Pk::LeftButton,
                      Pk::KeyboardModifiers());
    activeTool->mouseMoveEvent(&ev);
}

void KoToolProxyPrivate::checkAutoScroll(const KoPointerEvent &event)
{
    if (controller == 0) return;
    if (!activeTool) return;
    if (!activeTool->wantsAutoScroll()) return;
    if (!event.isAccepted()) return;
    if (!isToolPressed) return;
    if (event.buttons() != Pk::LeftButton) return;


    widgetScrollPointDoc = event.point;

    if (!scrollTimer.isActive()) {
        scrollTimer.start(std::chrono::milliseconds(100), [this] { timeout(); });
    }
}

void KoToolProxyPrivate::selectionChanged(bool newSelection)
{
    if (hasSelection == newSelection)
        return;
    hasSelection = newSelection;
    Q_EMIT parent->selectionChanged(hasSelection);
}

bool KoToolProxyPrivate::isActiveLayerEditable()
{
    if (!activeTool)
        return false;

    KoShapeManager * shapeManager = activeTool->canvas()->shapeManager();
    KoShapeLayer * activeLayer = shapeManager->selection()->activeLayer();
    if (activeLayer && !activeLayer->isShapeEditable())
        return false;
    return true;
}

KoToolProxy::KoToolProxy(KoCanvasBase *canvas, QObject *parent)
    : QObject(parent),
      d(new KoToolProxyPrivate(this))
{
    KoToolManager::instance()->priv()->registerToolProxy(static_cast<KoToolProxyHost *>(this), canvas);

}

KoToolProxy::~KoToolProxy()
{
    delete d;
}

// KoToolProxyHost — the bucket-agnostic surface native code sees. Every upcast
// to a base class happens in this translation unit, against the real (Qt)
// layout; native code never computes an offset of its own.
void KoToolProxy::setCanvasController(KoCanvasController *controller)
{
    d->setCanvasController(controller);
}

PkObject *KoToolProxy::toolProxyObject()
{
    return static_cast<PkObject *>(this);
}

void KoToolProxy::repaintToolDecorations()
{
    repaintDecorations();
}

KoPointerEvent *KoToolProxy::lastDeliveredToolPointerEvent()
{
    return lastDeliveredPointerEvent();
}

// KoCanvasBase host forwards, declared bucket-agnostically in KoCanvasBase.h and
// defined here so the dereference of the proxy uses the Qt layout.
PkObject *KoCanvasBase::toolProxyObject() const
{
    KoToolProxy *proxy = toolProxy();
    if (!proxy) return nullptr;
    return static_cast<KoToolProxyHost *>(proxy)->toolProxyObject();
}

void KoCanvasBase::repaintToolDecorations()
{
    KoToolProxy *proxy = toolProxy();
    if (!proxy) return;
    static_cast<KoToolProxyHost *>(proxy)->repaintToolDecorations();
}

KoPointerEvent *KoCanvasBase::lastDeliveredToolPointerEvent() const
{
    KoToolProxy *proxy = toolProxy();
    if (!proxy) return nullptr;
    return static_cast<KoToolProxyHost *>(proxy)->lastDeliveredToolPointerEvent();
}

void KoToolProxy::paint(PkPainter &painter, const KoViewConverter &converter)
{
    if (d->activeTool) d->activeTool->paint(painter, converter);
}

void KoToolProxy::repaintDecorations()
{
    if (d->activeTool) d->activeTool->repaintDecorations();
}

KoCanvasBase* KoToolProxy::canvas() const
{
    return d->controller->canvas();
}

int KoToolProxy::multiClickCount() const
{
    return d->multiClickCount;
}

void KoToolProxy::countMultiClick(KoPointerEvent *ev, KoPointerInputSource source)
{
    PkPointF globalPoint = ev->globalPos();

    if (d->multiClickSource != source) {
        d->multiClickCount = 0;
    }

    if (d->multiClickGlobalPoint != globalPoint) {
        if (qAbs(globalPoint.x() - d->multiClickGlobalPoint.x()) > 5||
                qAbs(globalPoint.y() - d->multiClickGlobalPoint.y()) > 5) {
            d->multiClickCount = 0;
        }
        d->multiClickGlobalPoint = globalPoint;
    }

    if (d->multiClickCount && d->multiClickTimeStamp.elapsed() < platformDoubleClickInterval(d->controller)) {
        // One more multiclick;
        d->multiClickCount++;
    } else {
        d->multiClickTimeStamp.start();
        d->multiClickCount = 1;
        d->multiClickSource = source;
    }

    if (d->activeTool) {
        switch (d->multiClickCount) {
        case 0:
        case 1:
            d->activeTool->mousePressEvent(ev);
            break;
        case 2:
            d->activeTool->mouseDoubleClickEvent(ev);
            break;
        case 3:
        default:
            d->activeTool->mouseTripleClickEvent(ev);
            break;
        }
    } else {
        d->multiClickCount = 0;
        ev->ignore();
    }

}

void KoToolProxy::tabletEvent(const KoInputDevice &id, const PkTabletEvent &event, const PkPointF &point)
{
    // We get these events exclusively from KisToolProxy - the host has already
    // classified the device, so the identity is taken as given here.
    KoToolManager::instance()->priv()->switchInputDevice(id);

    KoPointerEvent ev(event, point);

    switch (event.type()) {
    case PkInputEvent::TabletPress:
        countMultiClick(&ev, KoPointerInputSource::Tablet);
        break;
    case PkInputEvent::TabletRelease:
        d->scrollTimer.stop();
        if (d->activeTool)
            d->activeTool->mouseReleaseEvent(&ev);
        break;
    case PkInputEvent::TabletMove:
        if (d->activeTool)
            d->activeTool->mouseMoveEvent(&ev);
        d->checkAutoScroll(ev);
    default:
        ; // ignore the rest.
    }

    d->mouseLeaveWorkaround = true;
    d->lastPointerEvent = ev.detachedCopy();
}

void KoToolProxy::mousePressEvent(KoPointerEvent *ev)
{
    d->mouseLeaveWorkaround = false;
    KoInputDevice id;
    KoToolManager::instance()->priv()->switchInputDevice(id);
    d->mouseDownPoint = ev->pos();


    // this tries to make sure another mouse press event doesn't happen
    // before a release event happens
    if (d->isToolPressed) {
        mouseReleaseEvent(ev);
        d->scrollTimer.stop();

        if (d->activeTool) {
            d->activeTool->mouseReleaseEvent(ev);
        }

        d->isToolPressed = false;

        return;
    }

    countMultiClick(ev, KoPointerInputSource::Mouse);

    d->isToolPressed = true;
}

void KoToolProxy::mousePressEvent(const PkInputEvent &event, const PkPointF &point)
{
    KoPointerEvent ev(event, point);
    mousePressEvent(&ev);
    d->lastPointerEvent = ev.detachedCopy();
}

void KoToolProxy::mouseDoubleClickEvent(const PkInputEvent &event, const PkPointF &point)
{
    KoPointerEvent ev(event, point);
    mouseDoubleClickEvent(&ev);
    d->lastPointerEvent = ev.detachedCopy();
}

void KoToolProxy::mouseDoubleClickEvent(KoPointerEvent *event)
{
    // let us handle it as any other mousepress (where we then detect multi clicks
    mousePressEvent(event);
}

void KoToolProxy::mouseMoveEvent(const PkInputEvent &event, const PkPointF &point)
{
    KoPointerEvent ev(event, point);
    mouseMoveEvent(&ev);
    d->lastPointerEvent = ev.detachedCopy();
}

void KoToolProxy::mouseMoveEvent(KoPointerEvent *event)
{
    if (d->mouseLeaveWorkaround) {
        d->mouseLeaveWorkaround = false;
        return;
    }
    KoInputDevice id;
    KoToolManager::instance()->priv()->switchInputDevice(id);
    if (d->activeTool == 0) {
        event->ignore();
        return;
    }

    d->activeTool->mouseMoveEvent(event);

    d->checkAutoScroll(*event);
}

void KoToolProxy::mouseReleaseEvent(const PkInputEvent &event, const PkPointF &point)
{
    KoPointerEvent ev(event, point);
    mouseReleaseEvent(&ev);
    d->lastPointerEvent = ev.detachedCopy();
}

void KoToolProxy::mouseReleaseEvent(KoPointerEvent* event)
{
    d->mouseLeaveWorkaround = false;
    KoInputDevice id;
    KoToolManager::instance()->priv()->switchInputDevice(id);
    d->scrollTimer.stop();

    if (d->activeTool) {
        d->activeTool->mouseReleaseEvent(event);
    } else {
        event->ignore();
    }

    d->isToolPressed = false;
}

void KoToolProxy::keyPressEvent(PkToolKeyEvent &event)
{
    if (d->activeTool) {
        d->activeTool->pkKeyPressEvent(&event);
    } else {
        event.ignore();
    }
}

void KoToolProxy::keyReleaseEvent(PkToolKeyEvent &event)
{
    if (d->activeTool) {
        d->activeTool->pkKeyReleaseEvent(&event);
    } else {
        event.ignore();
    }

    d->isToolPressed = false;
}

void KoToolProxy::explicitUserStrokeEndRequest()
{
    if (d->activeTool) {
        d->activeTool->explicitUserStrokeEndRequest();
    }
}

PkVariant KoToolProxy::inputMethodQuery(Pk::InputMethodQuery query) const
{
    if (d->activeTool)
        return d->activeTool->inputMethodQuery(query);
    return PkVariant();
}

void KoToolProxy::inputMethodEvent(PkToolInputMethodEvent &event)
{
    if (!d->activeTool) return;
    d->activeTool->inputMethodEvent(&event);
}

void KoToolProxy::focusInEvent(PkToolEvent &event)
{
    if (!d->activeTool) return;
    d->activeTool->focusInEvent(&event);
}

void KoToolProxy::focusOutEvent(PkToolEvent &event)
{
    if (!d->activeTool) return;
    d->activeTool->focusOutEvent(&event);
}

QMenu *KoToolProxy::popupActionsMenu()
{
    return d->activeTool ? d->activeTool->popupActionsMenu() : 0;
}

KisPopupWidgetInterface* KoToolProxy::popupWidget()
{
    return d->activeTool ? d->activeTool->popupWidget() : nullptr;
}

void KoToolProxy::setActiveTool(KoToolBase *tool)
{
    if (d->activeTool) {
        PkObject::disconnect(d->activeTool, nullptr, this, nullptr);
    }

    d->activeTool = tool;

    if (tool) {
        PkObject::connect(d->activeTool, &KoToolBase::selectionChanged, this,
                [this](bool hasSelection) { d->selectionChanged(hasSelection); });
        d->selectionChanged(hasSelection());
        Q_EMIT toolChanged(tool->toolId());
    }
}

void KoToolProxy::selectionChanged(bool hasSelection)
{
    activateSignal<bool>(
        this, PkMemberFnKey::from(&KoToolProxy::selectionChanged), hasSelection);
}

void KoToolProxy::toolChanged(const PkString &toolId)
{
    activateSignal<const PkString &>(
        this, PkMemberFnKey::from(&KoToolProxy::toolChanged), toolId);
}

void KoToolProxy::touchEvent(const PkTouchEvent &event, const PkPointF& point)
{
    // only one "touchpoint" events should be here
    KoPointerEvent ev(event, point);

    if (!d->activeTool) return;

    switch (event.touchPointStates())
    {
    case Pk::TouchPointPressed:
        countMultiClick(&ev, KoPointerInputSource::Touch);
        break;
    case Pk::TouchPointMoved:
        d->activeTool->mouseMoveEvent(&ev);
        break;
    case Pk::TouchPointReleased:
        d->activeTool->mouseReleaseEvent(&ev);
        break;
    default: // don't care
        ;
    }

    d->lastPointerEvent = ev.detachedCopy();
}

KoPointerEvent *KoToolProxy::lastDeliveredPointerEvent() const
{
    return d->lastPointerEvent ? &(*d->lastPointerEvent) : nullptr;
}

void KoToolProxyPrivate::setCanvasController(KoCanvasController *c)
{
    controller = c;
}

bool KoToolProxy::hasSelection() const
{
    return d->activeTool ? d->activeTool->hasSelection() : false;
}

void KoToolProxy::cut()
{
    if (d->activeTool && d->isActiveLayerEditable())
        d->activeTool->cut();
}

void KoToolProxy::copy() const
{
    if (d->activeTool)
        d->activeTool->copy();
}

bool KoToolProxy::paste()
{
    bool success = false;

    if (d->activeTool && d->isActiveLayerEditable()) {
        success = d->activeTool->paste();
    }

    return success;
}

bool KoToolProxy::selectAll()
{
    bool success = false;

    if (d->activeTool && d->isActiveLayerEditable()) {
        success = d->activeTool->selectAll();
    }

    return success;
}

void KoToolProxy::deselect()
{
    if (d->activeTool)
        d->activeTool->deselect();
}

void KoToolProxy::dragMoveEvent(PkToolEvent &event, const PkPointF &point)
{
    if (!d->activeTool) return;
    d->activeTool->dragMoveEvent(&event, point);
}

void KoToolProxy::dragLeaveEvent(PkToolEvent &event)
{
    if (!d->activeTool) return;
    d->activeTool->dragLeaveEvent(&event);
}

void KoToolProxy::dropEvent(PkToolEvent &event, const PkPointF &point)
{
    if (!d->activeTool) return;
    d->activeTool->dropEvent(&event, point);
}

void KoToolProxy::deleteSelection()
{
    if (d->activeTool)
        d->activeTool->deleteSelection();
}

void KoToolProxy::processEvent() const
{
    // The host calls this entry for every canvas event. It is the retained
    // input thread's explicit pump for PkTimer and queued tool callbacks.
    PkThreadCallQueue::processPendingCalls();
}

bool KoToolProxy::shortcutOverride(const PkToolKeyEvent &event) const
{
    if (!d->activeTool || !d->activeTool->isInTextMode()) {
        return false;
    }

    if (event.modifiers() == Pk::NoModifier || event.modifiers() == Pk::ShiftModifier) {
        return true;
    }
#ifdef Q_OS_WIN
    // we should disallow AltGr shortcuts if a text box is in focus
    if (event.modifiers() == (Pk::AltModifier | Pk::ControlModifier) &&
        event.key() < Pk::Key_Escape) {
        return true;
    }
#endif
    return false;
}

void KoToolProxy::requestUndoDuringStroke()
{
    if (d->activeTool) {
        d->activeTool->requestUndoDuringStroke();
    }
}

void KoToolProxy::requestRedoDuringStroke()
{
    if (d->activeTool) {
        d->activeTool->requestRedoDuringStroke();
    }
}

void KoToolProxy::requestStrokeCancellation()
{
    if (d->activeTool) {
        d->activeTool->requestStrokeCancellation();
    }
}

void KoToolProxy::requestStrokeEnd()
{
    if (d->activeTool) {
        d->activeTool->requestStrokeEnd();
    }
}

KoToolProxyPrivate *KoToolProxy::priv()
{
    return d;
}

//have to include this because of Q_PRIVATE_SLOT
