/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006, 2010 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2011 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QDebug>

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
#include "KoShapeManager.h"
#include "KoSelectedShapesProxy.h"
#include "KoSelection.h"
#include "KoShape.h"
#include "KoToolSelection.h"
#include "KoCanvasController.h"
#include "KoToolProxy.h"
#include "KoDerivedResourceConverter.h"
#include "KoAbstractCanvasResourceInterface.h"
#include "KoCanvasInputMethodHost.h"

#include <klocalizedstring.h>
#include <PkFlakeBridge.h>
#include <PkFileStream.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <KoColor.h>

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

void KoToolBase::watchSelectedShapesChanged(std::function<void()> callback)
{
    Q_D(KoToolBase);
    if (!d->canvas || !d->canvas->selectedShapesProxy()) {
        return;
    }

    PkObject::connect(d->canvas->selectedShapesProxy(),
                      &KoSelectedShapesProxy::selectionChanged,
                      this,
                      std::move(callback));
}

PkToolPointerEventData KoToolBase::pointerEventData(const KoPointerEvent *event) const
{
    return {event->point,
            event->pressure(),
            event->rotation(),
            event->xTilt(),
            event->yTilt(),
            event->x()};
}

PkRectF KoToolBase::documentRectToView(const KoViewConverter &converter,
                                       const PkRectF &rect) const
{
    return PkRectF(converter.documentToView(rect.topLeft()),
                   converter.documentToView(rect.bottomRight()));
}

PkTransform KoToolBase::documentToViewTransform(const KoViewConverter &converter) const
{
    return converter.documentToView();
}

PkColor KoToolBase::canvasForegroundColor() const
{
    Q_D(const KoToolBase);
    return d->canvas && d->canvas->resourceManager()
        ? d->canvas->resourceManager()->foregroundColor().toQColor()
        : PkColor(Pk::black);
}

void KoToolBase::requestCanvasUpdate(const PkRectF &rect)
{
    Q_D(KoToolBase);
    if (d->canvas) {
        d->canvas->updateCanvas(rect);
    }
}

bool KoToolBase::selectShapeAt(const PkPointF &point)
{
    Q_D(KoToolBase);
    if (!d->canvas || !d->canvas->shapeManager()) {
        return false;
    }

    KoShapeManager *manager = d->canvas->shapeManager();
    KoShape *shape = manager->shapeAt(point);
    if (!shape || !manager->selection()) {
        return false;
    }

    manager->selection()->deselectAll();
    manager->selection()->select(shape);
    return true;
}

bool KoToolBase::addShapeToCanvas(KoShape *shape)
{
    Q_D(KoToolBase);
    if (!shape || !d->canvas || !d->canvas->shapeController()) {
        return false;
    }

    KUndo2Command *command = d->canvas->shapeController()->addShape(shape, nullptr);
    if (!command) {
        return false;
    }

    d->canvas->addCommand(command);
    d->canvas->updateCanvas(shape->boundingRect());
    return true;
}

PkToolSelectedShapes KoToolBase::selectedShapes() const
{
    Q_D(const KoToolBase);
    if (!d->canvas || !d->canvas->shapeManager() ||
        !d->canvas->shapeManager()->selection()) {
        return {};
    }

    KoSelection *selection = d->canvas->shapeManager()->selection();
    return {selection->firstSelectedShape(), selection->count()};
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

    return d->canvas->lastDeliveredToolPointerEvent();
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
    const KoCanvasInputMethodHost *host = dynamic_cast<KoCanvasInputMethodHost *>(d->canvas);
    const PkSize widgetSize = host ? host->toolCanvasWidgetSize() : PkSize();
    if (widgetSize.isEmpty())
        return PkVariant();

    switch (query) {
    case Pk::ImEnabled:
        return isInTextMode();
    case Pk::ImCursorRectangle:
        return PkRect(widgetSize.width() / 2, 0, 1, widgetSize.height());
    case Pk::ImFont:
        // 过渡期：QFont 无法进 PkVariant（输入法字体提示，绘画内核非关键路径）。
        return PkVariant();
    default:
        return PkVariant();
    }
}

void KoToolBase::inputMethodEvent(PkToolInputMethodEvent *event)
{
    if (!event->commitString.isEmpty()) {
        PkToolKeyEvent keyEvent(static_cast<Pk::Key>(-1), Pk::NoModifier, false,
                                false, event->commitString);
        pkKeyPressEvent(&keyEvent);
    }
    event->accept();
}

void KoToolBase::focusInEvent(PkToolEvent *event)
{
    event->ignore();
}

void KoToolBase::focusOutEvent(PkToolEvent *event)
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

bool KoToolBase::useCursor(KisCanvasCursorToken cursor)
{
    Q_D(KoToolBase);
    auto *host = dynamic_cast<KoCanvasCursorHost *>(d->canvas);
    if (!host || !host->toolOwnsCursor(cursor)) return false;

    d->currentCursorToken = cursor;
    host->toolApplyCursor(cursor);
    Q_EMIT cursorTokenChanged(cursor);
    return true;
}

void KoToolBase::useCursor(Pk::CursorShape cursorShape)
{
    Q_D(KoToolBase);
    auto *host = dynamic_cast<KoCanvasCursorHost *>(d->canvas);
    if (!host) return;

    useCursor(host->toolShapeCursorToken(cursorShape));
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

KisCanvasCursorToken KoToolBase::cursorToken() const
{
    Q_D(const KoToolBase);
    return d->currentCursorToken;
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
    return pkMax(doc.x(), doc.y());
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
    // 该属性只有内核知道（isInTextMode），宿主必须被通知去重新查询；宿主能力接口
    // 是这条信息的唯一出口——canvas 未实现该接口时没有接收方，通知照旧发出即可。
    if (auto *host = dynamic_cast<KoCanvasInputMethodHost *>(d->canvas)) {
        host->updateInputMethod(Pk::ImEnabled);
    }
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

void KoToolBase::dragMoveEvent(PkToolEvent *event, const PkPointF &point)
{
    Q_UNUSED(event);
    Q_UNUSED(point);
}

void KoToolBase::dragLeaveEvent(PkToolEvent *event)
{
    Q_UNUSED(event);
}

void KoToolBase::dropEvent(PkToolEvent *event, const PkPointF &point)
{
    Q_UNUSED(event);
    Q_UNUSED(point);
}

bool KoToolBase::hasSelection()
{
    return selectionHasSelection();
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

void KoToolBase::activateTool(const PkString &id)
{
    activateSignal<const PkString &>(
        this, PkMemberFnKey::from(&KoToolBase::activateTool), id);
}

void KoToolBase::cursorTokenChanged(KisCanvasCursorToken cursor)
{
    activateSignal<KisCanvasCursorToken>(
        this, PkMemberFnKey::from(&KoToolBase::cursorTokenChanged), cursor);
}

void KoToolBase::selectionChanged(bool hasSelection)
{
    activateSignal<bool>(
        this, PkMemberFnKey::from(&KoToolBase::selectionChanged), hasSelection);
}

void KoToolBase::statusTextChanged(const PkString &statusText)
{
    activateSignal<const PkString &>(
        this, PkMemberFnKey::from(&KoToolBase::statusTextChanged), statusText);
}

void KoToolBase::textModeChanged(bool inTextMode)
{
    activateSignal<bool>(
        this, PkMemberFnKey::from(&KoToolBase::textModeChanged), inTextMode);
}
