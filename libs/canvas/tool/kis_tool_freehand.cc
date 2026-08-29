/*
 *  kis_tool_freehand.cc - part of Krita
 *
 *  SPDX-FileCopyrightText: 2003-2007 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2004 Bart Coppens <kde@bartcoppens.be>
 *  SPDX-FileCopyrightText: 2007, 2008, 2010 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2009 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_freehand.h"
#include <QPainter>
#include <QRect>
#include <QThreadPool>

#include <Eigen/Core>

#include <KoPointerEvent.h>
#include <KoViewConverter.h>
#include <KoCanvasBase.h>
#include <KoCanvasController.h>

// Krita/image
#include <kis_layer.h>
#include <kis_paint_layer.h>
#include <kis_painter.h>
#include <brushengine/kis_paintop.h>
#include <kis_selection.h>
#include <brushengine/kis_paintop_preset.h>
#include <brushengine/KisOptimizedBrushOutline.h>


#include "kis_config_notifier.h"
#include "kis_image_config.h"
#include <KisCanvasToolServices.h>
#include "kis_painting_information_builder.h"
#include "kis_tool_freehand_helper.h"
#include "strokes/freehand_stroke.h"

using namespace std::placeholders; // For _1 placeholder


KisToolFreehand::KisToolFreehand(KoCanvasBase * canvas, const QCursor & cursor,
                                 const KUndo2MagicString &transactionText, bool useSavedSmoothing)
    : KisToolPaint(canvas, cursor),
      m_brushResizeCompressor(200, std::bind(&KisToolFreehand::slotDoResizeBrush, this, _1))
{

    setSupportOutline(true);
    updateMaskSyntheticEventsFromTouch();
    connect(KisConfigNotifier::instance(), SIGNAL(touchPaintingChanged()),
            SLOT(updateMaskSyntheticEventsFromTouch()));

    m_infoBuilder = new KisToolFreehandPaintingInformationBuilder(this);
    m_helper = new KisToolFreehandHelper(m_infoBuilder, canvas->resourceManager(), transactionText,
                                         new KisSmoothingOptions(useSavedSmoothing));

    connect(m_helper, SIGNAL(requestExplicitUpdateOutline()), SLOT(explicitUpdateOutline()));

    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas);
    KIS_ASSERT(services);
    connect(services->toolSignals(), &KisCanvasToolSignals::brushOutlineChanged,
            this, &KisToolFreehand::explicitUpdateOutline);
    connect(services->toolSignals(), &KisCanvasToolSignals::effectiveCompositeOpChanged,
            this, &KisToolFreehand::explicitUpdateOutline);
    connect(services->toolSignals(), &KisCanvasToolSignals::effectiveCompositeOpChanged,
            this, &KisToolFreehand::resetCursorStyle);
    connect(services->toolSignals(), &KisCanvasToolSignals::paintOpPresetChanged,
            this, &KisToolFreehand::explicitUpdateOutline);
    connect(services->toolSignals(), &KisCanvasToolSignals::paintOpPresetChanged,
            this, &KisToolFreehand::resetCursorStyle);
}

KisToolFreehand::~KisToolFreehand()
{
    delete m_helper;
    delete m_infoBuilder;
}

void KisToolFreehand::mouseMoveEvent(KoPointerEvent *event)
{
    KisToolPaint::mouseMoveEvent(event);
    m_helper->cursorMoved(convertToPixelCoord(event));
}

KisSmoothingOptionsSP KisToolFreehand::smoothingOptions() const
{
    return m_helper->smoothingOptions();
}

void KisToolFreehand::resetCursorStyle()
{
    KisImageConfig cfg(true);
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    KIS_ASSERT(services);

    bool useSeparateEraserCursor = cfg.separateEraserCursor() && isEraser();

    switch (useSeparateEraserCursor ? cfg.eraserCursorStyle() : cfg.newCursorStyle()) {
    case CURSOR_STYLE_NO_CURSOR:
        useCursor(services->toolCursor(CURSOR_STYLE_NO_CURSOR));
        break;
    case CURSOR_STYLE_POINTER:
        useCursor(services->toolCursor(CURSOR_STYLE_POINTER));
        break;
    case CURSOR_STYLE_SMALL_ROUND:
        useCursor(services->toolCursor(CURSOR_STYLE_SMALL_ROUND));
        break;
    case CURSOR_STYLE_CROSSHAIR:
        useCursor(services->toolCursor(CURSOR_STYLE_CROSSHAIR));
        break;
    case CURSOR_STYLE_TRIANGLE_RIGHTHANDED:
        useCursor(services->toolCursor(CURSOR_STYLE_TRIANGLE_RIGHTHANDED));
        break;
    case CURSOR_STYLE_TRIANGLE_LEFTHANDED:
        useCursor(services->toolCursor(CURSOR_STYLE_TRIANGLE_LEFTHANDED));
        break;
    case CURSOR_STYLE_BLACK_PIXEL:
        useCursor(services->toolCursor(CURSOR_STYLE_BLACK_PIXEL));
        break;
    case CURSOR_STYLE_WHITE_PIXEL:
        useCursor(services->toolCursor(CURSOR_STYLE_WHITE_PIXEL));
        break;
    case CURSOR_STYLE_ERASER:
        useCursor(services->toolCursor(CURSOR_STYLE_ERASER));
        break;
    case CURSOR_STYLE_TOOLICON:
    default:
        KisToolPaint::resetCursorStyle();
        break;
    }
}

KisPaintingInformationBuilder* KisToolFreehand::paintingInformationBuilder() const
{
    return m_infoBuilder;
}

void KisToolFreehand::resetHelper(KisToolFreehandHelper *helper)
{
    delete m_helper;
    m_helper = helper;
}

bool KisToolFreehand::supportsPaintingAssistants() const
{
    return true;
}

int KisToolFreehand::flags() const
{
    return KisTool::FLAG_USES_CUSTOM_COMPOSITEOP|KisTool::FLAG_USES_CUSTOM_PRESET
           |KisTool::FLAG_USES_CUSTOM_SIZE;
}

void KisToolFreehand::activate(const QSet<KoShape*> &shapes)
{
    KisToolPaint::activate(shapes);
}

void KisToolFreehand::deactivate()
{
    if (mode() == PAINT_MODE) {
        endStroke();
        setMode(KisTool::HOVER_MODE);
    }
    KisToolPaint::deactivate();
}

void KisToolFreehand::initStroke(KoPointerEvent *event)
{
    m_helper->initPaint(event,
                        convertToPixelCoord(event),
                        image(),
                        currentNode(),
                        image().data(),
                        0,
                        0,
                        toolId() == "KritaShape/KisToolBrush");
}

void KisToolFreehand::doStroke(KoPointerEvent *event)
{
    m_helper->paintEvent(event);
}

void KisToolFreehand::endStroke()
{
    m_helper->endPaint();
    bool paintOpIgnoredEvent = currentPaintOpPreset()->settings()->mouseReleaseEvent();
    Q_UNUSED(paintOpIgnoredEvent);
}

bool KisToolFreehand::primaryActionSupportsHiResEvents() const
{
    return true;
}

void KisToolFreehand::beginPrimaryAction(KoPointerEvent *event)
{
    // FIXME: workaround for the Duplicate Op
    trySampleByPaintOp(event, SampleFgImage);

    requestUpdateOutline(event->point, event);

    NodePaintAbility paintability = nodePaintAbility();
    // XXX: move this to KisTool and make it work properly for clone layers: for clone layers, the shape paint tools don't work either
    if (!nodeEditable() || paintability != PAINT) {
        if (paintability == KisToolPaint::VECTOR || paintability == KisToolPaint::CLONE){
            dynamic_cast<KisCanvasToolServices *>(canvas())->toolShowLockedLayerMessage(false);
        }
        else if (paintability == MYPAINTBRUSH_UNPAINTABLE) {
            dynamic_cast<KisCanvasToolServices *>(canvas())->toolShowLockedLayerMessage(true);
        }
        event->ignore();

        return;
    }

    KIS_SAFE_ASSERT_RECOVER_RETURN(!m_helper->isRunning());

    setMode(KisTool::PAINT_MODE);

    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    if (services) {
        services->toolSetControlsEnabled(false);
    }

    initStroke(event);
}

void KisToolFreehand::continuePrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    requestUpdateOutline(event->point, event);

    /**
     * Actual painting
     */
    doStroke(event);
}

void KisToolFreehand::endPrimaryAction(KoPointerEvent *event)
{
    Q_UNUSED(event);
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    endStroke();

    if (m_assistant) {
        dynamic_cast<KisCanvasToolServices *>(canvas())->toolEndAssistantStroke();
    }

    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    if (services) {
        services->toolSetControlsEnabled(true);
    }

    setMode(KisTool::HOVER_MODE);
}

bool KisToolFreehand::trySampleByPaintOp(KoPointerEvent *event, AlternateAction action)
{
    if (action != SampleFgNode && action != SampleFgImage) return false;

    /**
     * FIXME: we need some better way to implement modifiers
     * for a paintop level. This method is used in DuplicateOp only!
     */
    QPointF pos = adjustPosition(event->point, event->point);
    qreal perspective = calculatePerspective(pos);
    if (!currentPaintOpPreset()) {
        return false;
    }
    KisPaintInformation info(convertToPixelCoord(event->point),
                             m_infoBuilder->pressureToCurve(event->pressure()),
                             event->xTilt(), event->yTilt(),
                             event->rotation(),
                             event->tangentialPressure(),
                             perspective, 0, 0);
    info.setRandomSource(new KisRandomSource());
    info.setPerStrokeRandomSource(new KisPerStrokeRandomSource());

    bool paintOpIgnoredEvent = currentPaintOpPreset()->settings()->mousePressEvent(info,
                                                                                   event->modifiers(),
                                                                                   currentNode());
    // DuplicateOP during the sampling of new source point (origin)
    // is the only paintop that returns "false" here
    return !paintOpIgnoredEvent;
}

void KisToolFreehand::activateAlternateAction(AlternateAction action)
{
    if (action != ChangeSize && action != ChangeSizeSnap) {
        KisToolPaint::activateAlternateAction(action);
        return;
    }

    useCursor(dynamic_cast<KisCanvasToolServices *>(canvas())->toolCursor(CURSOR_STYLE_NO_CURSOR));
    setOutlineVisible(true);
}

void KisToolFreehand::deactivateAlternateAction(AlternateAction action)
{
    if (action != ChangeSize && action != ChangeSizeSnap) {
        KisToolPaint::deactivateAlternateAction(action);
        return;
    }

    resetCursorStyle();
    setOutlineVisible(false);
}

void KisToolFreehand::beginAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (trySampleByPaintOp(event, action)) {
        m_paintopBasedSamplingInAction = true;
        return;
    }

    if (action != ChangeSize && action != ChangeSizeSnap) {
        KisToolPaint::beginAlternateAction(event, action);
        return;
    }

    setMode(GESTURE_MODE);
    m_initialGestureDocPoint = event->point;
    m_initialGestureGlobalPoint = event->globalPos();

    m_lastDocumentPoint = event->point;
    m_lastPaintOpSize = currentPaintOpPreset()->settings()->paintOpSize();

    m_beginAlternateActionEvent = event->deepCopyEvent();
    requestUpdateOutline(m_initialGestureDocPoint, &m_beginAlternateActionEvent->event);
}

void KisToolFreehand::continueAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (trySampleByPaintOp(event, action) || m_paintopBasedSamplingInAction) return;

    if (action != ChangeSize && action != ChangeSizeSnap) {
        KisToolPaint::continueAlternateAction(event, action);
        return;
    }

    QPointF lastWidgetPosition = convertDocumentToWidget(m_lastDocumentPoint);
    QPointF actualWidgetPosition = convertDocumentToWidget(event->point);

    QPointF offset = actualWidgetPosition - lastWidgetPosition;

    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices *>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(services);
    const QRect screenRect = services->toolAvailableVirtualScreenGeometry();
    const qreal scaleX = services->toolImageScaleX();

    const qreal maxBrushSize = KisImageConfig(true).maxBrushSize();
    const qreal effectiveMaxDragSize = 0.5 * screenRect.width();
    const qreal effectiveMaxBrushSize = qMin(maxBrushSize, effectiveMaxDragSize / scaleX);

    const qreal scaleCoeff = effectiveMaxBrushSize / effectiveMaxDragSize;
    const qreal sizeDiff = scaleCoeff * offset.x() ;

    if (qAbs(sizeDiff) > 0.01) {
        KisPaintOpSettingsSP settings = currentPaintOpPreset()->settings();

        qreal newSize = m_lastPaintOpSize + sizeDiff;

        if (action == ChangeSizeSnap) {
            newSize = qMax(qRound(newSize), 1);
        }

        newSize = qBound(0.01, newSize, maxBrushSize);

        settings->setPaintOpSize(newSize);

        requestUpdateOutline(
            m_initialGestureDocPoint,
            m_beginAlternateActionEvent.has_value() ? &m_beginAlternateActionEvent->event : nullptr);
        //m_brushResizeCompressor.start(newSize);

        m_lastDocumentPoint = event->point;
        m_lastPaintOpSize = newSize;
    }
}

void KisToolFreehand::endAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if (trySampleByPaintOp(event, action) || m_paintopBasedSamplingInAction) {
        m_paintopBasedSamplingInAction = false;
        return;
    }

    if (action != ChangeSize && action != ChangeSizeSnap) {
        KisToolPaint::endAlternateAction(event, action);
        return;
    }

    dynamic_cast<KisCanvasToolServices *>(canvas())->toolSetCursorPosition(m_initialGestureGlobalPoint);
    requestUpdateOutline(m_initialGestureDocPoint, 0);

    setMode(HOVER_MODE);

    m_beginAlternateActionEvent.reset();
}

bool KisToolFreehand::wantsAutoScroll() const
{
    return false;
}

void KisToolFreehand::setAssistant(bool assistant)
{
    m_assistant = assistant;
}

void KisToolFreehand::setOnlyOneAssistantSnap(bool assistant)
{
    m_only_one_assistant = assistant;
}

void KisToolFreehand::setSnapEraser(bool assistant)
{
    m_eraser_snapping = assistant;
}

void KisToolFreehand::slotDoResizeBrush(qreal newSize)
{
    KisPaintOpSettingsSP settings = currentPaintOpPreset()->settings();

    settings->setPaintOpSize(newSize);
    requestUpdateOutline(m_initialGestureDocPoint, 0);

}

QPointF KisToolFreehand::adjustPosition(const QPointF& point, const QPointF& strokeBegin)
{
    if (m_assistant) {
        return dynamic_cast<KisCanvasToolServices *>(canvas())->toolAdjustAssistantPosition(
            point, strokeBegin, m_magnetism, m_only_one_assistant, m_eraser_snapping);
    }
    return point;
}

qreal KisToolFreehand::calculatePerspective(const QPointF &documentPoint)
{
    return dynamic_cast<KisCanvasToolServices *>(canvas())->toolAssistantPerspective(documentPoint);
}

void KisToolFreehand::updateMaskSyntheticEventsFromTouch()
{
    setMaskSyntheticEvents(KisImageConfig(true).disableTouchOnCanvas(KoPointerEvent::tabletInputReceived()));
}

void KisToolFreehand::explicitUpdateOutline()
{
    requestUpdateOutline(m_outlineDocPoint, 0);
}

KisOptimizedBrushOutline KisToolFreehand::getOutlinePath(const QPointF &documentPos,
                                             const KoPointerEvent *event,
                                             KisPaintOpSettings::OutlineMode outlineMode)
{
    if (currentPaintOpPreset())
        return m_helper->paintOpOutline(convertToPixelCoord(documentPos),
                                        event,
                                        currentPaintOpPreset()->settings(),
                                        outlineMode);
    else
        return KisOptimizedBrushOutline();
}
