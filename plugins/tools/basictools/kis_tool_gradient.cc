/*
 *  kis_tool_gradient.cc - part of Krita
 *
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2003 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2004-2007 Adrian Page <adrian@pagenet.plus.com>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_gradient.h"

#include <cfloat>

#include <QApplication>
#include <QPainter>
#include <QLayout>

#include <kis_transaction.h>
#include <kis_debug.h>
#include <klocalizedstring.h>


#include <KoPointerEvent.h>
#include <KoCanvasBase.h>
#include <KoViewConverter.h>
#include <KoUpdater.h>
#include <KoProgressUpdater.h>

#include <kis_gradient_painter.h>
#include <kis_painter.h>
#include <kis_canvas_resource_provider.h>
#include <kis_layer.h>
#include <kis_selection.h>
#include <kis_paint_layer.h>

#include <canvas/kis_canvas2.h>
#include <KisViewManager.h>
#include <kis_cursor.h>
#include <kis_config.h>
#include "kis_resources_snapshot.h"
#include "kis_command_utils.h"
#include "kis_processing_applicator.h"
#include "kis_processing_visitor.h"


KisToolGradient::KisToolGradient(KoCanvasBase * canvas)
        : KisToolPaint(canvas, KisCursor::load("tool_gradient_cursor.png", 6, 6))
{
    setObjectName("tool_gradient");

    m_startPos = QPointF(0, 0);
    m_endPos = QPointF(0, 0);

    m_dither = false;
    m_reverse = false;
    m_shape = KisGradientPainter::GradientShapeLinear;
    m_repeat = KisGradientPainter::GradientRepeatNone;
    m_antiAliasThreshold = 0.0;

    KisCanvas2 *kritaCanvas = dynamic_cast<KisCanvas2*>(canvas);

    connect(kritaCanvas->viewManager()->canvasResourceProvider(), SIGNAL(sigEffectiveCompositeOpChanged()), SLOT(resetCursorStyle()));
}

KisToolGradient::~KisToolGradient()
{
}

void KisToolGradient::resetCursorStyle()
{
    if (isEraser()) {
        useCursor(KisCursor::load("tool_gradient_eraser_cursor.png", 6, 6));
    } else {
        KisToolPaint::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisToolGradient::activate(const QSet<KoShape*> &shapes)
{
    KisToolPaint::activate(shapes);
}

void KisToolGradient::paint(QPainter &painter, const KoViewConverter &converter)
{
    if (mode() == KisTool::PAINT_MODE && m_startPos != m_endPos) {
        paintLine(painter);
    }
    KisToolPaint::paint(painter, converter);
}

void KisToolGradient::beginPrimaryAction(KoPointerEvent *event)
{
    if (!nodeEditable()) {
        event->ignore();
        return;
    }

    setMode(KisTool::PAINT_MODE);

    m_startPos = convertToPixelCoordAndSnap(event, QPointF(), false);
    m_endPos = m_startPos;
}

void KisToolGradient::continuePrimaryAction(KoPointerEvent *event)
{
    /**
     * TODO: The gradient tool is still not in strokes, so the end of
     *       its action can call processEvent(), which would result in
     *       nested event handler calls. Please uncomment this line
     *       when the tool is ported to strokes.
     */
    //CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    // First ensure the old guideline is deleted
    updateGuideline();

    QPointF pos = convertToPixelCoordAndSnap(event, QPointF(), false);

    if (event->modifiers() == Qt::ShiftModifier) {
        m_endPos = straightLine(pos);
    } else {
        m_endPos = pos;
    }

    updateGuideline();
}

void KisToolGradient::endPrimaryAction(KoPointerEvent *event)
{
    Q_UNUSED(event);
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    setMode(KisTool::HOVER_MODE);

    if (!currentNode())
        return;

    if (m_startPos == m_endPos) {
        return;
    }

    KisImageSP image = this->image();

    KisResourcesSnapshotSP resources =
        new KisResourcesSnapshot(image, currentNode(), this->canvas()->resourceManager());

    if (image && resources->currentNode()->paintDevice()) {
        KUndo2MagicString actionName = kundo2_i18n("Gradient");
        KisProcessingApplicator applicator(image, resources->currentNode(),
                                           KisProcessingApplicator::NONE,
                                           KisImageSignalVector(),
                                           actionName);

        applicator.applyCommand(
            new KisCommandUtils::LambdaCommand(
                [resources, startPos = m_startPos, endPos = m_endPos,
                 shape = m_shape, repeat = m_repeat, reverse = m_reverse,
                 antiAliasThreshold = m_antiAliasThreshold, dither = m_dither] () mutable {

                    KisNodeSP node = resources->currentNode();
                    KisPaintDeviceSP device = node->paintDevice();
                    KisProcessingVisitor::ProgressHelper helper(node);
                    const QRect bounds = device->defaultBounds()->bounds();

                    KisGradientPainter painter(device, resources->activeSelection());
                    resources->setupPainter(&painter);
                    painter.setProgress(helper.updater());

                    painter.beginTransaction();

                    painter.setGradientShape(shape);
                    painter.paintGradient(startPos, endPos,
                                          repeat, antiAliasThreshold,
                                          reverse, 0, 0,
                                          bounds.width(), bounds.height(),
                                          dither);

                    return painter.endAndTakeTransaction();
                }));
        applicator.end();
    }

    updateGuideline();
}

QPointF KisToolGradient::straightLine(QPointF point)
{
    QPointF comparison = point - m_startPos;
    QPointF result;

    if (fabs(comparison.x()) > fabs(comparison.y())) {
        result.setX(point.x());
        result.setY(m_startPos.y());
    } else {
        result.setX(m_startPos.x());
        result.setY(point.y());
    }

    return result;
}

void KisToolGradient::paintLine(QPainter& gc)
{
    QPointF viewStartPos = pixelToView(m_startPos);
    QPointF viewStartEnd = pixelToView(m_endPos);

    if (canvas()) {
        QPainterPath path;
        path.moveTo(viewStartPos);
        path.lineTo(viewStartEnd);
        paintToolOutline(&gc, path);
    }
}

void KisToolGradient::updateGuideline()
{
    if (canvas()) {
        QRectF bound(m_startPos, m_endPos);
        canvas()->updateCanvas(convertToPt(bound.normalized().adjusted(-3, -3, 3, 3)));
    }
}



