/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/qmath.h>

#include "kis_tool_rectangle_base.h"

#include <KoCanvasBase.h>
#include <KoCanvasController.h>
#include <KoPointerEvent.h>
#include <KoViewConverter.h>
#include <KisCanvasToolServices.h>
#include <PkFlakeBridge.h>

KisToolRectangleBase::KisToolRectangleBase(KoCanvasBase * canvas, KisToolRectangleBase::ToolType type, const QCursor & cursor)
    : KisToolShape(canvas, cursor)
    , m_dragStart(0, 0)
    , m_dragEnd(0, 0)
    , m_type(type)
    , m_isRatioForced(false)
    , m_isWidthForced(false)
    , m_isHeightForced(false)
    , m_rotateActive(false)
    , m_forcedRatio(1.0)
    , m_forcedWidth(0)
    , m_forcedHeight(0)
    , m_roundCornersX(0)
    , m_roundCornersY(0)
    , m_referenceAngle(0)
    , m_angle(0)
    , m_angleBuffer(0)
    , m_currentModifiers(Pk::NoModifier)
{
}


void KisToolRectangleBase::constraintsChanged(bool forceRatio, bool forceWidth, bool forceHeight, float ratio, float width, float height)
{
    m_isWidthForced = forceWidth;
    m_isHeightForced = forceHeight;
    m_isRatioForced = forceRatio;

    m_forcedHeight = height;
    m_forcedWidth = width;
    m_forcedRatio = ratio;

    // Avoid division by zero in size calculations
    if (ratio < 0.0001f)
        m_isRatioForced = false;
}

void KisToolRectangleBase::roundCornersChanged(int rx, int ry)
{
    m_roundCornersX = rx;
    m_roundCornersY = ry;
}

void KisToolRectangleBase::showSize()
{
    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices*>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(services);
    const PkRectF rect = createRect(m_dragStart, m_dragEnd);
    services->toolShowRectangleSize(pkRound(rect.width()), pkRound(rect.height()));

}
void KisToolRectangleBase::paint(PkPainter &gc, const KoViewConverter &converter)
{
    if(mode() == KisTool::PAINT_MODE) {
        paintRectangle(gc, createRect(m_dragStart, m_dragEnd));
    }

    KisToolPaint::paint(gc, converter);
}

void KisToolRectangleBase::activate(const PkSet<KoShape *> &shapes)
{
    KisToolShape::activate(shapes);

    sigRequestReloadConfig();
}

void KisToolRectangleBase::deactivate()
{
    cancelStroke();
    KisToolShape::deactivate();
}

void KisToolRectangleBase::pkKeyPressEvent(PkToolKeyEvent *event) {
    const Pk::Key key = event->key() == Pk::Key_Meta &&
            event->modifiers().testFlag(Pk::ShiftModifier)
        ? Pk::Key_Alt : event->key();

    if (key == Pk::Key_Control) {
        m_currentModifiers |= Pk::ControlModifier;
    } else if (key == Pk::Key_Shift) {
        m_currentModifiers |= Pk::ShiftModifier;
    } else if (key == Pk::Key_Alt) {
        m_currentModifiers |= Pk::AltModifier;
    }

    KisToolShape::pkKeyPressEvent(event);
}

void KisToolRectangleBase::pkKeyReleaseEvent(PkToolKeyEvent *event) {
    const Pk::Key key = event->key() == Pk::Key_Meta &&
            event->modifiers().testFlag(Pk::ShiftModifier)
        ? Pk::Key_Alt : event->key();

    if (key == Pk::Key_Control) {
        m_currentModifiers &= ~Pk::ControlModifier;
    } else if (key == Pk::Key_Shift) {
        m_currentModifiers &= ~Pk::ShiftModifier;
    } else if (key == Pk::Key_Alt) {
        m_currentModifiers &= ~Pk::AltModifier;
    }

    KisToolShape::pkKeyReleaseEvent(event);
}

void KisToolRectangleBase::beginPrimaryAction(KoPointerEvent *event)
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
    beginShape();

    m_currentModifiers = Pk::NoModifier;

    PkPointF pos = convertToPixelCoordAndSnap(event, PkPointF(), false);
    m_dragStart = m_dragCenter = pos;
    m_angle = m_angleBuffer = 0;
    m_rotateActive = false;

    PkSizeF area = PkSizeF(0,0);

    applyConstraints(area, false);

    m_dragEnd.setX(m_dragStart.x() + area.width());
    m_dragEnd.setY(m_dragStart.y() + area.height());

    m_dragCenter = PkPointF((m_dragStart.x() + m_dragEnd.x()) / 2,
                           (m_dragStart.y() + m_dragEnd.y()) / 2);
    showSize();
    event->accept();
}

bool KisToolRectangleBase::isFixedSize() {
  if (m_isWidthForced && m_isHeightForced) return true;
  if (m_isRatioForced && (m_isWidthForced || m_isHeightForced)) return true;

  return false;
}

void KisToolRectangleBase::applyConstraints(PkSizeF &area, bool overrideRatio) {
  if (m_isWidthForced) {
    area.setWidth(m_forcedWidth);
  }
  if (m_isHeightForced) {
    area.setHeight(m_forcedHeight);
  }

  if (m_isHeightForced && m_isWidthForced) return;

  if (m_isRatioForced || overrideRatio) {
    float ratio = m_isRatioForced ? m_forcedRatio : 1.0f;

    if (m_isWidthForced) {
      area.setHeight(area.width() / ratio);
    } else {
      area.setWidth(area.height() * ratio);
    }
  }
}

void KisToolRectangleBase::continuePrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    bool constraintToggle = m_currentModifiers & Pk::ShiftModifier;
    bool translateMode = m_currentModifiers & Pk::AltModifier;
    bool expandFromCenter = m_currentModifiers & Pk::ControlModifier;

    bool rotateMode = expandFromCenter && translateMode;
    bool fixedSize = isFixedSize() && !constraintToggle;

    PkPointF pos = convertToPixelCoordAndSnap(event, PkPointF(), false);

    if (rotateMode) {
        PkPointF angleVector;
        if (!m_rotateActive) {
            m_rotateActive = true;
            angleVector = (fixedSize)? m_dragEnd: pos;
            angleVector -= m_dragStart;
            m_referenceAngle = atan2(angleVector.y(), angleVector.x());
        }
        angleVector = pos - m_dragStart;
        qreal a2 = atan2(angleVector.y(), angleVector.x());
        m_angleBuffer = a2 - m_referenceAngle;
    } else {
        m_rotateActive = false;
        m_angle += m_angleBuffer;
        m_angleBuffer = 0;
    }

    if (fixedSize && !rotateMode) {
      m_dragStart = pos;
    } else if (translateMode && !rotateMode) {
      PkPointF trans = pos - m_dragEnd;
      m_dragStart += trans;
      m_dragEnd += trans;

    }

    PkPointF diag = pos - m_dragStart;
    PkTransform t1, t2;
    t1.rotateRadians(-getRotationAngle());
    PkPointF baseDiag = t1.map(diag);
    PkSizeF area = PkSizeF(fabs(baseDiag.x()), fabs(baseDiag.y()));

    bool overrideRatio = constraintToggle && !(m_isHeightForced || m_isWidthForced || m_isRatioForced);
    if (!constraintToggle || overrideRatio) {
      applyConstraints(area, overrideRatio);
    }

    baseDiag = PkPointF(
      (baseDiag.x() < 0) ? -area.width() : area.width(),
      (baseDiag.y() < 0) ? -area.height() : area.height()
    );

    t2.rotateRadians(getRotationAngle());
    diag = t2.map(baseDiag);

    // resize around center point?
    if (expandFromCenter && !fixedSize && !rotateMode) {
      m_dragStart = m_dragCenter - diag / 2;
      m_dragEnd = m_dragCenter + diag / 2;
    } else {
      m_dragEnd = m_dragStart + diag;
    }

    if(!translateMode) {
        showSize();
    }
    else {
        KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices*>(canvas());
        KIS_ASSERT(services);
        services->toolShowRectanglePosition(m_dragStart.x(), m_dragStart.y());
    }
    updateArea();
    m_dragCenter = PkPointF((m_dragStart.x() + m_dragEnd.x()) / 2,
                           (m_dragStart.y() + m_dragEnd.y()) / 2);

    KisToolPaint::requestUpdateOutline(event->point, event);
}

void KisToolRectangleBase::endPrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);
    // If the event was not originated by the user releasing the button
    // (for example due to the canvas loosing focus), then we just cancel the
    // operation. This prevents some issues with shapes being added after
    // the image was closed while the shape was being made
    if (event->spontaneous()) {
        endStroke();
    } else {
        cancelStroke();
    }
    event->accept();
}

void KisToolRectangleBase::requestStrokeEnd()
{
    if (mode() != KisTool::PAINT_MODE) {
        return;
    }
    endStroke();
}

void KisToolRectangleBase::requestStrokeCancellation()
{
    if (mode() != KisTool::PAINT_MODE) {
        return;
    }
    cancelStroke();
}

void KisToolRectangleBase::endStroke()
{
    setMode(KisTool::HOVER_MODE);
    updateArea();
    finishRect(createRect(m_dragStart, m_dragEnd), m_roundCornersX, m_roundCornersY);
    endShape();
}

void KisToolRectangleBase::cancelStroke()
{
    setMode(KisTool::HOVER_MODE);
    updateArea();
    endShape();
}

PkRectF KisToolRectangleBase::createRect(const PkPointF &start, const PkPointF &end)
{
    PkTransform t;
    t.translate(start.x(), start.y());
    t.rotateRadians(-getRotationAngle());
    t.translate(-start.x(), -start.y());
    const PkTransform tInv = t.inverted();

    const PkPointF end1 = t.map(end);
    const PkPointF newStart(pkRound(start.x()), pkRound(start.y()));
    const PkPointF newEnd(pkRound(end1.x()), pkRound(end1.y()));
    const PkPointF newCenter = (newStart + newEnd) / 2.0;
   
    PkRectF result(newStart, newEnd);
    result.moveCenter(tInv.map(newCenter));

    return result.normalized();
}

bool KisToolRectangleBase::showRoundCornersGUI() const
{
    return true;
}

void KisToolRectangleBase::paintRectangle(PkPainter &gc, const PkRectF &imageRect)
{
    KIS_ASSERT_RECOVER_RETURN(canvas());

    const PkRect viewRect = pixelToView(imageRect).toAlignedRect();

    KisCanvasToolServices *services = dynamic_cast<KisCanvasToolServices*>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(services);

    const qreal roundCornersX = services->toolCoordinateEffectiveZoom() * m_roundCornersX;
    const qreal roundCornersY = services->toolCoordinateEffectiveZoom() * m_roundCornersY;

    PkPainterPath path;
    if (m_roundCornersX > 0 || m_roundCornersY > 0) {
        path.addRoundedRect(PkRectF(viewRect),
                            roundCornersX, roundCornersY);
    } else {
        path.addRect(PkRectF(viewRect));
    }

    getRotatedPath(path, PkPointF(viewRect.center()), getRotationAngle());
    path.addPath(drawX(pixelToView(m_dragStart)));
    path.addPath(drawX(pixelToView(m_dragCenter)));
    paintToolOutline(&gc, KisOptimizedBrushOutline(path));
}

void KisToolRectangleBase::updateArea() {
    const PkRectF bound = createRect(m_dragStart, m_dragEnd);

    canvas()->updateCanvas(convertToPt(bound).adjusted(-100, -100, +200, +200));

    rectangleChanged(bound);
}

qreal KisToolRectangleBase::getRotationAngle() {
    return m_angle + m_angleBuffer;
}

PkPainterPath KisToolRectangleBase::drawX(const PkPointF &pt) {
    PkPainterPath path;
    path.moveTo(PkPointF(pt.x() - 5.0, pt.y() - 5.0)); path.lineTo(PkPointF(pt.x() + 5.0, pt.y() + 5.0));
    path.moveTo(PkPointF(pt.x() - 5.0, pt.y() + 5.0)); path.lineTo(PkPointF(pt.x() + 5.0, pt.y() - 5.0));
    return path;
}

void KisToolRectangleBase::getRotatedPath(PkPainterPath &path, const PkPointF &center, const qreal &angle) {
    PkTransform t;
    t.translate(center.x(), center.y());
    t.rotateRadians(angle);
    t.translate(-center.x(), -center.y());

    path = t.map(path);
}
