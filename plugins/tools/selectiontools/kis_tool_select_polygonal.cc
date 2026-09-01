/*
 *  kis_tool_select_polygonal.h - part of Krayon^WKrita
 *
 *  SPDX-FileCopyrightText: 2000 John Califf <jcaliff@compuzone.net>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *  SPDX-FileCopyrightText: 2015 Michael Abrahams <miabraha@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_select_polygonal.h"

#include <KoPathShape.h>

#include "kis_algebra_2d.h"
#include "kis_painter.h"
#include <brushengine/kis_paintop_registry.h>
#include "kis_selection_options.h"
#include "KisSelectionUtils.h"
#include "kis_pixel_selection.h"
#include "kis_selection_tool_helper.h"
#include "kis_shape_tool_helper.h"
#include <kis_default_bounds.h>

#include <kis_command_utils.h>
#include <kis_selection_filters.h>

namespace {
constexpr int SnappingThreshold = 10;
constexpr int SnappingHandleRadius = 8;
constexpr int PreviewLineWidth = 1;
}

__KisToolSelectPolygonalLocal::__KisToolSelectPolygonalLocal(KoCanvasBase *canvas)
    : KisToolPolylineBase(canvas, KisToolPolylineBase::SELECT,
                          dynamic_cast<KisCanvasToolServices*>(canvas)->toolLoadCursor("tool_polygonal_selection_cursor.png", 6, 6))
{
    setObjectName("tool_select_polygonal");
}

void __KisToolSelectPolygonalLocal::activate(const PkSet<KoShape *> &shapes)
{
    KisToolShape::activate(shapes);
}

void __KisToolSelectPolygonalLocal::deactivate()
{
    cancelStroke();
    KisToolShape::deactivate();
}

void __KisToolSelectPolygonalLocal::requestStrokeEnd()
{
    endStroke();
}

void __KisToolSelectPolygonalLocal::requestStrokeCancellation()
{
    cancelStroke();
}

KisPopupWidgetInterface *__KisToolSelectPolygonalLocal::popupWidget()
{
    return nullptr;
}

void __KisToolSelectPolygonalLocal::beginPrimaryAction(KoPointerEvent *event)
{
    if (!selectionEditable()) {
        event->ignore();
        return;
    }

    setMode(KisTool::PAINT_MODE);
    if (m_dragging && m_closeSnappingActivated) {
        m_points.append(m_points.first());
        endStroke();
    } else {
        beginShape();
        m_dragging = true;
    }
}

void __KisToolSelectPolygonalLocal::endPrimaryAction(KoPointerEvent *event)
{
    if (mode() != KisTool::PAINT_MODE) return;
    setMode(KisTool::HOVER_MODE);

    if (m_dragging) {
        m_dragStart = convertToPixelCoordAndSnap(event);
        m_dragEnd = m_dragStart;
        m_points.append(m_dragStart);
    }
}

void __KisToolSelectPolygonalLocal::beginPrimaryDoubleClickAction(KoPointerEvent *event)
{
    endStroke();
    event->ignore();
}

void __KisToolSelectPolygonalLocal::beginAlternateAction(
    KoPointerEvent *event, AlternateAction action)
{
    if ((action != ChangeSize && action != ChangeSizeSnap) || !m_dragging) {
        KisToolPaint::beginAlternateAction(event, action);
    }

    if (m_closeSnappingActivated) {
        m_points.append(m_points.first());
    }
    endStroke();
}

void __KisToolSelectPolygonalLocal::mouseMoveEvent(KoPointerEvent *event)
{
    if (m_dragging && !m_points.empty()) {
        PkRectF updateRect = dragBoundingRect();
        m_dragEnd = convertToPixelCoordAndSnap(event);
        updateRect |= dragBoundingRect();
        updateCanvasViewRect(updateRect);

        const PkPointF basePoint = pixelToView(m_points.first());
        m_closeSnappingActivated =
            m_points.size() > 1 &&
            (basePoint - pixelToView(m_dragEnd)).manhattanLength() < SnappingThreshold;

        updateCanvasViewRect(PkRectF(
            basePoint.x() - SnappingHandleRadius + PreviewLineWidth,
            basePoint.y() - SnappingHandleRadius + PreviewLineWidth,
            2 * (SnappingHandleRadius + PreviewLineWidth),
            2 * (SnappingHandleRadius + PreviewLineWidth)));
        KisToolPaint::requestUpdateOutline(event->point, event);
    } else {
        KisToolPaint::mouseMoveEvent(event);
    }
}

void __KisToolSelectPolygonalLocal::undoSelection()
{
    if (!m_dragging) return;

    PkRectF updateRect = dragBoundingRect();
    if (m_points.size() > 1) {
        const PkRectF lastSegmentRect =
            pixelToView(PkRectF(m_points.last(), m_points.at(m_points.size() - 2)).normalized())
                .adjusted(-PreviewLineWidth,
                          -PreviewLineWidth,
                          PreviewLineWidth,
                          PreviewLineWidth);
        updateRect = updateRect.united(lastSegmentRect);
        m_points.remove(m_points.size() - 1);
    }
    m_dragStart = m_points.last();
    updateCanvasViewRect(updateRect.united(dragBoundingRect()));
}

void __KisToolSelectPolygonalLocal::undoSelectionOrCancel()
{
    if (m_points.size() > 1) {
        undoSelection();
    } else {
        cancelStroke();
    }
}

void __KisToolSelectPolygonalLocal::paint(
    PkPainter &painter, const KoViewConverter &converter)
{
    if (!canvas() || !currentImage()) return;

    PkPainterPath path;
    if (m_dragging && !m_points.empty()) {
        path.moveTo(pixelToView(m_dragStart));
        path.lineTo(pixelToView(m_dragEnd));
    }

    for (int i = 1; i < m_points.size(); ++i) {
        path.moveTo(pixelToView(m_points.at(i - 1)));
        path.lineTo(pixelToView(m_points.at(i)));
    }

    if (m_closeSnappingActivated) {
        path.addEllipse(pixelToView(m_points.first()),
                        SnappingHandleRadius,
                        SnappingHandleRadius);
    }

    paintToolOutline(&painter, path);
    KisToolPaint::paint(painter, converter);
}

void __KisToolSelectPolygonalLocal::updateArea()
{
    updateCanvasPixelRect(image()->bounds());
}

void __KisToolSelectPolygonalLocal::endStroke()
{
    if (!m_dragging) return;

    m_dragging = false;
    if (m_points.count() > 1) {
        finishPolyline(m_points);
    }
    m_points.clear();
    m_closeSnappingActivated = false;
    updateArea();
    endShape();
}

void __KisToolSelectPolygonalLocal::cancelStroke()
{
    if (!m_dragging) return;

    m_dragging = false;
    m_points.clear();
    m_closeSnappingActivated = false;
    updateArea();
    endShape();
}

PkRectF __KisToolSelectPolygonalLocal::dragBoundingRect()
{
    return pixelToView(PkRectF(m_dragStart, m_dragEnd).normalized())
        .adjusted(-PreviewLineWidth,
                  -PreviewLineWidth,
                  PreviewLineWidth,
                  PreviewLineWidth);
}


KisToolSelectPolygonal::KisToolSelectPolygonal(KoCanvasBase *canvas):
    KisToolSelectBase<__KisToolSelectPolygonalLocal>(canvas, PkString("Polygonal Selection"))
{
}

void KisToolSelectPolygonal::finishPolyline(const PkVector<PkPointF> &points)
{
    KisImageSP image = currentImage().toStrongRef();
    if (!image)
        return;

    const PkRectF boundingViewRect = pixelToView(KisAlgebra2D::accumulateBounds(points));

    KisSelectionToolHelper helper(
        canvas(), image, currentNode(), kundo2_i18n("Select Polygon"));

    if (helper.tryDeselectCurrentSelection(pixelToView(boundingViewRect), selectionAction())) {
        return;
    }

    const SelectionMode mode =
        helper.tryOverrideSelectionMode(
            KisSelectionUtils::activeSelectionForNode(image, currentNode()),
            selectionMode(),
            selectionAction());

    if (mode == PIXEL_SELECTION) {
        KisProcessingApplicator applicator(currentImage(),
                                           currentNode(),
                                           KisProcessingApplicator::NONE,
                                           KisImageSignalVector(),
                                           kundo2_i18n("Select Polygon"));

        KisPixelSelectionSP tmpSel =
            new KisPixelSelection(new KisDefaultBounds(currentImage()));

        const bool antiAlias = antiAliasSelection();
        const int grow = growSelection();
        const int feather = featherSelection();

        PkPainterPath path;
        path.addPolygon(points);
        path.closeSubpath();

        KUndo2Command *cmd = new KisCommandUtils::LambdaCommand(
            [tmpSel, antiAlias, grow, feather, path]() mutable
            -> KUndo2Command * {
                KisPainter painter(tmpSel);
                painter.setPaintColor(KoColor(Qt::black, tmpSel->colorSpace()));
                // Since the feathering already smooths the selection, the
                // antiAlias is not applied if we must feather
                painter.setAntiAliasPolygonFill(antiAlias && feather == 0);
                painter.setFillStyle(KisPainter::FillStyleForegroundColor);
                painter.setStrokeStyle(KisPainter::StrokeStyleNone);

                painter.paintPainterPath(path);

                if (grow > 0) {
                    KisGrowSelectionFilter biggy(grow, grow);
                    biggy.process(tmpSel,
                                  tmpSel->selectedRect().adjusted(-grow,
                                                                  -grow,
                                                                  grow,
                                                                  grow));
                } else if (grow < 0) {
                    KisShrinkSelectionFilter tiny(-grow, -grow, false);
                    tiny.process(tmpSel, tmpSel->selectedRect());
                }
                if (feather > 0) {
                    KisFeatherSelectionFilter feathery(feather);
                    feathery.process(tmpSel,
                                     tmpSel->selectedRect().adjusted(-feather,
                                                                     -feather,
                                                                     feather,
                                                                     feather));
                }

                if (grow == 0 && feather == 0) {
                    tmpSel->setOutlineCache(path);
                } else {
                    tmpSel->invalidateOutlineCache();
                }

                return 0;
            });

        applicator.applyCommand(cmd, KisStrokeJobData::SEQUENTIAL);
        helper.selectPixelSelection(applicator, tmpSel, selectionAction());
        applicator.end();

    } else {
        KoPathShape* path = new KoPathShape();
        path->setShapeId(KoPathShapeId);

        PkTransform resolutionMatrix;
        resolutionMatrix.scale(1 / currentImage()->xRes(), 1 / currentImage()->yRes());
        path->moveTo(resolutionMatrix.map(points[0]));
        for (int i = 1; i < points.count(); i++)
            path->lineTo(resolutionMatrix.map(points[i]));
        path->close();
        path->normalize();

        helper.addSelectionShape(path, selectionAction());
    }
}

void KisToolSelectPolygonal::beginShape()
{
    beginSelectInteraction();
}

void KisToolSelectPolygonal::endShape()
{
    endSelectInteraction();
}

void KisToolSelectPolygonal::resetCursorStyle()
{
    if (selectionAction() == SELECTION_ADD) {
        useCursor(dynamic_cast<KisCanvasToolServices*>(canvas())->toolLoadCursor("tool_polygonal_selection_cursor_add.png", 6, 6));
    } else if (selectionAction() == SELECTION_SUBTRACT) {
        useCursor(dynamic_cast<KisCanvasToolServices*>(canvas())->toolLoadCursor("tool_polygonal_selection_cursor_sub.png", 6, 6));
    } else if (selectionAction() == SELECTION_INTERSECT) {
        useCursor(dynamic_cast<KisCanvasToolServices*>(canvas())->toolLoadCursor("tool_polygonal_selection_cursor_inter.png", 6, 6));
    } else if (selectionAction() == SELECTION_SYMMETRICDIFFERENCE) {
        useCursor(dynamic_cast<KisCanvasToolServices*>(canvas())->toolLoadCursor("tool_polygonal_selection_cursor_symdiff.png", 6, 6));
    } else {
        KisToolSelectBase<__KisToolSelectPolygonalLocal>::resetCursorStyle();
    }
}
