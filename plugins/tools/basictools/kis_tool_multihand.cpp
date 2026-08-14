/*
 *  SPDX-FileCopyrightText: 2011 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "kis_tool_multihand.h"

#include <QTransform>

#include <KoCanvasBase.h>
#include <KisCanvasFeedback.h>
#include <KisCanvasInvalidation.h>
#include <KisSelectionUtils.h>
#include "kis_selection.h"

#include "kis_tool_multihand_helper.h"

#include <QtGlobal>


static const int MAXIMUM_BRUSHES = 50;

KisToolMultihand::KisToolMultihand(KoCanvasBase *canvas)
    : KisToolBrush(canvas),
      m_transformMode(SYMMETRY),
      m_angle(0),
      m_handsCount(4),
      m_mirrorVertically(false),
      m_mirrorHorizontally(false),
      m_showAxes(false),
      m_translateRadius(0),
      m_setupAxesFlag(false),
      m_addSubbrushesMode(false),
      m_intervalX(0),
      m_intervalY(0)
    , m_randomGenerator(QRandomGenerator::global()->generate())
{


    m_helper =
        new KisToolMultihandHelper(paintingInformationBuilder(),
                                   canvas->resourceManager(),
                                   kundo2_i18n("Multibrush Stroke"));
    resetHelper(m_helper);
    if (image()) {
        m_axesPoint = QPointF(0.5 * image()->width(), 0.5 * image()->height());
        connect(image(), SIGNAL(sigSizeChanged(QPointF,QPointF)), this, SLOT(resetAxes()));
    }

}

KisToolMultihand::~KisToolMultihand()
{
}

void KisToolMultihand::beginPrimaryAction(KoPointerEvent *event)
{
    if(m_setupAxesFlag) {
        setMode(KisTool::OTHER);
        m_axesPoint = convertToPixelCoord(event->point);
        requestUpdateOutline(event->point, 0);
        updateCanvas();
    }
    else if (m_addSubbrushesMode &&  m_transformMode == COPYTRANSLATE){
        QPointF newPoint = convertToPixelCoord(event->point);
        m_subbrOriginalLocations << newPoint;
        requestUpdateOutline(event->point, 0);
        updateCanvas();
    }
    else if (m_transformMode == COPYTRANSLATEINTERVALS) {
        m_axesPoint = convertToPixelCoord(event->point);
        initTransformations();
        KisToolFreehand::beginPrimaryAction(event);
    }
    else {
        initTransformations();
        KisToolFreehand::beginPrimaryAction(event);
    }
}

void KisToolMultihand::continuePrimaryAction(KoPointerEvent *event)
{
    if(mode() == KisTool::OTHER) {
        m_axesPoint = convertToPixelCoord(event->point);
        requestUpdateOutline(event->point, 0);
        updateCanvas();
    }
    else {
        requestUpdateOutline(event->point, 0);
        KisToolFreehand::continuePrimaryAction(event);
    }
}

void KisToolMultihand::endPrimaryAction(KoPointerEvent *event)
{
    if(mode() == KisTool::OTHER) {
        setMode(KisTool::HOVER_MODE);
        requestUpdateOutline(event->point, 0);
        finishAxesSetup();
    }
    else {
        KisToolFreehand::endPrimaryAction(event);
    }
}

void KisToolMultihand::beginAlternateAction(KoPointerEvent* event, AlternateAction action)
{
    if ((action != ChangeSize && action != ChangeSizeSnap) ||
            m_transformMode != COPYTRANSLATE ||
            !m_addSubbrushesMode) {
        KisToolBrush::beginAlternateAction(event, action);
        return;
    }
    setMode(KisTool::OTHER_1);
    m_axesPoint = convertToPixelCoord(event->point);
    requestUpdateOutline(event->point, 0);
    updateCanvas();
}

void KisToolMultihand::continueAlternateAction(KoPointerEvent* event, AlternateAction action)
{
    if ((action != ChangeSize && action != ChangeSizeSnap) ||
            m_transformMode != COPYTRANSLATE ||
            !m_addSubbrushesMode) {
        KisToolBrush::continueAlternateAction(event, action);
        return;
    }
    if (mode() == KisTool::OTHER_1) {
        m_axesPoint = convertToPixelCoord(event->point);
        requestUpdateOutline(event->point, 0);
        updateCanvas();
    }
}

void KisToolMultihand::endAlternateAction(KoPointerEvent* event, AlternateAction action)
{
    if ((action != ChangeSize && action != ChangeSizeSnap) ||
            m_transformMode != COPYTRANSLATE ||
            !m_addSubbrushesMode) {
        KisToolBrush::endAlternateAction(event, action);
        return;
    }
    if (mode() == KisTool::OTHER_1) {
        setMode(KisTool::HOVER_MODE);
    }
}

void KisToolMultihand::mouseMoveEvent(KoPointerEvent* event)
{
    if (mode() == HOVER_MODE) {
        m_lastToolPos=convertToPixelCoord(event->point);
    }
    KisToolBrush::mouseMoveEvent(event);
}

void KisToolMultihand::paint(QPainter& gc, const KoViewConverter &converter)
{
    QPainterPath path;

    if (m_showAxes) {
        const int axisLength = currentImage()->height() + currentImage()->width();

        // add division guide lines if using multiple brushes
        if ((m_handsCount > 1 && m_transformMode == SYMMETRY) ||
            (m_handsCount > 1 && m_transformMode == SNOWFLAKE) ) {
            int axesCount;
            if (m_transformMode == SYMMETRY){
                 axesCount = m_handsCount;
            }
            else {
                axesCount = m_handsCount*2;
            }
            const qreal axesAngle = 360.0 / float(axesCount);
            float currentAngle = 0.0;
            const float startingInsetLength = 20; // don't start each line at the origin so we can see better when all points converge

            // draw lines radiating from the origin
            for( int i=0; i < axesCount; i++) {

                currentAngle = i*axesAngle;

                // convert angles to radians since cos and sin need that
                currentAngle = currentAngle * 0.017453 + m_angle; // m_angle is current rotation set on UI

                const QPoint startingSpot = QPoint(m_axesPoint.x()+ (sin(currentAngle)*startingInsetLength), m_axesPoint.y()- (cos(currentAngle))*startingInsetLength );
                path.moveTo(startingSpot.x(), startingSpot.y());
                QPointF symmetryLinePoint(m_axesPoint.x()+ (sin(currentAngle)*axisLength), m_axesPoint.y()- (cos(currentAngle))*axisLength );
                path.lineTo(symmetryLinePoint);
            }

        }
        else if(m_transformMode == MIRROR) {

            if (m_mirrorHorizontally) {
                path.moveTo(m_axesPoint.x()-axisLength*cos(m_angle+M_PI_2), m_axesPoint.y()-axisLength*sin(m_angle+M_PI_2));
                path.lineTo(m_axesPoint.x()+axisLength*cos(m_angle+M_PI_2), m_axesPoint.y()+axisLength*sin(m_angle+M_PI_2));
            }

            if(m_mirrorVertically) {
                path.moveTo(m_axesPoint.x()-axisLength*cos(m_angle), m_axesPoint.y()-axisLength*sin(m_angle));
                path.lineTo(m_axesPoint.x()+axisLength*cos(m_angle), m_axesPoint.y()+axisLength*sin(m_angle));
            }
        }
        else if (m_transformMode == COPYTRANSLATE) {

            const int ellipsePreviewSize = 10;
            // draw ellipse at origin to emphasize this is a drawing point
            path.addEllipse(m_axesPoint.x()-(ellipsePreviewSize),
                            m_axesPoint.y()-(ellipsePreviewSize),
                            ellipsePreviewSize*2,
                            ellipsePreviewSize*2);

            Q_FOREACH (QPointF dPos, m_subbrOriginalLocations) {
                path.addEllipse(dPos, ellipsePreviewSize, ellipsePreviewSize);  // Show subbrush reference locations while in add mode
            }

            // draw the horiz/vertical line for axis  origin
            path.moveTo(m_axesPoint.x()-axisLength*cos(m_angle), m_axesPoint.y()-axisLength*sin(m_angle));
            path.lineTo(m_axesPoint.x()+axisLength*cos(m_angle), m_axesPoint.y()+axisLength*sin(m_angle));
            path.moveTo(m_axesPoint.x()-axisLength*cos(m_angle+M_PI_2), m_axesPoint.y()-axisLength*sin(m_angle+M_PI_2));
            path.lineTo(m_axesPoint.x()+axisLength*cos(m_angle+M_PI_2), m_axesPoint.y()+axisLength*sin(m_angle+M_PI_2));

        }
        else if (m_transformMode == COPYTRANSLATEINTERVALS) {
            const int ellipsePreviewSize = 10;

            Q_FOREACH (QPointF dPos, intervalLocations()) {
                path.addEllipse(dPos, ellipsePreviewSize, ellipsePreviewSize);
            }
        }
        else {

            // draw the horiz/vertical line for axis  origin
            path.moveTo(m_axesPoint.x()-axisLength*cos(m_angle), m_axesPoint.y()-axisLength*sin(m_angle));
            path.lineTo(m_axesPoint.x()+axisLength*cos(m_angle), m_axesPoint.y()+axisLength*sin(m_angle));
            path.moveTo(m_axesPoint.x()-axisLength*cos(m_angle+M_PI_2), m_axesPoint.y()-axisLength*sin(m_angle+M_PI_2));
            path.lineTo(m_axesPoint.x()+axisLength*cos(m_angle+M_PI_2), m_axesPoint.y()+axisLength*sin(m_angle+M_PI_2));
        }

    } else {

        // not showing axis
        if (m_transformMode == COPYTRANSLATE) {

            Q_FOREACH (QPointF dPos, m_subbrOriginalLocations) {
                // Show subbrush reference locations while in add mode
                if (m_addSubbrushesMode) {
                    path.addEllipse(dPos, 10, 10);
                }
            }
        }
    }

    KisToolFreehand::paint(gc, converter);

    // origin point preview line/s
    gc.save();
    QPen outlinePen;
    outlinePen.setColor(QColor(100,100,100,150));
    outlinePen.setStyle(Qt::PenStyle::SolidLine);
    gc.setPen(outlinePen);
    paintToolOutline(&gc, pixelToView(path));
    gc.restore();


    // fill in a dot for the origin if showing axis
    if (m_showAxes && m_transformMode != COPYTRANSLATEINTERVALS) {
        // draw a dot at the origin point to help with precisely moving
        QPainterPath dotPath;
        const int dotRadius = 4;
        dotPath.moveTo(m_axesPoint.x(), m_axesPoint.y());
        dotPath.addEllipse(m_axesPoint.x()- dotRadius*0.25, m_axesPoint.y()- dotRadius*0.25, dotRadius, dotRadius); // last 2 parameters are dot's size

        QBrush fillBrush;
        fillBrush.setColor(QColor(255, 255, 255, 255));
        fillBrush.setStyle(Qt::SolidPattern);
        gc.fillPath(pixelToView(dotPath), fillBrush);


        // add slight offset circle for contrast to help show it on
        dotPath = QPainterPath(); // resets path
        dotPath.addEllipse(m_axesPoint.x() - dotRadius*0.75, m_axesPoint.y()- dotRadius*0.75, dotRadius, dotRadius); // last 2 parameters are dot's size
        fillBrush.setColor(QColor(120, 120, 120, 255));
        gc.fillPath(pixelToView(dotPath), fillBrush);
    }

}

void KisToolMultihand::initTransformations()
{
    QVector<QTransform> transformations;
    QTransform m;

    if(m_transformMode == SYMMETRY) {
        qreal angle = 0;
        const qreal angleStep = (2 * M_PI) / m_handsCount;

        for(int i = 0; i < m_handsCount; i++) {
            m.translate(m_axesPoint.x(), m_axesPoint.y());
            m.rotateRadians(angle);
            m.translate(-m_axesPoint.x(), -m_axesPoint.y());

            transformations << m;
            m.reset();
            angle += angleStep;
        }
    }
    else if(m_transformMode == MIRROR) {
        transformations << m;

        if (m_mirrorHorizontally) {
            m.translate(m_axesPoint.x(),m_axesPoint.y());
            m.rotateRadians(m_angle);
            m.scale(-1,1);
            m.rotateRadians(-m_angle);
            m.translate(-m_axesPoint.x(), -m_axesPoint.y());
            transformations << m;
            m.reset();
        }

        if (m_mirrorVertically) {
            m.translate(m_axesPoint.x(),m_axesPoint.y());
            m.rotateRadians(m_angle);
            m.scale(1,-1);
            m.rotateRadians(-m_angle);
            m.translate(-m_axesPoint.x(), -m_axesPoint.y());
            transformations << m;
            m.reset();
        }

        if (m_mirrorVertically && m_mirrorHorizontally){
            m.translate(m_axesPoint.x(),m_axesPoint.y());
            m.rotateRadians(m_angle);
            m.scale(-1,-1);
            m.rotateRadians(-m_angle);
            m.translate(-m_axesPoint.x(), -m_axesPoint.y());
            transformations << m;
            m.reset();
        }

    }
    else if(m_transformMode == SNOWFLAKE) {
        qreal angle = 0;
        const qreal angleStep = (2 * M_PI) / m_handsCount/4;

        for(int i = 0; i < m_handsCount*4; i++) {
           if ((i%2)==1) {

               m.translate(m_axesPoint.x(), m_axesPoint.y());
               m.rotateRadians(m_angle-angleStep);
               m.rotateRadians(angle);
               m.scale(-1,1);
               m.rotateRadians(-m_angle+angleStep);
               m.translate(-m_axesPoint.x(), -m_axesPoint.y());

               transformations << m;
               m.reset();
               angle += angleStep*2;
           } else {
               m.translate(m_axesPoint.x(), m_axesPoint.y());
               m.rotateRadians(m_angle-angleStep);
               m.rotateRadians(angle);
               m.rotateRadians(-m_angle+angleStep);
               m.translate(-m_axesPoint.x(), -m_axesPoint.y());

               transformations << m;
               m.reset();
               angle += angleStep*2;
            }
        }
    }
    else if(m_transformMode == TRANSLATE) {
        /**
         * TODO: currently, the seed is the same for all the
         * strokes
         */
        for (int i = 0; i < m_handsCount; i++){
            const qreal angle = m_randomGenerator.bounded(2.0 * M_PI);
            const qreal length = m_randomGenerator.bounded(1.0);

            // convert the Polar coordinates to Cartesian coordinates
            qreal nx = (m_translateRadius * cos(angle) * length);
            qreal ny = (m_translateRadius * sin(angle) * length);

            m.translate(m_axesPoint.x(),m_axesPoint.y());
            m.rotateRadians(m_angle);
            m.translate(nx,ny);
            m.rotateRadians(-m_angle);
            m.translate(-m_axesPoint.x(), -m_axesPoint.y());
            transformations << m;
            m.reset();
        }
    } else if (m_transformMode == COPYTRANSLATE) {
        transformations << m;
        Q_FOREACH (QPointF dPos, m_subbrOriginalLocations) {
            const QPointF resPos = dPos-m_axesPoint; // Calculate the difference between subbrush reference position and "origin" reference
            m.translate(resPos.x(), resPos.y());
            transformations << m;
            m.reset();
        }
    } else if (m_transformMode == COPYTRANSLATEINTERVALS) {
        KisSelectionSP selection =
            KisSelectionUtils::activeSelectionForNode(image(), currentNode());
        const QRect bounds = selection ? selection->selectedExactRect() : image()->bounds();
        const QPoint dPos = bounds.topLeft() +
                      QPoint(m_intervalX ? m_intervalX * floor((m_axesPoint.x() - bounds.left()) / m_intervalX) : 0,
                             m_intervalY ? m_intervalY * floor((m_axesPoint.y() - bounds.top()) / m_intervalY) : 0);

        Q_FOREACH (QPoint pos, intervalLocations()) {
                const QPointF resPos = pos - dPos;
                m.translate(resPos.x(), resPos.y());
                transformations << m;
                m.reset();
        }
    }

    m_helper->setupTransformations(transformations);
}

void KisToolMultihand::resetAxes()
{
    m_axesPoint = QPointF(0.5 * image()->width(), 0.5 * image()->height());
    finishAxesSetup();
}


void KisToolMultihand::finishAxesSetup()
{
    m_setupAxesFlag = false;
    resetCursorStyle();
    updateCanvas();
}

void KisToolMultihand::updateCanvas()
{
    KisCanvasInvalidation *invalidation = dynamic_cast<KisCanvasInvalidation *>(canvas());
    KIS_SAFE_ASSERT_RECOVER_RETURN(invalidation);
    invalidation->invalidateAll();
    if (m_setupAxesFlag)
    {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER_RETURN(feedback);
        feedback->showFloatingMessage(i18n("X: %1 px\nY: %2 px"
                , QString::number(this->m_axesPoint.x(),'f',1),QString::number(this->m_axesPoint.y(),'f',1))
                , QIcon(), 1000, KisCanvasFeedback::Priority::High, Qt::AlignLeft | Qt::TextWordWrap | Qt::AlignVCenter);
    }
}

QVector<QPoint> KisToolMultihand::intervalLocations()
{
    QVector<QPoint> intervalLocations;

    KisSelectionSP selection =
        KisSelectionUtils::activeSelectionForNode(image(), currentNode());
    const QRect bounds = selection ? selection->selectedExactRect() : image()->bounds();

    const int intervals = m_intervalX ? (bounds.width() / m_intervalX) : 0 +
                    m_intervalY ? (bounds.height() / m_intervalY) : 0;
    if (intervals > MAXIMUM_BRUSHES) {
        KisCanvasFeedback *feedback = dynamic_cast<KisCanvasFeedback*>(canvas());
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(feedback, intervalLocations);
        feedback->showFloatingMessage(
            i18n("Multibrush Tool does not support more than %1 brushes; use a larger interval.",
            QString::number(MAXIMUM_BRUSHES)), QIcon(), 4500,
            KisCanvasFeedback::Priority::Medium, Qt::AlignCenter | Qt::TextWordWrap);
        return intervalLocations;
    }

    for (int x = bounds.left(); x <= bounds.right(); x += m_intervalX) {
        for (int y = bounds.top(); y <= bounds.bottom(); y += m_intervalY) {
            intervalLocations << QPoint(x,y);

            if (m_intervalY == 0) { break; }
        }
        if (m_intervalX == 0) { break; }
    }

    return intervalLocations;
}
