/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006-2011 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KoToolProxy.h"
#include "KoToolProxy_p.h"

#include <QMimeData>
#include <QUrl>
#include <QApplication>
#include <QTouchEvent>
#include <QClipboard>
#include <QEvent>
#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QFocusEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>
#include <QTextFormat>

#include <PkThreadCallQueue.h>

#include <chrono>

#include <kundo2command.h>
#include <KoProperties.h>

#include <FlakeDebug.h>
#include <klocalizedstring.h>

#include "KoToolBase.h"
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

void syncAcceptedState(const PkToolEvent &source, QEvent *target)
{
    source.isAccepted() ? target->accept() : target->ignore();
}

PkToolInputMethodTextFormat toolInputMethodTextFormat(const QTextCharFormat &format)
{
    PkToolInputMethodTextFormat result;
    if (format.hasProperty(QTextFormat::FontUnderline)) {
        result.underline = format.property(QTextFormat::FontUnderline).toBool();
    }
    if (format.hasProperty(QTextFormat::FontOverline)) {
        result.overline = format.property(QTextFormat::FontOverline).toBool();
    }
    if (format.hasProperty(QTextFormat::FontStrikeOut)) {
        result.strikeOut = format.property(QTextFormat::FontStrikeOut).toBool();
    }
    if (format.hasProperty(QTextFormat::TextUnderlineStyle)) {
        const QTextCharFormat::UnderlineStyle style = format.underlineStyle();
        result.underline = style != QTextCharFormat::NoUnderline;
        if (style == QTextCharFormat::DotLine) {
            result.style = PkToolInputMethodLineStyle::Dotted;
        } else if (style == QTextCharFormat::DashUnderline) {
            result.style = PkToolInputMethodLineStyle::Dashed;
        } else if (style == QTextCharFormat::WaveUnderline || style == QTextCharFormat::SpellCheckUnderline) {
            result.style = PkToolInputMethodLineStyle::Wavy;
#ifdef Q_OS_MACOS
            if (style == QTextCharFormat::SpellCheckUnderline) {
                result.style = PkToolInputMethodLineStyle::Dotted;
            }
#endif
        }
    }
    if (format.hasProperty(QTextFormat::BackgroundBrush)) {
        result.thick = format.background().isOpaque();
#ifdef Q_OS_LINUX
        if (result.style == PkToolInputMethodLineStyle::Dashed) {
            result.style = PkToolInputMethodLineStyle::Solid;
        }
#endif
    }
    if (!result.underline && !result.overline && !result.strikeOut) {
        result.underline = true;
    }
    return result;
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
    if (event.buttons() != Qt::LeftButton) return;


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
    KoToolManager::instance()->priv()->registerToolProxy(this, canvas);

}

KoToolProxy::~KoToolProxy()
{
    delete d;
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

    if (d->multiClickCount && d->multiClickTimeStamp.elapsed() < QApplication::doubleClickInterval()) {
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

void KoToolProxy::tabletEvent(QTabletEvent *event, const PkPointF &point)
{
    // We get these events exclusively from KisToolProxy - accept them
    event->accept();

    KoInputDevice id(KoInputDevice::convertDeviceType(event),
                     KoInputDevice::convertPointerType(event), event->uniqueId());
    KoToolManager::instance()->priv()->switchInputDevice(id);

    KoPointerEvent ev(event, point);

    switch (event->type()) {
    case QEvent::TabletPress:
        countMultiClick(&ev, KoPointerInputSource::Tablet);
        break;
    case QEvent::TabletRelease:
        d->scrollTimer.stop();
        if (d->activeTool)
            d->activeTool->mouseReleaseEvent(&ev);
        break;
    case QEvent::TabletMove:
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

void KoToolProxy::mousePressEvent(QMouseEvent *event, const PkPointF &point)
{
    KoPointerEvent ev(event, point);
    mousePressEvent(&ev);
    d->lastPointerEvent = ev.detachedCopy();
}

void KoToolProxy::mouseDoubleClickEvent(QMouseEvent *event, const PkPointF &point)
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

void KoToolProxy::mouseMoveEvent(QMouseEvent *event, const PkPointF &point)
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

void KoToolProxy::mouseReleaseEvent(QMouseEvent *event, const PkPointF &point)
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

void KoToolProxy::keyPressEvent(QKeyEvent *event)
{
    if (d->activeTool) {
        PkToolKeyEvent nativeEvent(
            static_cast<Pk::Key>(event->key()),
            Pk::KeyboardModifiers(PkFlag(static_cast<int>(event->modifiers()))),
            event->isAccepted(), event->isAutoRepeat(), toPkString(event->text()));
        d->activeTool->pkKeyPressEvent(&nativeEvent);
        nativeEvent.isAccepted() ? event->accept() : event->ignore();
    } else {
        event->ignore();
    }
}

void KoToolProxy::keyReleaseEvent(QKeyEvent *event)
{
    if (d->activeTool) {
        PkToolKeyEvent nativeEvent(
            static_cast<Pk::Key>(event->key()),
            Pk::KeyboardModifiers(PkFlag(static_cast<int>(event->modifiers()))),
            event->isAccepted(), event->isAutoRepeat(), toPkString(event->text()));
        d->activeTool->pkKeyReleaseEvent(&nativeEvent);
        nativeEvent.isAccepted() ? event->accept() : event->ignore();
    } else {
        event->ignore();
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

void KoToolProxy::inputMethodEvent(QInputMethodEvent *event)
{
    if (!d->activeTool) return;

    PkToolInputMethodEvent nativeEvent;
    event->isAccepted() ? nativeEvent.accept() : nativeEvent.ignore();
    nativeEvent.commitString = toPkString(event->commitString());
    nativeEvent.preeditString = toPkString(event->preeditString());
    nativeEvent.replacementStart = event->replacementStart();
    nativeEvent.replacementLength = event->replacementLength();
    for (const QInputMethodEvent::Attribute &attribute : event->attributes()) {
        PkToolInputMethodAttribute nativeAttribute;
        nativeAttribute.start = attribute.start;
        nativeAttribute.length = attribute.length;
        if (attribute.type == QInputMethodEvent::Selection) {
            nativeAttribute.type = PkToolInputMethodAttributeType::Selection;
        } else if (attribute.type == QInputMethodEvent::Cursor) {
            nativeAttribute.type = PkToolInputMethodAttributeType::Cursor;
        } else if (attribute.type == QInputMethodEvent::TextFormat) {
            if (attribute.length == 0 || attribute.start < 0 || !attribute.value.isValid()) {
                continue;
            }
            nativeAttribute.type = PkToolInputMethodAttributeType::TextFormat;
            nativeAttribute.format = toolInputMethodTextFormat(
                attribute.value.value<QTextFormat>().toCharFormat());
        } else {
            continue;
        }
        nativeEvent.attributes.append(nativeAttribute);
    }
    d->activeTool->inputMethodEvent(&nativeEvent);
    syncAcceptedState(nativeEvent, event);
}

void KoToolProxy::focusInEvent(QFocusEvent *event)
{
    if (!d->activeTool) return;
    PkToolEvent nativeEvent(event->isAccepted());
    d->activeTool->focusInEvent(&nativeEvent);
    syncAcceptedState(nativeEvent, event);
}

void KoToolProxy::focusOutEvent(QFocusEvent *event)
{
    if (!d->activeTool) return;
    PkToolEvent nativeEvent(event->isAccepted());
    d->activeTool->focusOutEvent(&nativeEvent);
    syncAcceptedState(nativeEvent, event);
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

void KoToolProxy::touchEvent(QTouchEvent* event, const PkPointF& point)
{
    // only one "touchpoint" events should be here
    KoPointerEvent ev(event, point);

    if (!d->activeTool) return;

    switch (event->touchPointStates())
    {
    case Qt::TouchPointPressed:
        countMultiClick(&ev, KoPointerInputSource::Touch);
        break;
    case Qt::TouchPointMoved:
        d->activeTool->mouseMoveEvent(&ev);
        break;
    case Qt::TouchPointReleased:
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

void KoToolProxy::dragMoveEvent(QDragMoveEvent *event, const PkPointF &point)
{
    if (!d->activeTool) return;
    PkToolEvent nativeEvent(event->isAccepted());
    d->activeTool->dragMoveEvent(&nativeEvent, point);
    syncAcceptedState(nativeEvent, event);
}

void KoToolProxy::dragLeaveEvent(QDragLeaveEvent *event)
{
    if (!d->activeTool) return;
    PkToolEvent nativeEvent(event->isAccepted());
    d->activeTool->dragLeaveEvent(&nativeEvent);
    syncAcceptedState(nativeEvent, event);
}

void KoToolProxy::dropEvent(QDropEvent *event, const PkPointF &point)
{
    if (!d->activeTool) return;
    PkToolEvent nativeEvent(event->isAccepted());
    d->activeTool->dropEvent(&nativeEvent, point);
    syncAcceptedState(nativeEvent, event);
}

void KoToolProxy::deleteSelection()
{
    if (d->activeTool)
        d->activeTool->deleteSelection();
}

void KoToolProxy::processEvent(QEvent *e) const
{
    // The host calls this entry for every canvas event. It is the retained
    // input thread's explicit pump for PkTimer and queued tool callbacks.
    PkThreadCallQueue::processPendingCalls();

    if(e->type()==QEvent::ShortcutOverride
            && d->activeTool
            && d->activeTool->isInTextMode()
            && (static_cast<QKeyEvent*>(e)->modifiers()==Qt::NoModifier ||
                static_cast<QKeyEvent*>(e)->modifiers()==Qt::ShiftModifier
#ifdef Q_OS_WIN
            // we should disallow AltGr shortcuts if a text box is in focus
            || (static_cast<QKeyEvent*>(e)->modifiers()==(Qt::AltModifier | Qt::ControlModifier) &&
                static_cast<QKeyEvent*>(e)->key() < Qt::Key_Escape)
#endif
            )) {
        e->accept();
    }
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
