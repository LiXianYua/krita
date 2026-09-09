/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Fela Winkelmolen <fela.kde@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KarbonCalligraphyTool.h"
#include "KarbonCalligraphicShape.h"

#include <KoPathShape.h>
#include <KoColorBackground.h>

#include <PkPainter.h>

#include <cmath>

#undef M_PI
const double M_PI = 3.1415927;
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
    watchSelectedShapesChanged([this] { updateSelectedPath(); });

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
        painter.setPen(PkColor(Pk::red));   // TODO make configurable
        painter.drawRect(documentRectToView(converter, m_selectedPath->boundingRect()));
        painter.restore();
    }

    if (!m_shape) {
        return;
    }

    painter.save();

    painter.setTransform(m_shape->absoluteTransformation() *
                         documentToViewTransform(converter) *
                         painter.transform());

    const auto *background = dynamic_cast<const KoColorBackground *>(m_shape->background().data());
    if (background) {
        painter.fillPath(m_shape->outline(), PkBrush(background->color()));
    }

    painter.restore();
}

void KarbonCalligraphyTool::mousePressEvent(KoPointerEvent *event)
{
    if (m_isDrawing) {
        return;
    }

    const PkToolPointerEventData input = pointerEventData(event);
    m_lastPoint = input.point;
    m_speed = PkPointF(0, 0);

    m_isDrawing = true;
    m_pointCount = 0;
    m_shape = new KarbonCalligraphicShape(m_caps);
    m_shape->setBackground(PkSharedPointer<KoShapeBackground>(new KoColorBackground(canvasForegroundColor())));
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
        if (pointerEventData(event).point == m_lastPoint) {
            selectShapeAt(m_lastPoint);
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

    if (!addShapeToCanvas(m_shape)) {
        // don't leak shape when command could not be created
        delete m_shape;
    }

    m_shape = 0;
}

void KarbonCalligraphyTool::addPoint(KoPointerEvent *event)
{
    const PkToolPointerEventData input = pointerEventData(event);
    if (m_pointCount == 0) {
        if (m_usePath && m_selectedPath) {
            m_selectedPathOutline = m_selectedPath->absoluteTransformation().map(m_selectedPath->outline());
        }
        m_pointCount = 1;
        m_endOfPath = false;
        m_followPathPosition = 0;
        m_lastMousePos = input.point;
        m_lastPoint = calculateNewPoint(input.point, &m_speed);
        m_deviceSupportsTilt = (input.xTilt != 0 || input.yTilt != 0);
        return;
    }

    if (m_endOfPath) {
        return;
    }

    ++m_pointCount;

    setAngle(event);

    PkPointF newSpeed;
    PkPointF newPoint = calculateNewPoint(input.point, &newSpeed);
    double width = calculateWidth(input.pressure);
    double angle = calculateAngle(m_speed, newSpeed);

    // add the previous point
    m_shape->appendPoint(m_lastPoint, angle, width);

    m_speed = newSpeed;
    m_lastPoint = newPoint;
    requestCanvasUpdate(m_shape->lastPieceBoundingRect());

    if (m_usePath && m_selectedPath) {
        m_speed = PkPointF(0, 0);    // following path
    }
}

void KarbonCalligraphyTool::setAngle(KoPointerEvent *event)
{
    const PkToolPointerEventData input = pointerEventData(event);
    if (!m_useAngle) {
        m_angle = (360.0 - m_customAngle + 90.0) / 180.0 * M_PI;
        return;
    }

    // setting m_angle to the angle of the device
    if (input.xTilt != 0 || input.yTilt != 0) {
        m_deviceSupportsTilt = true;
    }

    if (m_deviceSupportsTilt) {
        if (input.xTilt == 0 && input.yTilt == 0) {
            return;    // leave as is
        }
        if (input.widgetX == 0) {
            m_angle = M_PI / 2.0;
            return;
        }

        // y is inverted in qt painting
        m_angle = std::atan(static_cast<double>(-input.yTilt) / static_cast<double>(input.xTilt)) + M_PI / 2.0;
    } else {
        m_angle = input.rotation + M_PI / 2.0;
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
    double step = PkLineF(PkPointF(0, 0), sp).length();
    m_followPathPosition += step;

    double t;
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

double KarbonCalligraphyTool::calculateWidth(double pressure)
{
    // calculate the modulo of the speed
    double speed = std::sqrt(pow(m_speed.x(), 2) + pow(m_speed.y(), 2));
    double thinning =  m_thinning * (speed + 1) / 10.0; // can be negative

    if (thinning > 1) {
        thinning = 1;
    }

    if (!m_usePressure) {
        pressure = 1.0;
    }

    double strokeWidth = m_strokeWidth * pressure * (1 - thinning);

    const double MINIMUM_STROKE_WIDTH = 1.0;
    if (strokeWidth < MINIMUM_STROKE_WIDTH) {
        strokeWidth = MINIMUM_STROKE_WIDTH;
    }

    return strokeWidth;
}

double KarbonCalligraphyTool::calculateAngle(const PkPointF &oldSpeed, const PkPointF &newSpeed)
{
    // calculate the average of the speed (sum of the normalized values)
    double oldLength = PkLineF(PkPointF(0, 0), oldSpeed).length();
    double newLength = PkLineF(PkPointF(0, 0), newSpeed).length();
    PkPointF oldSpeedNorm = !pkQtFuzzyCompare(oldLength + 1, 1) ?
                oldSpeed / oldLength : PkPointF(0, 0);
    PkPointF newSpeedNorm = !pkQtFuzzyCompare(newLength + 1, 1) ?
                newSpeed / newLength : PkPointF(0, 0);
    PkPointF speed = oldSpeedNorm + newSpeedNorm;

    // angle solely based on the speed
    double speedAngle = 0;
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

    double fixedAngle = m_angle;
    // check if the fixed angle needs to be flipped
    double diff = fixedAngle - speedAngle;
    while (diff >= M_PI) { // normalize diff between -180 and 180
        diff -= 2 * M_PI;
    }
    while (diff < -M_PI) {
        diff += 2 * M_PI;
    }

    if (std::abs(diff) > M_PI / 2) { // if absolute value < 90
        fixedAngle += M_PI;    // += 180
    }

    double dAngle = speedAngle - fixedAngle;

    // normalize dAngle between -90 and +90
    while (dAngle >= M_PI / 2) {
        dAngle -= M_PI;
    }
    while (dAngle < -M_PI / 2) {
        dAngle += M_PI;
    }

    double angle = fixedAngle + dAngle * (1.0 - m_fixation);

    return angle;
}

void KarbonCalligraphyTool::activate(const PkSet<KoShape*> &shapes)
{
    KoToolBase::activate(shapes);

    useCursor(Pk::CrossCursor);
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
    const PkToolSelectedShapes selection = selectedShapes();
    if (selection.first) {
        // null pointer if it the selection isn't a KoPathShape
        // or if the selection is empty
        m_selectedPath =
                dynamic_cast<KoPathShape *>(selection.first);

        // or if it's a KoPathShape but with no or more than one subpaths
        if (m_selectedPath && m_selectedPath->subpathCount() != 1) {
            m_selectedPath = 0;
        }

        // or if there ora none or more than 1 shapes selected
        if (selection.count != 1) {
            m_selectedPath = 0;
        }
    } else {
        m_selectedPath = nullptr;
    }
}
