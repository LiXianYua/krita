/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006, 2010 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QDebug>

#include <QAction>

#include "KoToolBase.h"
#include "KoToolBase_p.h"
#include "KoToolFactoryBase.h"
#include "KoCanvasBase.h"
#include "KoPointerEvent.h"
#include "KoDocumentResourceManager.h"
#include "KoCanvasResourceProvider.h"
#include "KoViewConverter.h"
#include "KoShapeController.h"
#include "KoShapeControllerBase.h"
#include "KoToolSelection.h"
#include "KoCanvasController.h"
#include "KoToolProxy.h"
#include "KoDerivedResourceConverter.h"
#include "KoAbstractCanvasResourceInterface.h"

#include <klocalizedstring.h>
#include <QWidget>
#include <PkFileStream.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <QApplication>

#include <QKeyEvent>
#include <QInputMethodEvent>
#include <QFocusEvent>

KoToolBase::KoToolBase(KoCanvasBase *canvas)
    : d_ptr(new KoToolBasePrivate(this, canvas))
{
    Q_D(KoToolBase);
    d->connectSignals();
}

KoToolBase::KoToolBase(KoToolBasePrivate &dd)
    : d_ptr(&dd)
{
    Q_D(KoToolBase);
    d->connectSignals();
}

KoToolBase::~KoToolBase()
{
    delete d_ptr;
}


bool KoToolBase::isActivated() const
{
    Q_D(const KoToolBase);
    return d->isActivated;
}

KoPointerEvent *KoToolBase::lastDeliveredPointerEvent() const
{
    Q_D(const KoToolBase);

    if (!d->canvas) return 0;
    if (!d->canvas->toolProxy()) return 0;

    return d->canvas->toolProxy()->lastDeliveredPointerEvent();
}

void KoToolBase::activate(const PkSet<KoShape *> &shapes)
{
    Q_UNUSED(shapes);

    Q_D(KoToolBase);
    d->isActivated = true;
}

void KoToolBase::deactivate()
{
    Q_D(KoToolBase);
    d->isActivated = false;
}

void KoToolBase::canvasResourceChanged(int key, const PkVariant & res)
{
    Q_UNUSED(key);
    Q_UNUSED(res);
}

void KoToolBase::documentResourceChanged(int key, const PkVariant &res)
{
    Q_UNUSED(key);
    Q_UNUSED(res);
}

bool KoToolBase::wantsAutoScroll() const
{
    return true;
}

void KoToolBase::mouseDoubleClickEvent(KoPointerEvent *event)
{
    event->ignore();
}

void KoToolBase::mouseTripleClickEvent(KoPointerEvent *event)
{
    event->ignore();
}

void KoToolBase::keyPressEvent(QKeyEvent *e)
{
    PkToolKeyEvent event(static_cast<Pk::Key>(e->key()),
                         Pk::KeyboardModifiers(PkFlag(static_cast<int>(e->modifiers()))),
                         e->isAccepted());
    pkKeyPressEvent(&event);
    event.isAccepted() ? e->accept() : e->ignore();
}

void KoToolBase::keyReleaseEvent(QKeyEvent *e)
{
    PkToolKeyEvent event(static_cast<Pk::Key>(e->key()),
                         Pk::KeyboardModifiers(PkFlag(static_cast<int>(e->modifiers()))),
                         e->isAccepted());
    pkKeyReleaseEvent(&event);
    event.isAccepted() ? e->accept() : e->ignore();
}

void KoToolBase::pkKeyPressEvent(PkToolKeyEvent *e)
{
    e->ignore();
}

void KoToolBase::pkKeyReleaseEvent(PkToolKeyEvent *e)
{
    e->ignore();
}


void KoToolBase::explicitUserStrokeEndRequest()
{
}

PkVariant KoToolBase::inputMethodQuery(Pk::InputMethodQuery query) const
{
    Q_D(const KoToolBase);
    if (d->canvas->canvasWidget() == 0)
        return PkVariant();

    switch (query) {
    case Pk::ImEnabled:
        return isInTextMode();
    case Pk::ImCursorRectangle:
        return PkRect(d->canvas->canvasWidget()->width() / 2, 0, 1, d->canvas->canvasWidget()->height());
    case Pk::ImFont:
        // 过渡期：QFont 无法进 PkVariant（输入法字体提示，绘画内核非关键路径）。
        return PkVariant();
    default:
        return PkVariant();
    }
}

void KoToolBase::inputMethodEvent(QInputMethodEvent * event)
{
    if (! event->commitString().isEmpty()) {
        QKeyEvent ke(QEvent::KeyPress, -1, QFlags<Qt::KeyboardModifier>(), event->commitString());
        keyPressEvent(&ke);
    }
    event->accept();
}

void KoToolBase::focusInEvent(QFocusEvent *event)
{
    event->ignore();
}

void KoToolBase::focusOutEvent(QFocusEvent *event)
{
    event->ignore();
}

void KoToolBase::customPressEvent(KoPointerEvent * event)
{
    event->ignore();
}

void KoToolBase::customReleaseEvent(KoPointerEvent * event)
{
    event->ignore();
}

void KoToolBase::customMoveEvent(KoPointerEvent * event)
{
    event->ignore();
}

void KoToolBase::useCursor(const QCursor &cursor)
{
    Q_D(KoToolBase);
    d->currentCursor = cursor;
    Q_EMIT cursorChanged(d->currentCursor);
}

PkList<QPointer<QWidget> > KoToolBase::optionWidgets()
{
    Q_D(KoToolBase);
    if (!d->optionWidgetsCreated) {
        d->optionWidgets = createOptionWidgets();
        d->optionWidgetsCreated = true;
    }
    return d->optionWidgets;
}

QAction *KoToolBase::action(const PkString &name) const
{
    Q_D(const KoToolBase);
    if (d->canvas && d->canvas->canvasController() && d->canvas->canvasController()) {
        QObject *collection = d->canvas->canvasController()->actionCollection();
        return collection ? collection->findChild<QAction *>(toQString(name)) : 0;
    }
    return 0;
}

QWidget * KoToolBase::createOptionWidget()
{
    return 0;
}

PkList<QPointer<QWidget> > KoToolBase::createOptionWidgets()
{
    PkList<QPointer<QWidget> > ow;
    if (QWidget *widget = createOptionWidget()) {
        if (widget->objectName().isEmpty()) {
            widget->setObjectName(toQString(toolId()));
        }
        ow.append(widget);
    }
    return ow;
}

void KoToolBase::setFactory(KoToolFactoryBase *factory)
{
    Q_D(KoToolBase);
    d->factory = factory;
}

KoToolFactoryBase* KoToolBase::factory() const
{
    Q_D(const KoToolBase);
    return d->factory;
}

PkString KoToolBase::toolId() const
{
    Q_D(const KoToolBase);
    return d->factory ? d->factory->id() : PkString();
}

QCursor KoToolBase::cursor() const
{
    Q_D(const KoToolBase);
    return d->currentCursor;
}

void KoToolBase::deleteSelection()
{
}

void KoToolBase::cut()
{
    copy();
    deleteSelection();
}

KoCanvasBase * KoToolBase::canvas() const
{
    Q_D(const KoToolBase);
    return d->canvas;
}

void KoToolBase::setStatusText(const PkString &statusText)
{
    Q_EMIT statusTextChanged(statusText);
}

int KoToolBase::handleRadius() const
{
    Q_D(const KoToolBase);
    if (d->canvas
            && d->canvas->resourceManager()
       )
    {
        return d->canvas->resourceManager()->handleRadius();
    }
    else {
        return 3;
    }
}

qreal KoToolBase::handleDocRadius() const
{
    Q_D(const KoToolBase);
    const KoViewConverter * converter = d->canvas->viewConverter();
    const PkPointF doc = converter->viewToDocument(PkPointF(handleRadius(), handleRadius()));
    return qMax(doc.x(), doc.y());
}

int KoToolBase::decorationThickness() const
{
    Q_D(const KoToolBase);
    if (d->canvas
            && d->canvas->resourceManager()
       )
    {
        return d->canvas->resourceManager()->decorationThickness();
    }
    else {
        return 1;
    }
}

int KoToolBase::grabSensitivity() const
{
    Q_D(const KoToolBase);
    if(d->canvas->shapeController()->resourceManager())
    {
        return d->canvas->shapeController()->resourceManager()->grabSensitivity();
    } else {
        return 3;
    }
}

PkRectF KoToolBase::handleGrabRect(const PkPointF &position) const
{
    Q_D(const KoToolBase);
    const KoViewConverter * converter = d->canvas->viewConverter();
    uint handleSize = 2*grabSensitivity();
    PkRectF r = converter->viewToDocument(PkRectF(0, 0, handleSize, handleSize));
    r.moveCenter(position);
    return r;
}

PkRectF KoToolBase::handlePaintRect(const PkPointF &position) const
{
    Q_D(const KoToolBase);
    const KoViewConverter * converter = d->canvas->viewConverter();
    uint handleSize = 2*handleRadius();
    PkRectF r = converter->viewToDocument(PkRectF(0, 0, handleSize, handleSize));
    r.moveCenter(position);
    return r;
}

void KoToolBase::setTextMode(bool value)
{
    Q_D(KoToolBase);
    d->isInTextMode = value;
    qApp->inputMethod()->update(Qt::ImEnabled);
    Q_EMIT textModeChanged(d->isInTextMode);
}

bool KoToolBase::paste()
{
    return false;
}

bool KoToolBase::selectAll()
{
    return false;
}

void KoToolBase::deselect()
{
}

void KoToolBase::copy() const
{
}

void KoToolBase::dragMoveEvent(QDragMoveEvent *event, const PkPointF &point)
{
    Q_UNUSED(event);
    Q_UNUSED(point);
}

void KoToolBase::dragLeaveEvent(QDragLeaveEvent *event)
{
    Q_UNUSED(event);
}

void KoToolBase::dropEvent(QDropEvent *event, const PkPointF &point)
{
    Q_UNUSED(event);
    Q_UNUSED(point);
}

bool KoToolBase::hasSelection()
{
    KoToolSelection *sel = selection();
    return (sel && sel->hasSelection());
}

KoToolSelection *KoToolBase::selection()
{
    return 0;
}

void KoToolBase::repaintDecorations()
{
    Q_D(KoToolBase);

    PkRectF dirtyRect = d->lastDecorationsRect;
    d->lastDecorationsRect = decorationsRect();
    dirtyRect |= d->lastDecorationsRect;

    if (!dirtyRect.isEmpty()) {
        canvas()->updateCanvas(dirtyRect);
    }
}

PkRectF KoToolBase::decorationsRect() const
{
    return PkRectF();
}

bool KoToolBase::isInTextMode() const
{
    Q_D(const KoToolBase);
    return d->isInTextMode;
}

void KoToolBase::requestUndoDuringStroke()
{
    /**
     * Default implementation just cancels the stroke
     */
    requestStrokeCancellation();
}


void KoToolBase::requestRedoDuringStroke()
{
}

void KoToolBase::requestStrokeCancellation()
{
}

void KoToolBase::requestStrokeEnd()
{
}

bool KoToolBase::maskSyntheticEvents() const
{
    Q_D(const KoToolBase);
    return d->maskSyntheticEvents;
}

void KoToolBase::setMaskSyntheticEvents(bool value)
{
    Q_D(KoToolBase);
    d->maskSyntheticEvents = value;
}

bool KoToolBase::isOpacityPresetMode() const
{
    Q_D(const KoToolBase);
    return d->isOpacityPresetMode;
}

void KoToolBase::setIsOpacityPresetMode(bool value)
{
    Q_D(KoToolBase);
    d->isOpacityPresetMode = value;
}

void KoToolBase::setConverter(KoDerivedResourceConverterSP converter) {
    Q_D(KoToolBase);
    d->toolCanvasResources.converters[converter->key()] = converter;
}

void KoToolBase::setAbstractResource(KoAbstractCanvasResourceInterfaceSP abstractResource) {
    Q_D(KoToolBase);
    d->toolCanvasResources.abstractResources[abstractResource->key()] = abstractResource;
}

PkHash<int, KoAbstractCanvasResourceInterfaceSP> KoToolBase::toolAbstractResources()
{
    Q_D(KoToolBase);
    return d->toolCanvasResources.abstractResources;
}

PkHash<int, KoDerivedResourceConverterSP> KoToolBase::toolConverters()
{
    Q_D(KoToolBase);
    return d->toolCanvasResources.converters;
}

void KoToolBase::updateOptionsWidgetIcons()
{
    Q_D(KoToolBase);
    Q_UNUSED(d);
}
