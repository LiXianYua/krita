/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Fela Winkelmolen <fela.kde@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KarbonCalligraphyTool.h"
#include "KarbonCalligraphicShape.h"

#include <KoPathShape.h>
#include <KoShapeGroup.h>
#include <KoPointerEvent.h>
#include <KoPathPoint.h>
#include <KoCanvasBase.h>
#include <KoShapeController.h>
#include <KoShapeManager.h>
#include <KoSelectedShapesProxy.h>
#include <KoSelection.h>
#include <KoCurveFit.h>
#include <KoColorBackground.h>
#include <KoCanvasResourceProvider.h>
#include <KoColor.h>
#include <KoViewConverter.h>
#include <KisPopupWidgetInterface.h>

#include <PkPainter.h>

#include <cmath>

#undef M_PI
const qreal M_PI = 3.1415927;
using std::pow;
using std::sqrt;

// default calligraphic pen parameters, formerly loaded from the "Mouse"
// profile of the (now removed) option widget's karboncalligraphyrc config
KarbonCalligraphyTool::KarbonCalligraphyTool(KoCanvasBase *canvas)
    : KoToolBase(canvas)
    , m_shape(0)
    , m_usePath(false)
    , m_usePressure(false)
    , m_useAngle(false)
    , m_strokeWidth(30.0)
    , m_customAngle(30)
    , m_angle(0)
    , m_fixation(1.0)
    , m_thinning(0.2)
    , m_caps(0.0)
    , m_mass(3.0 * 3.0 + 1.0) // matches the former setMass(3.0) conversion
    , m_drag(0.7)
    , m_selectedPath(0)
    , m_isDrawing(false)
    , m_speed(0, 0)
{
    PkObject::connect(canvas->selectedShapesProxy(), &KoSelectedShapesProxy::selectionChanged,
                      this, &KarbonCalligraphyTool::updateSelectedPath);

    updateSelectedPath();
}

KarbonCalligraphyTool::~KarbonCalligraphyTool()
{
}

void KarbonCalligraphyTool::paint(PkPainter &painter, const KoViewConverter &converter)
{
    if (m_selectedPath) {
        painter.save();
        painter.setRenderHints(PkPainter::Antialiasing, false);
        painter.setPen(Qt::red);   // TODO make configurable
        PkRectF rect = m_selectedPath->boundingRect();
        PkPointF p1 = converter.documentToView(rect.topLeft());
        PkPointF p2 = converter.documentToView(rect.bottomRight());
        painter.drawRect(PkRectF(p1, p2));
        painter.restore();
    }

    if (!m_shape) {
        return;
    }

    painter.save();

    painter.setTransform(m_shape->absoluteTransformation() *
                         converter.documentToView() *
                         painter.transform());

    m_shape->paint(painter);

    painter.restore();
}

void KarbonCalligraphyTool::mousePressEvent(KoPointerEvent *event)
{
    if (m_isDrawing) {
        return;
    }

    m_lastPoint = event->point;
    m_speed = PkPointF(0, 0);

    m_isDrawing = true;
    m_pointCount = 0;
    m_shape = new KarbonCalligraphicShape(m_caps);
    m_shape->setBackground(PkSharedPointer<KoShapeBackground>(new KoColorBackground(canvas()->resourceManager()->foregroundColor().toQColor())));
    //addPoint( event );
}

void KarbonCalligraphyTool::mouseMoveEvent(KoPointerEvent *event)
{
    if (!m_isDrawing) {
        return;
    }

    addPoint(event);
}

void KarbonCalligraphyTool::mouseReleaseEvent(KoPointerEvent *event)
{
    if (!m_isDrawing) {
        return;
    }

    if (m_pointCount == 0) {
        // handle click: select shape (if any)
        if (event->point == m_lastPoint) {
            KoShapeManager *shapeManager = canvas()->shapeManager();
            KoShape *selectedShape = shapeManager->shapeAt(event->point);
            if (selectedShape != 0) {
                shapeManager->selection()->deselectAll();
                shapeManager->selection()->select(selectedShape);
            }
        }

        delete m_shape;
        m_shape = 0;
        m_isDrawing = false;
        return;
    } else {
        m_endOfPath = false;    // allow last point being added
        addPoint(event);        // add last point
        m_isDrawing = false;
    }

    m_shape->simplifyGuidePath();

    KUndo2Command *cmd = canvas()->shapeController()->addShape(m_shape, 0);
    if (cmd) {
        canvas()->addCommand(cmd);
        canvas()->updateCanvas(m_shape->boundingRect());
    } else {
        // don't leak shape when command could not be created
        delete m_shape;
    }

    m_shape = 0;
}

void KarbonCalligraphyTool::addPoint(KoPointerEvent *event)
{
    if (m_pointCount == 0) {
        if (m_usePath && m_selectedPath) {
            m_selectedPathOutline = m_selectedPath->absoluteTransformation().map(m_selectedPath->outline());
        }
        m_pointCount = 1;
        m_endOfPath = false;
        m_followPathPosition = 0;
        m_lastMousePos = event->point;
        m_lastPoint = calculateNewPoint(event->point, &m_speed);
        m_deviceSupportsTilt = (event->xTilt() != 0 || event->yTilt() != 0);
        return;
    }

    if (m_endOfPath) {
        return;
    }

    ++m_pointCount;

    setAngle(event);

    PkPointF newSpeed;
    PkPointF newPoint = calculateNewPoint(event->point, &newSpeed);
    qreal width = calculateWidth(event->pressure());
    qreal angle = calculateAngle(m_speed, newSpeed);

    // add the previous point
    m_shape->appendPoint(m_lastPoint, angle, width);

    m_speed = newSpeed;
    m_lastPoint = newPoint;
    canvas()->updateCanvas(m_shape->lastPieceBoundingRect());

    if (m_usePath && m_selectedPath) {
        m_speed = PkPointF(0, 0);    // following path
    }
}

void KarbonCalligraphyTool::setAngle(KoPointerEvent *event)
{
    if (!m_useAngle) {
        m_angle = (360.0 - m_customAngle + 90.0) / 180.0 * M_PI;
        return;
    }

    // setting m_angle to the angle of the device
    if (event->xTilt() != 0 || event->yTilt() != 0) {
        m_deviceSupportsTilt = true;
    }

    if (m_deviceSupportsTilt) {
        if (event->xTilt() == 0 && event->yTilt() == 0) {
            return;    // leave as is
        }
        if (event->x() == 0) {
            m_angle = M_PI / 2.0;
            return;
        }

        // y is inverted in qt painting
        m_angle = std::atan(static_cast<double>(-event->yTilt()) / static_cast<double>(event->xTilt())) + M_PI / 2.0;
    } else {
        m_angle = event->rotation() + M_PI / 2.0;
    }
}

PkPointF KarbonCalligraphyTool::calculateNewPoint(const PkPointF &mousePos, PkPointF *speed)
{
    if (!m_usePath || !m_selectedPath) { // don't follow path
        PkPointF force = mousePos - m_lastPoint;
        PkPointF dSpeed = force / m_mass;
        *speed = m_speed * (1.0 - m_drag) + dSpeed;
        return m_lastPoint + *speed;
    }

    PkPointF sp = mousePos - m_lastMousePos;
    m_lastMousePos = mousePos;

    // follow selected path
    qreal step = PkLineF(PkPointF(0, 0), sp).length();
    m_followPathPosition += step;

    qreal t;
    if (m_followPathPosition >= m_selectedPathOutline.length()) {
        t = 1.0;
        m_endOfPath = true;
    } else {
        t = m_selectedPathOutline.percentAtLength(m_followPathPosition);
    }

    PkPointF res = m_selectedPathOutline.pointAtPercent(t);
    *speed = res - m_lastPoint;
    return res;
}

qreal KarbonCalligraphyTool::calculateWidth(qreal pressure)
{
    // calculate the modulo of the speed
    qreal speed = std::sqrt(pow(m_speed.x(), 2) + pow(m_speed.y(), 2));
    qreal thinning =  m_thinning * (speed + 1) / 10.0; // can be negative

    if (thinning > 1) {
        thinning = 1;
    }

    if (!m_usePressure) {
        pressure = 1.0;
    }

    qreal strokeWidth = m_strokeWidth * pressure * (1 - thinning);

    const qreal MINIMUM_STROKE_WIDTH = 1.0;
    if (strokeWidth < MINIMUM_STROKE_WIDTH) {
        strokeWidth = MINIMUM_STROKE_WIDTH;
    }

    return strokeWidth;
}

qreal KarbonCalligraphyTool::calculateAngle(const PkPointF &oldSpeed, const PkPointF &newSpeed)
{
    // calculate the average of the speed (sum of the normalized values)
    qreal oldLength = PkLineF(PkPointF(0, 0), oldSpeed).length();
    qreal newLength = PkLineF(PkPointF(0, 0), newSpeed).length();
    PkPointF oldSpeedNorm = !qFuzzyCompare(oldLength + 1, 1) ?
                oldSpeed / oldLength : PkPointF(0, 0);
    PkPointF newSpeedNorm = !qFuzzyCompare(newLength + 1, 1) ?
                newSpeed / newLength : PkPointF(0, 0);
    PkPointF speed = oldSpeedNorm + newSpeedNorm;

    // angle solely based on the speed
    qreal speedAngle = 0;
    if (speed.x() != 0) { // avoid division by zero
        speedAngle = std::atan(speed.y() / speed.x());
    } else if (speed.y() > 0) {
        // x == 0 && y != 0
        speedAngle = M_PI / 2;
    } else if (speed.y() < 0) {
        // x == 0 && y != 0
        speedAngle = -M_PI / 2;
    }
    if (speed.x() < 0) {
        speedAngle += M_PI;
    }

    // move 90 degrees
    speedAngle += M_PI / 2;

    qreal fixedAngle = m_angle;
    // check if the fixed angle needs to be flipped
    qreal diff = fixedAngle - speedAngle;
    while (diff >= M_PI) { // normalize diff between -180 and 180
        diff -= 2 * M_PI;
    }
    while (diff < -M_PI) {
        diff += 2 * M_PI;
    }

    if (std::abs(diff) > M_PI / 2) { // if absolute value < 90
        fixedAngle += M_PI;    // += 180
    }

    qreal dAngle = speedAngle - fixedAngle;

    // normalize dAngle between -90 and +90
    while (dAngle >= M_PI / 2) {
        dAngle -= M_PI;
    }
    while (dAngle < -M_PI / 2) {
        dAngle += M_PI;
    }

    qreal angle = fixedAngle + dAngle * (1.0 - m_fixation);

    return angle;
}

void KarbonCalligraphyTool::activate(const PkSet<KoShape*> &shapes)
{
    KoToolBase::activate(shapes);

    useCursor(Qt::CrossCursor);
}

void KarbonCalligraphyTool::deactivate()
{
    KoToolBase::deactivate();
}

KisPopupWidgetInterface *KarbonCalligraphyTool::popupWidget()
{
    return nullptr;
}

void KarbonCalligraphyTool::updateSelectedPath()
{
    KoSelection *selection = canvas()->shapeManager()->selection();
    if (selection) {
        // null pointer if it the selection isn't a KoPathShape
        // or if the selection is empty
        m_selectedPath =
                dynamic_cast<KoPathShape *>(selection->firstSelectedShape());

        // or if it's a KoPathShape but with no or more than one subpaths
        if (m_selectedPath && m_selectedPath->subpathCount() != 1) {
            m_selectedPath = 0;
        }

        // or if there ora none or more than 1 shapes selected
        if (selection->count() != 1) {
            m_selectedPath = 0;
        }
    }
}
