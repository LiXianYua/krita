/*
 *
 *  SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_tool_measure.h"

#include <math.h>

#include <QPainter>
#include <QPainterPath>

#include <KoUnit.h>

#include <kis_debug.h>
#include <klocalizedstring.h>

#include "kis_algebra_2d.h"
#include "kis_image.h"
#include "kis_cursor.h"
#include "KoPointerEvent.h"
#include "KoCanvasBase.h"
#include <KoViewConverter.h>
#include "krita_utils.h"
#include <KisCanvasFeedback.h>
#include <KisOptimizedBrushOutline.h>

#define INNER_RADIUS 50

KisToolMeasure::KisToolMeasure(KoCanvasBase * canvas)
    : KisTool(canvas, KisCursor::crossCursor())
{
}

KisToolMeasure::~KisToolMeasure()
{
}
QPointF KisToolMeasure::lockedAngle(QPointF pos)
{
    const QPointF lineVector = pos - m_startPos;
    qreal lineAngle = normalizeAngle(std::atan2(lineVector.y(), lineVector.x()));

    const qreal ANGLE_BETWEEN_CONSTRAINED_LINES = (2 * M_PI) / 24;

    const quint32 constrainedLineIndex = static_cast<quint32>((lineAngle / ANGLE_BETWEEN_CONSTRAINED_LINES) + 0.5);
    const qreal constrainedLineAngle = constrainedLineIndex * ANGLE_BETWEEN_CONSTRAINED_LINES;

    const qreal lineLength = KisAlgebra2D::norm(lineVector);

    const QPointF constrainedLineVector(lineLength * std::cos(constrainedLineAngle), lineLength * std::sin(constrainedLineAngle));

    const QPointF result = m_startPos + constrainedLineVector;

    return result;
}

void KisToolMeasure::paint(QPainter& gc, const KoViewConverter &converter)
{
    QPen old = gc.pen();
    QPen pen(Qt::SolidLine);
    gc.setPen(pen);

    QPainterPath elbowPath;
    elbowPath.moveTo(m_endPos);
    elbowPath.lineTo(m_startPos);

    QPointF offset = (m_baseLineVec * INNER_RADIUS).toPoint();
    QPointF diff = m_endPos-m_startPos;

    bool switch_elbow = QPointF::dotProduct(diff, offset) > 0.0;
    if(switch_elbow) {
        elbowPath.lineTo(m_startPos + offset);
    } else {
        elbowPath.lineTo(m_startPos - offset);
    }

    if (distance() >= INNER_RADIUS) {
        QRectF rectangle(m_startPos.x() - INNER_RADIUS, m_startPos.y() - INNER_RADIUS, 2*INNER_RADIUS, 2*INNER_RADIUS);
        
        double det = diff.x() * m_baseLineVec.y() - diff.y() * m_baseLineVec.x();
        int startAngle = -atan2(m_baseLineVec.y(), m_baseLineVec.x()) / (2*M_PI) * 360;
        int spanAngle = switch_elbow ? -angle() : angle();

        if(!switch_elbow) {
            startAngle+=180;
            startAngle%=360;
        }

        if(det > 0) {
            spanAngle = -spanAngle;
        }

        elbowPath.arcTo(rectangle, startAngle, spanAngle);
    }

    // The opengl renderer doesn't take the QPainter's transform, so the path is scaled here
    qreal sx, sy;
    converter.zoom(&sx, &sy);
    QTransform transf;
    transf.scale(sx / currentImage()->xRes(), sy / currentImage()->yRes());
    paintToolOutline(&gc, transf.map(elbowPath));

    gc.setPen(old);
}
void KisToolMeasure::showDistanceAngleOnCanvas()
{
    KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(feedback);
    QString message = i18nc("%1=distance %2=unit of distance %3=angle in degrees", "%1 %2\n%3°",
                            KritaUtils::prettyFormatReal(distance() / currentImage()->xRes()),
                            KoUnit(KoUnit::Pixel).symbol(),
                            QString::number(angle(),'f',1));
    feedback->showFloatingMessage(message, QIcon(), 2000, KisCanvasFeedback::Priority::High);
}
void KisToolMeasure::beginPrimaryAction(KoPointerEvent *event)
{
    setMode(KisTool::PAINT_MODE);

    // Erase old temporary lines
    canvas()->updateCanvas(convertToPt(boundingRect()));

    m_startPos = convertToPixelCoord(event);
    m_endPos = m_startPos;
    m_baseLineVec = QVector2D(1.0f, 0.0f);
}

void KisToolMeasure::continuePrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    // Erase old temporary lines
    canvas()->updateCanvas(convertToPt(boundingRect()));

    QPointF pos = convertToPixelCoord(event);

    if (event->modifiers() & Qt::AltModifier) {
        QPointF trans = pos - m_endPos;
        m_startPos += trans;
        m_endPos += trans;
    } else if(event->modifiers() & Qt::ShiftModifier){
        m_endPos = lockedAngle(pos);
    } else {
        m_endPos = pos;
    }

    if(!(event->modifiers() & Qt::ControlModifier)) {
        m_chooseBaseLineVec = false;
    } else if(!m_chooseBaseLineVec) {
        m_chooseBaseLineVec = true;
        m_baseLineVec = QVector2D(m_endPos-m_startPos).normalized();
    }

    canvas()->updateCanvas(convertToPt(boundingRect()));
    showDistanceAngleOnCanvas();
}

void KisToolMeasure::endPrimaryAction(KoPointerEvent *event)
{
    CHECK_MODE_SANITY_OR_RETURN(KisTool::PAINT_MODE);

    Q_UNUSED(event);
    setMode(KisTool::HOVER_MODE);
}

double KisToolMeasure::angle()
{
    double dot = QVector2D::dotProduct(QVector2D(m_endPos-m_startPos).normalized(), m_baseLineVec);
    return acos(qAbs(dot)) / (2*M_PI)*360;
}

double KisToolMeasure::distance()
{
    return QVector2D(m_endPos - m_startPos).length();
}

QRectF KisToolMeasure::boundingRect()
{
    QRectF bound;
    bound.setTopLeft(m_startPos);
    bound.setBottomRight(m_endPos);
    bound = bound.united(QRectF(m_startPos.x() - INNER_RADIUS, m_startPos.y() - INNER_RADIUS, 2 * INNER_RADIUS, 2 * INNER_RADIUS));
    return bound.normalized();
}
