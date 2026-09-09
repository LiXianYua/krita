/*
 *  SPDX-FileCopyrightText: 2000 John Califf <jcaliff@compuzone.net>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *  SPDX-FileCopyrightText: 2015 Michael Abrahams <miabraha@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkFlakeBridge.h>

#include <KoPointerEvent.h>
#include <KoShapeController.h>
#include <KoViewConverter.h>
#include <KoCanvasBase.h>
#include <KisCanvasToolServices.h>
#include <KisOptimizedBrushOutline.h>

#include "KisToolOutlineBase.h"
#include "input/KisInputActionGroup.h"

KisToolOutlineBase::KisToolOutlineBase(KoCanvasBase * canvas, ToolType type, const QCursor & cursor)
    : KisToolShape(canvas, cursor)
    , m_continuedMode(false)
    , m_type(type)
    , m_numberOfContinuedModePoints(0)
    , m_hasUserInteractionRunning(false)
{}

KisToolOutlineBase::~KisToolOutlineBase()
{}

void KisToolOutlineBase::pkKeyPressEvent(PkToolKeyEvent *event)
{
    // Allow to enter continued mode only if we started drawing the shape
    if (mode() == PAINT_MODE && event->key() == Pk::Key_Control) {
        m_continuedMode = true;
        installBlockActionGuard();
    }
    KisToolShape::pkKeyPressEvent(event);
}

void KisToolOutlineBase::pkKeyReleaseEvent(PkToolKeyEvent *event)
{
    if (event->key() == Pk::Key_Control || !(event->modifiers() & Pk::ControlModifier)) {
        m_continuedMode = false;
        if (mode() != PAINT_MODE) {
            endStroke();
        }
    }
    KisToolShape::pkKeyReleaseEvent(event);
}

void KisToolOutlineBase::mouseMoveEvent(KoPointerEvent *event)
{
    if (m_continuedMode && mode() != PAINT_MODE) {
        updateContinuedMode();
        m_lastCursorPos = convertToPixelCoordAndSnap(event);
    } else {
        m_lastCursorPos = convertToPixelCoord(event);
    }
    if (mode() == PAINT_MODE) {
        KisToolShape::requestUpdateOutline(event->point, event);
    }

    KisToolShape::mouseMoveEvent(event);
}

void KisToolOutlineBase::activate(const PkSet<KoShape *> &shapes)
{
    KisToolShape::activate(shapes);
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(services);
    services->toolSetActionCallback(
        "undo_polygon_selection", this, callLifetime(), [this] { undoLastPoint(); }, true);
    services->toolSetPriorityRightClickCallback(
        this, callLifetime(),
        [this] {
            if (m_points.isEmpty()) {
                return false;
            }
            undoLastPoint();
            return true;
        },
        true);
}

void KisToolOutlineBase::deactivate()
{
    cancelStroke();
    
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(services);
    services->toolUpdateCanvas();

    m_continuedMode = false;

    services->toolSetPriorityRightClickCallback(this, callLifetime(), {}, false);

    KisToolShape::deactivate();
}

KisPopupWidgetInterface* KisToolOutlineBase::popupWidget()
{
    return !m_points.isEmpty() || m_type == SELECT ? nullptr : KisToolShape::popupWidget();
}

void KisToolOutlineBase::undoLastPoint()
{
    if(!m_points.isEmpty() && m_continuedMode && mode() != PAINT_MODE && m_numberOfContinuedModePoints > 0) {
        // Initialize with the dragging segment's rect
        PkRectF updateRect = dragBoundingRect();

        if (m_points.size() > 1) {
            // Add the rect for the last segment
            const PkRectF lastSegmentRect =
                pixelToView(PkRectF(m_points.last(), m_points.at(m_points.size() - 2)).normalized())
                .adjusted(-FEEDBACK_LINE_WIDTH, -FEEDBACK_LINE_WIDTH, FEEDBACK_LINE_WIDTH, FEEDBACK_LINE_WIDTH);
            updateRect = updateRect.united(lastSegmentRect);

            m_points.remove(m_points.size() - 1);
            --m_numberOfContinuedModePoints;
        }

        // Add the new dragging segment's rect
        updateRect = updateRect.united(dragBoundingRect());
        updateCanvasViewRect(updateRect);
    }
}

void KisToolOutlineBase::beginPrimaryAction(KoPointerEvent *event)
{
    NodePaintAbility paintability = nodePaintAbility();
    if ((m_type == PAINT && (!nodeEditable() || paintability == UNPAINTABLE || paintability  == KisToolPaint::CLONE || paintability == KisToolPaint::MYPAINTBRUSH_UNPAINTABLE)) || (m_type == SELECT && !selectionEditable())) {

        if (paintability == KisToolPaint::CLONE ||
            paintability == KisToolPaint::MYPAINTBRUSH_UNPAINTABLE) {
            dynamic_cast<KisCanvasToolServices*>(canvas())->toolShowLockedLayerMessage(
                paintability == KisToolPaint::MYPAINTBRUSH_UNPAINTABLE);
        }

        event->ignore();
        return;
    }

    setMode(KisTool::PAINT_MODE);

    if (!m_continuedMode || m_points.isEmpty()) {
        m_hasUserInteractionRunning = true;
        beginShape();
    }

    if (m_continuedMode) {
        m_points.append(convertToPixelCoordAndSnap(event));
        ++m_numberOfContinuedModePoints;
    } else {
        m_numberOfContinuedModePoints = 0;
        m_points.append(convertToPixelCoord(event));
    }
}

void KisToolOutlineBase::continuePrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    PkPointF point = convertToPixelCoord(event);
    m_points.append(point);
    updateFeedback();
}

void KisToolOutlineBase::endPrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    setMode(KisTool::HOVER_MODE);
    if (!m_continuedMode) {
        // If the event was not originated by the user releasing the button
        // (for example due to the canvas loosing focus), then we just cancel
        // the operation. This prevents some issues with shapes being added
        // after the image was closed while the shape was being made
        if (event->spontaneous()) {
            endStroke();
        } else {
            cancelStroke();
        }
        event->accept();
    }
}

void KisToolOutlineBase::paint(PkPainter &gc, const KoViewConverter &converter)
{
    if ((mode() == KisTool::PAINT_MODE || m_continuedMode) && !m_points.isEmpty()) {
        PkPainterPath outline;
        outline.moveTo(pixelToView(m_points.first()));
        for (qint32 i = 1; i < m_points.size(); ++i) {
            outline.lineTo(pixelToView(m_points[i]));
        }
        if (m_continuedMode && mode() != KisTool::PAINT_MODE) {
            outline.lineTo(pixelToView(m_lastCursorPos));
        }
        paintToolOutline(&gc, KisOptimizedBrushOutline(outline));
    }

    KisToolShape::paint(gc, converter);
}

void KisToolOutlineBase::updateFeedback()
{
    if (m_points.count() > 1) {
        qint32 lastPointIndex = m_points.count() - 1;

        PkRectF updateRect = PkRectF(m_points[lastPointIndex - 1], m_points[lastPointIndex]).normalized();
        updateRect = kisGrowRect(updateRect, FEEDBACK_LINE_WIDTH);

        updateCanvasPixelRect(updateRect);
    }
}

PkRectF KisToolOutlineBase::dragBoundingRect()
{
    PkRectF updateRect = pixelToView(PkRectF(m_points.last(), m_lastCursorPos).normalized());
    updateRect = kisGrowRect(updateRect, FEEDBACK_LINE_WIDTH);
    return updateRect;
}

void KisToolOutlineBase::updateContinuedMode()
{
    if (!m_points.isEmpty()) {
        updateCanvasViewRect(dragBoundingRect());
    }
}

bool KisToolOutlineBase::hasUserInteractionRunning() const
{
    return m_hasUserInteractionRunning;
}

void KisToolOutlineBase::endStroke()
{
    if (!hasUserInteractionRunning()) {
        return;
    }
    uninstallBlockActionGuard();
    setMode(KisTool::HOVER_MODE);
    m_hasUserInteractionRunning = false;

    finishOutline(m_points);
    m_points.clear();
    endShape();
}

void KisToolOutlineBase::cancelStroke()
{
    if (!hasUserInteractionRunning()) {
        return;
    }
    uninstallBlockActionGuard();
    setMode(KisTool::HOVER_MODE);
    m_hasUserInteractionRunning = false;

    m_points.clear();
    endShape();
}

void KisToolOutlineBase::requestStrokeEnd()
{
    endStroke();
}

void KisToolOutlineBase::requestStrokeCancellation()
{
    cancelStroke();
}

void KisToolOutlineBase::installBlockActionGuard()
{
    if (m_blockModifyingActionsGuard)
        return;
    m_blockModifyingActionsGuard.reset(new KisInputActionGroupsMaskGuard(
        dynamic_cast<KisCanvasToolServices*>(canvas())->toolInputActionGroupsMaskInterface(),
                                 ViewTransformActionGroup | ToolInvoactionActionGroup
                                ));
}

void KisToolOutlineBase::uninstallBlockActionGuard()
{
    m_blockModifyingActionsGuard.reset();
}
