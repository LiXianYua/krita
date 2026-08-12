/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Fela Winkelmolen <fela.kde@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KARBONCALLIGRAPHYTOOL_H
#define KARBONCALLIGRAPHYTOOL_H

#include <KoToolBase.h>
#include <KoPathShape.h>
#include <QPainterPath>

class KoPathShape;
class KarbonCalligraphicShape;

class KarbonCalligraphyTool : public KoToolBase
{
    Q_OBJECT
public:
    explicit KarbonCalligraphyTool(KoCanvasBase *canvas);
    ~KarbonCalligraphyTool() override;

    void paint(QPainter &painter, const KoViewConverter &converter) override;

    void mousePressEvent(KoPointerEvent *event) override;
    void mouseMoveEvent(KoPointerEvent *event) override;
    void mouseReleaseEvent(KoPointerEvent *event) override;

    KisPopupWidgetInterface *popupWidget() override;

    void activate(const QSet<KoShape *> &shapes) override;
    void deactivate() override;

private Q_SLOTS:
    void updateSelectedPath();

private:
    void addPoint(KoPointerEvent *event);
    // auxiliary function that sets m_angle
    void setAngle(KoPointerEvent *event);
    // auxiliary functions to calculate the dynamic parameters
    // returns the new point and sets speed to the speed
    QPointF calculateNewPoint(const QPointF &mousePos, QPointF *speed);
    qreal calculateWidth(qreal pressure);
    qreal calculateAngle(const QPointF &oldSpeed, const QPointF &newSpeed);

    QPointF m_lastPoint;
    KarbonCalligraphicShape *m_shape;

    // used to determine if the device supports tilt
    bool m_deviceSupportsTilt;

    bool m_usePath;         // follow selected path
    bool m_usePressure;     // use tablet pressure
    bool m_useAngle;        // use tablet angle
    qreal m_strokeWidth;
    qreal m_lastWidth;
    qreal m_customAngle;   // angle set by the user
    qreal m_angle;  // angle to use, may use the device angle, in radians!!!
    qreal m_fixation;
    qreal m_thinning;
    qreal m_caps;
    qreal m_mass;  // in raw format (not user friendly)
    qreal m_drag;  // from 0.0 to 1.0

    KoPathShape *m_selectedPath;
    QPainterPath m_selectedPathOutline;
    qreal m_followPathPosition;
    bool m_endOfPath;
    QPointF m_lastMousePos;

    bool m_isDrawing;
    int m_pointCount;

    // dynamic parameters
    QPointF m_speed; // used as a vector
};

#endif // KARBONCALLIGRAPHYTOOL_H
