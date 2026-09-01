/*
 *  kis_tool_line.cc - part of Krayon
 *
 *  SPDX-FileCopyrightText: 2000 John Califf <jwcaliff@compuzone.net>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2003 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2009 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2007, 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_line.h"



#include <KoCanvasBase.h>
#include <KoCanvasResourceProvider.h>
#include <KoPointerEvent.h>
#include <KoPathShape.h>
#include <KoShapeController.h>
#include <KoShapeStroke.h>

#include <kis_debug.h>
#include <KisCanvasToolServices.h>
#include <brushengine/kis_paintop_registry.h>
#include <kis_figure_painting_tool_helper.h>
#include <KisCanvasFeedback.h>
#include <kis_coordinates_converter.h>
#include <KisCanvasToolServices.h>
#include <kis_painting_information_builder.h>

#include "kis_tool_line_helper.h"
#include "kis_basic_tools_string_utils.h"


const KisCoordinatesConverter* getCoordinatesConverter(KoCanvasBase * canvas)
{
    const KisCoordinatesConverter *converter =
        dynamic_cast<const KisCoordinatesConverter *>(canvas->viewConverter());
    KIS_ASSERT(converter);
    return converter;
}


KisToolLine::KisToolLine(KoCanvasBase * canvas)
    : KisToolShape(canvas, dynamic_cast<KisCanvasToolServices *>(canvas)->toolLoadCursor("tool_line_cursor.png", 6, 6)),
      m_showGuideline(true),
      m_strokeIsRunning(false),
      m_infoBuilder(new KisConverterPaintingInformationBuilder(getCoordinatesConverter(canvas))),
      m_helper(new KisToolLineHelper(m_infoBuilder.data(),
                                     canvas->resourceManager(),
                                     kundo2_i18n("Draw Line"))),
      m_strokeUpdateCompressor(200, KisSignalCompressor::POSTPONE),
      m_longStrokeUpdateCompressor(750, KisSignalCompressor::FIRST_INACTIVE)
{
    setObjectName("tool_line");

    setSupportOutline(true);

    setIsOpacityPresetMode(true);

    m_strokeUpdateConnection =
        PkObject::connect(&m_strokeUpdateCompressor, &KisSignalCompressor::timeout,
                          &m_strokeUpdateCompressor, [this]() { updateStroke(); });
    m_longStrokeUpdateConnection =
        PkObject::connect(&m_longStrokeUpdateCompressor, &KisSignalCompressor::timeout,
                          &m_longStrokeUpdateCompressor, [this]() { updateStroke(); });

    connect(canvas->resourceManager(), &KoCanvasResourceProvider::canvasResourceChanged,
            this, [this](int key, const PkVariant &) {
                if (key == KoCanvasResource::CurrentEffectiveCompositeOp) {
                    resetCursorStyle();
                }
            });
}

KisToolLine::~KisToolLine()
{
    PkObject::disconnect(m_longStrokeUpdateConnection);
    PkObject::disconnect(m_strokeUpdateConnection);
}

void KisToolLine::resetCursorStyle()
{
    if (isEraser() && (nodePaintAbility() == PAINT)) {
        useCursor(dynamic_cast<KisCanvasToolServices *>(canvas())->toolLoadCursor("tool_line_eraser_cursor.png", 6, 6));
    } else {
        KisToolPaint::resetCursorStyle();
    }

    overrideCursorIfNotEditable();
}

void KisToolLine::activate(const PkSet<KoShape*> &shapes)
{
   KisToolPaint::activate(shapes);
}

void KisToolLine::deactivate()
{
    KisToolPaint::deactivate();
    cancelStroke();
}

void KisToolLine::requestStrokeCancellation()
{
    cancelStroke();
}

void KisToolLine::requestStrokeEnd()
{
    // Terminate any in-progress strokes
    if (nodePaintAbility() == PAINT && m_helper->isRunning()) {
        endStroke();
    }
}

void KisToolLine::updatePreviewTimer(bool showGuideline)
{
    // If the user disables the guideline, we will want to try to draw some
    // preview lines even if they're slow, so set the timer to FIRST_ACTIVE.
    if (showGuideline) {
        m_strokeUpdateCompressor.setMode(KisSignalCompressor::POSTPONE);
    } else {
        m_strokeUpdateCompressor.setMode(KisSignalCompressor::FIRST_ACTIVE);
    }
}


void KisToolLine::paint(PkPainter& gc, const KoViewConverter &converter)
{
    (void)converter;

    if(mode() == KisTool::PAINT_MODE) {
        paintLine(gc,PkRect());
    }
    KisToolPaint::paint(gc,converter);
}

void KisToolLine::beginPrimaryAction(KoPointerEvent *event)
{
    NodePaintAbility nodeAbility = nodePaintAbility();
    if (nodeAbility == UNPAINTABLE || !nodeEditable()) {
        event->ignore();
        return;
    }

    if (nodeAbility == MYPAINTBRUSH_UNPAINTABLE) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER(feedback) {
            event->ignore();
            return;
        }
        PkString message("The MyPaint Brush Engine is not available for this colorspace");
        feedback->showFloatingMessage(message, {}, 4500,
                                      KisCanvasFeedback::Priority::Medium,
                                      Qt::AlignCenter | Qt::TextWordWrap);
        event->ignore();
        return;
    }

    setMode(KisTool::PAINT_MODE);

    const KisToolShape::ShapeAddInfo info =
        shouldAddShape(currentNode());

    // Always show guideline on vector layers
    m_showGuideline = true;
    updatePreviewTimer(m_showGuideline);
    m_helper->setEnabled((nodeAbility == PAINT && !info.shouldAddShape) || info.shouldAddSelectionShape);
    m_helper->setUseSensors(true);
    m_helper->start(event, canvas()->resourceManager());

    m_startPoint = convertToPixelCoordAndSnap(event);
    m_endPoint = m_startPoint;
    m_lastUpdatedPoint = m_startPoint;

    m_strokeIsRunning = true;
    m_altInitiallyHeld = event->modifiers().testFlag(Qt::AltModifier);

    showSize();
}

void KisToolLine::updateStroke()
{
    if (!m_strokeIsRunning) return;

    m_helper->repaintLine(image(),
                          currentNode(),
                          image().data());
}

void KisToolLine::continuePrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    if (!m_strokeIsRunning) return;

    // If the user was holding Alt at the start of the line, we don't want to
    // move the origin around because moving the origin of a zero-length line
    // is silly and this interferes with users coming from PS binding Alt to
    // the quick switch line tool.
    Qt::KeyboardModifiers effectiveModifiers = event->modifiers();
    if (m_altInitiallyHeld){
        if (effectiveModifiers.testFlag(Qt::AltModifier)) {
            // Remove the modifier if it was held at the beginning. The checks
            // in the subsequent code use equality instead of testing the flag,
            // so this retains the expected behavior without much rejigging.
            effectiveModifiers.setFlag(Qt::AltModifier, false);
        } else {
            // User lifted the Alt key, we'll let them re-press it from this
            // point on to move the origin after all.
            m_altInitiallyHeld = false;
        }
    }

    // First ensure the old guideline is deleted
    updateGuideline();

    PkPointF pos = convertToPixelCoordAndSnap(event);

    if (effectiveModifiers == Qt::AltModifier) {
        PkPointF trans = pos - m_endPoint;
        m_helper->translatePoints(trans);
        m_startPoint += trans;
        m_endPoint += trans;
    } else if (effectiveModifiers == Qt::ShiftModifier) {
        pos = straightLine(pos);
        m_helper->addPoint(event, pos);
    } else {
        m_helper->addPoint(event, pos);
        m_helper->movePointsTo(m_startPoint, pos);
    }
    m_endPoint = pos;

    // Draw preview (showPreview panel checkbox removed; its config default was true)
    // If the cursor has moved a significant amount, immediately clear the
    // current preview and redraw. Otherwise, do slow redraws periodically.
    auto updateDistance = (pixelToView(m_lastUpdatedPoint) - pixelToView(pos)).manhattanLength();
    if (updateDistance > 10) {
        m_helper->clearPaint();
        m_longStrokeUpdateCompressor.stop();
        m_strokeUpdateCompressor.start();
        m_lastUpdatedPoint = pos;
    } else if (updateDistance > 1 &&  !m_strokeUpdateCompressor.isActive() && !m_longStrokeUpdateCompressor.isActive()) {
        m_longStrokeUpdateCompressor.start();
        m_lastUpdatedPoint = pos;
    }

    if(effectiveModifiers == Qt::AltModifier) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER_NOOP(feedback);
        if (feedback) {
            feedback->showFloatingMessage(
                PkString("X: %1 px\nY: %2 px")
                    .arg(KisBasicToolsString::numberFixed(m_startPoint.x(), 1))
                    .arg(KisBasicToolsString::numberFixed(m_startPoint.y(), 1)),
                {}, 1000, KisCanvasFeedback::Priority::High,
                Qt::AlignLeft | Qt::TextWordWrap | Qt::AlignVCenter);
        }
    }
    else {
        showSize();
    }

    updateGuideline();
    KisToolPaint::requestUpdateOutline(event->point, event);
}

void KisToolLine::endPrimaryAction(KoPointerEvent *event)
{
    (void)event;
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    setMode(KisTool::HOVER_MODE);

    updateGuideline();
    endStroke();

    if (KisCanvasToolServices *services =
            dynamic_cast<KisCanvasToolServices *>(canvas())) {
        services->toolEndAssistantStroke();
    }
}

bool KisToolLine::primaryActionSupportsHiResEvents() const
{
    return true;
}


void KisToolLine::endStroke()
{
    NodePaintAbility nodeAbility = nodePaintAbility();

    if (!m_strokeIsRunning || m_startPoint == m_endPoint || nodeAbility == UNPAINTABLE) {
        m_helper->clearPoints();
        return;
    }

    const KisToolShape::ShapeAddInfo info =
        shouldAddShape(currentNode());

    if ((nodeAbility == PAINT && !info.shouldAddShape) || info.shouldAddSelectionShape) {
        updateStroke();
        m_helper->end();
    }
    else {
        KisResourcesSnapshot resources(image(),
                                       currentNode(),
                                       canvas()->resourceManager()->canvasResourcesInterface());
        KoPathShape* path = new KoPathShape();
        path->setShapeId(KoPathShapeId);

        PkTransform resolutionMatrix;
        resolutionMatrix.scale(1 / currentImage()->xRes(), 1 / currentImage()->yRes());
        path->moveTo(resolutionMatrix.map(m_startPoint));
        path->lineTo(resolutionMatrix.map(m_endPoint));
        path->normalize();

        KoShapeStrokeSP border(new KoShapeStroke(currentStrokeWidth(), resources.currentFgColor().toQColor()));
        path->setStroke(border);

        KUndo2Command * cmd = canvas()->shapeController()->addShape(path, nullptr);
        canvas()->addCommand(cmd);
    }

    m_strokeIsRunning = false;
    m_endPoint = m_startPoint;
}

void KisToolLine::cancelStroke()
{
    if (!m_strokeIsRunning) return;
    if (m_startPoint == m_endPoint) return;

    /**
     * The actual stroke is run by the timer so it is a legal
     * situation when m_strokeIsRunning is true, but the actual redraw
     * stroke is not running.
     */
    if (m_helper->isRunning()) {
        m_helper->cancel();
    }

    m_strokeIsRunning = false;
    m_endPoint = m_startPoint;
}

PkPointF KisToolLine::straightLine(PkPointF point)
{
    const PkPointF lineVector = point - m_startPoint;
    qreal lineAngle = std::atan2(lineVector.y(), lineVector.x());

    if (lineAngle < 0) {
        lineAngle += 2 * M_PI;
    }

    const qreal ANGLE_BETWEEN_CONSTRAINED_LINES = (2 * M_PI) / 24;

    const quint32 constrainedLineIndex = static_cast<quint32>((lineAngle / ANGLE_BETWEEN_CONSTRAINED_LINES) + 0.5);
    const qreal constrainedLineAngle = constrainedLineIndex * ANGLE_BETWEEN_CONSTRAINED_LINES;

    const qreal lineLength = std::sqrt((lineVector.x() * lineVector.x()) + (lineVector.y() * lineVector.y()));

    const PkPointF constrainedLineVector(lineLength * std::cos(constrainedLineAngle), lineLength * std::sin(constrainedLineAngle));

    const PkPointF result = m_startPoint + constrainedLineVector;

    return result;
}

void KisToolLine::updateGuideline()
{
    if (canvas()) {
        PkRectF bound(m_startPoint, m_endPoint);
        canvas()->updateCanvas(convertToPt(bound.normalized().adjusted(-3, -3, 3, 3)));
    }
}


void KisToolLine::showSize()
{
    KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(feedback);
    feedback->showFloatingMessage(
        PkString("Length: %1 px").arg(
            KisBasicToolsString::numberFixed(PkLineF(m_startPoint, m_endPoint).length(), 1)),
        {}, 1000, KisCanvasFeedback::Priority::High,
        Qt::AlignLeft | Qt::TextWordWrap | Qt::AlignVCenter);
}
void KisToolLine::paintLine(PkPainter& gc, const PkRect&)
{
    PkPointF viewStartPos = pixelToView(m_startPoint);
    PkPointF viewStartEnd = pixelToView(m_endPoint);

    if (m_showGuideline && canvas()) {
        PkPainterPath path;
        path.moveTo(viewStartPos);
        path.lineTo(viewStartEnd);
        paintToolOutline(&gc, path);
    }
}

PkString KisToolLine::quickHelp() const
{
    return PkString("Alt+Drag will move the origin of the currently displayed line around, Shift+Drag will force you to draw straight lines");
}

bool KisToolLine::supportsPaintingAssistants() const
{
    return true;
}
