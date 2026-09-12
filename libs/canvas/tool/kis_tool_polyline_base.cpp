/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */


#include <PkPainterPath.h>
#include <PkPen.h>
#include <PkFlakeBridge.h>

#include <KoPointerEvent.h>
#include <KoCanvasBase.h>
#include <KoCanvasController.h>
#include <KoViewConverter.h>
#include "kis_tool_polyline_base.h"
#include <KisCanvasToolServices.h>
#include <KisOptimizedBrushOutline.h>

#define SNAPPING_THRESHOLD 10
#define SNAPPING_HANDLE_RADIUS 8
#define PREVIEW_LINE_WIDTH 1

KisToolPolylineBase::KisToolPolylineBase(KoCanvasBase * canvas,  KisToolPolylineBase::ToolType type, KisCanvasCursorToken cursor)
    : KisToolShape(canvas, cursor),
      m_dragging(false),
      m_type(type),
      m_closeSnappingActivated(false)
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices*>(canvas);
    KIS_SAFE_ASSERT_RECOVER_RETURN(services);
    PkObject::connect(services->toolSignals(),
                      &KisCanvasToolSignals::effectiveCompositeOpChanged,
                      this,
                      &KisToolPolylineBase::resetCursorStyle);
}


void KisToolPolylineBase::activate(const PkSet<KoShape *> &shapes)
{
    KisToolShape::activate(shapes);
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices*>(canvas());
    KIS_ASSERT_RECOVER_RETURN(services);
    services->toolSetActionCallback(
        "undo_polygon_selection", this, callLifetime(),
        [this] { undoSelectionOrCancel(); }, true);
    services->toolSetPriorityRightClickCallback(
        this, callLifetime(),
        [this] {
            if (!m_dragging) {
                return false;
            }
            undoSelectionOrCancel();
            return true;
        },
        true);
}

void KisToolPolylineBase::deactivate()
{
    cancelStroke();

    dynamic_cast<KisCanvasToolServices*>(canvas())->toolSetPriorityRightClickCallback(
        this, callLifetime(), {}, false);

    KisToolShape::deactivate();
}

void KisToolPolylineBase::requestStrokeEnd()
{
    endStroke();
}

void KisToolPolylineBase::requestStrokeCancellation()
{
    cancelStroke();
}

KisPopupWidgetInterface* KisToolPolylineBase::popupWidget()
{
    return m_dragging || m_type == SELECT ? nullptr : KisToolShape::popupWidget();
}

void KisToolPolylineBase::beginPrimaryAction(KoPointerEvent *event)
{
    Q_UNUSED(event);
    NodePaintAbility paintability = nodePaintAbility();
    if ((m_type == PAINT && (!nodeEditable() || paintability == UNPAINTABLE || paintability  == KisToolPaint::CLONE || paintability == KisToolPaint::MYPAINTBRUSH_UNPAINTABLE)) ||
        (m_type == SELECT && !selectionEditable())) {

        if (paintability == KisToolPaint::CLONE ||
            paintability == KisToolPaint::MYPAINTBRUSH_UNPAINTABLE) {
            dynamic_cast<KisCanvasToolServices*>(canvas())->toolShowLockedLayerMessage(
                paintability == KisToolPaint::MYPAINTBRUSH_UNPAINTABLE);
        }

        event->ignore();
        return;
    }

    setMode(KisTool::PAINT_MODE);

    if(m_dragging && m_closeSnappingActivated) {
        m_points.append(m_points.first());
        endStroke();
    } else {
        beginShape();
        m_dragging = true;
    }
}

void KisToolPolylineBase::endPrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    setMode(KisTool::HOVER_MODE);

    if(m_dragging) {
        m_dragStart = convertToPixelCoordAndSnap(event);
        m_dragEnd = m_dragStart;
        m_points.append(m_dragStart);
    }
}

void KisToolPolylineBase::beginPrimaryDoubleClickAction(KoPointerEvent *event)
{
    endStroke();

    // this action will have no continuation
    event->ignore();
}

void KisToolPolylineBase::beginAlternateAction(KoPointerEvent *event, AlternateAction action)
{
    if ((action != ChangeSize && action != ChangeSizeSnap) || !m_dragging) {
        KisToolPaint::beginAlternateAction(event, action);
    }

    if (m_closeSnappingActivated) {
        m_points.append(m_points.first());
    }
    endStroke();
}

void KisToolPolylineBase::mouseMoveEvent(KoPointerEvent *event)
{
    if (m_dragging && !m_points.empty()) {
        // erase old lines on canvas
        PkRectF updateRect = dragBoundingRect();
        // get current mouse position
        m_dragEnd = convertToPixelCoordAndSnap(event);
        // draw new lines on canvas
        updateRect |= dragBoundingRect();
        updateCanvasViewRect(updateRect);


        PkPointF basePoint = pixelToView(m_points.first());
        m_closeSnappingActivated =
            m_points.size() > 1 &&
            (basePoint - pixelToView(m_dragEnd)).manhattanLength() < SNAPPING_THRESHOLD;

        updateCanvasViewRect(PkRectF(basePoint, 2 * PkSize(SNAPPING_HANDLE_RADIUS + PREVIEW_LINE_WIDTH, SNAPPING_HANDLE_RADIUS + PREVIEW_LINE_WIDTH)).translated(-SNAPPING_HANDLE_RADIUS + PREVIEW_LINE_WIDTH,-SNAPPING_HANDLE_RADIUS + PREVIEW_LINE_WIDTH));
        KisToolPaint::requestUpdateOutline(event->point, event);
    } else {
        KisToolPaint::mouseMoveEvent(event);
    }
}

void KisToolPolylineBase::undoSelection()
{
    if (m_dragging) {
        // Initialize with the dragging segment's rect
        PkRectF updateRect = dragBoundingRect();

        if (m_points.size() > 1) {
            // Add the rect for the last segment
            const PkRectF lastSegmentRect =
                pixelToView(PkRectF(m_points.last(),
                                   m_points.at(m_points.size() - 2)).normalized())
                .adjusted(-PREVIEW_LINE_WIDTH, -PREVIEW_LINE_WIDTH, PREVIEW_LINE_WIDTH, PREVIEW_LINE_WIDTH);
            updateRect = updateRect.united(lastSegmentRect);

            m_points.remove(m_points.size() - 1);
        }
        m_dragStart = m_points.last();

        // Add the new dragging segment's rect
        updateRect = updateRect.united(dragBoundingRect());
        updateCanvasViewRect(updateRect);
    }
}

void KisToolPolylineBase::undoSelectionOrCancel()
{
    if (m_points.size() > 1) {
        undoSelection();
    } else {
        cancelStroke();
    }
}

void KisToolPolylineBase::paint(PkPainter &gc, const KoViewConverter &converter)
{
    Q_UNUSED(converter);

    if (!canvas() || !currentImage())
        return;

    PkPointF start, end;
    PkPointF startPos;
    PkPointF endPos;

    PkPainterPath path;
    if (m_dragging && !m_points.empty()) {
        startPos = pixelToView(m_dragStart);
        endPos = pixelToView(m_dragEnd);
        path.moveTo(startPos);
        path.lineTo(endPos);
    }

    for (PkVector<PkPointF>::iterator it = m_points.begin(); it != m_points.end(); ++it) {

        if (it == m_points.begin()) {
            start = *it;
        } else {
            end = *it;

            startPos = pixelToView(start);
            endPos = pixelToView(end);
            path.moveTo(startPos);
            path.lineTo(endPos);
            start = end;
        }
    }

    if (m_closeSnappingActivated) {
        PkPointF basePoint = pixelToView(m_points.first());
        path.addEllipse(basePoint, SNAPPING_HANDLE_RADIUS, SNAPPING_HANDLE_RADIUS);
    }

    paintToolOutline(&gc, KisOptimizedBrushOutline(path));
    KisToolPaint::paint(gc,converter);
}

void KisToolPolylineBase::updateArea()
{
    const PkRect bounds = image()->bounds();
    updateCanvasPixelRect(PkRectF(bounds.x(), bounds.y(), bounds.width(), bounds.height()));
}

void KisToolPolylineBase::endStroke()
{
    if (!m_dragging) return;

    m_dragging = false;
    if(m_points.count() > 1) {
        PkVector<PkPointF> points;
        points.reserve(m_points.size());
        for (const PkPointF &point : m_points) {
            points.append(point);
        }
        finishPolyline(points);
    }
    m_points.clear();
    m_closeSnappingActivated = false;
    updateArea();
    endShape();
}

void KisToolPolylineBase::cancelStroke()
{
    if (!m_dragging) return;

    m_dragging = false;
    m_points.clear();
    m_closeSnappingActivated = false;
    updateArea();
    endShape();
}

PkRectF KisToolPolylineBase::dragBoundingRect()
{
    PkRectF rect = pixelToView(PkRectF(m_dragStart, m_dragEnd).normalized());
    rect.adjust(-PREVIEW_LINE_WIDTH, -PREVIEW_LINE_WIDTH, PREVIEW_LINE_WIDTH, PREVIEW_LINE_WIDTH);
    return rect;
}
