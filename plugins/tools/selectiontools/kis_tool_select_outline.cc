/*
 *  kis_tool_select_freehand.h - part of Krayon^WKrita
 *
 *  SPDX-FileCopyrightText: 2000 John Califf <jcaliff@compuzone.net>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *  SPDX-FileCopyrightText: 2015 Michael Abrahams <miabraha@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_select_outline.h"

#include <kis_debug.h>
#include <klocalizedstring.h>

#include <KoPointerEvent.h>
#include <KoShapeController.h>
#include <KoPathShape.h>
#include <KoColorSpace.h>
#include <KoCompositeOp.h>
#include <KoViewConverter.h>

#include <kis_layer.h>
#include <kis_selection_options.h>
#include <kis_image.h>
#include <kis_default_bounds.h>

#include "KisCanvasInvalidation.h"
#include "KisSelectionUtils.h"
#include "kis_painter.h"
#include "kis_pixel_selection.h"
#include "kis_selection_tool_helper.h"
#include <brushengine/kis_paintop_registry.h>
#include <kis_command_utils.h>
#include <kis_selection_filters.h>
#include <KisCursorOverrideLock.h>

#include "kis_algebra_2d.h"

__KisToolSelectOutlineLocal::__KisToolSelectOutlineLocal(KoCanvasBase * canvas)
    : KisToolOutlineBase(canvas, KisToolOutlineBase::SELECT,
                         dynamic_cast<KisCanvasToolServices*>(canvas)->toolLoadCursor("tool_outline_selection_cursor.png", 5, 5))
{
    setObjectName("tool_select_outline");
}


KisToolSelectOutline::KisToolSelectOutline(KoCanvasBase * canvas)
    : KisToolSelectBase<__KisToolSelectOutlineLocal>(canvas, PkString("Freehand Selection"))
{}

void KisToolSelectOutline::finishOutline(const PkVector<PkPointF>& points)
{
    KisImageSP image = currentImage().toStrongRef();
    KIS_ASSERT_RECOVER_RETURN(image);
    KisCanvasInvalidation *invalidation =
        dynamic_cast<KisCanvasInvalidation *>(canvas());
    KIS_ASSERT_RECOVER_RETURN(invalidation);
    invalidation->invalidateAll();

    const PkRectF boundingRect = KisAlgebra2D::accumulateBounds(points);
    const PkRectF boundingViewRect = pixelToView(boundingRect);

    KisSelectionToolHelper helper(
        canvas(), image, currentNode(), kundo2_i18n("Freehand Selection"));

    if (helper.tryDeselectCurrentSelection(boundingViewRect, selectionAction())) {
        endSelectInteraction();
        return;
    }

    if (points.count() < 3) {
        return;
    }

    KisCursorOverrideLock cursorLock(Qt::WaitCursor);

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
                                           kundo2_i18n("Freehand Selection"));

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
        KoPathShape *path = new KoPathShape();
        path->setShapeId(KoPathShapeId);

        PkTransform resolutionMatrix;
        resolutionMatrix.scale(1 / currentImage()->xRes(),
                               1 / currentImage()->yRes());
        path->moveTo(resolutionMatrix.map(points[0]));
        for (int i = 1; i < points.count(); i++)
            path->lineTo(resolutionMatrix.map(points[i]));
        path->close();
        path->normalize();

        helper.addSelectionShape(path, selectionAction());
    }
}

void KisToolSelectOutline::beginShape()
{
    beginSelectInteraction();
}

void KisToolSelectOutline::endShape()
{
    endSelectInteraction();
}

bool KisToolSelectOutline::primaryActionSupportsHiResEvents() const
{
    return !isMovingSelection();
}

bool KisToolSelectOutline::alternateActionSupportsHiResEvents(AlternateAction action) const
{
    /**
     * In selection tools we abuse alternate actions to switch different
     * selection modes. So we should notify input manager that we need a
     * good precision for them
     */

    (void)action;
    return !isMovingSelection();
}

void KisToolSelectOutline::resetCursorStyle()
{
    if (selectionAction() == SELECTION_ADD) {
        useCursor(dynamic_cast<KisCanvasToolServices*>(canvas())->toolLoadCursor("tool_outline_selection_cursor_add.png", 5, 5));
    } else if (selectionAction() == SELECTION_SUBTRACT) {
        useCursor(dynamic_cast<KisCanvasToolServices*>(canvas())->toolLoadCursor("tool_outline_selection_cursor_sub.png", 5, 5));
    } else if (selectionAction() == SELECTION_INTERSECT) {
        useCursor(dynamic_cast<KisCanvasToolServices*>(canvas())->toolLoadCursor("tool_outline_selection_cursor_inter.png", 5, 5));
    } else if (selectionAction() == SELECTION_SYMMETRICDIFFERENCE) {
        useCursor(dynamic_cast<KisCanvasToolServices*>(canvas())->toolLoadCursor("tool_outline_selection_cursor_symdiff.png", 5, 5));
    } else {
        KisToolSelectBase<__KisToolSelectOutlineLocal>::resetCursorStyle();
    }
}
