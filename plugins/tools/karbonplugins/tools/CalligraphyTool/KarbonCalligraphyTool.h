/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Fela Winkelmolen <fela.kde@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KARBONCALLIGRAPHYTOOL_H
#define KARBONCALLIGRAPHYTOOL_H

#include <KoToolBase.h>
#include <PkPainter.h>
#include <PkPainterPath.h>

class KoPathShape;
class KarbonCalligraphicShape;

class KarbonCalligraphyTool : public KoToolBase
{
public:
    explicit KarbonCalligraphyTool(KoCanvasBase *canvas);
    ~KarbonCalligraphyTool() override;

    void paint(PkPainter &painter, const KoViewConverter &converter) override;

    void mousePressEvent(KoPointerEvent *event) override;
    void mouseMoveEvent(KoPointerEvent *event) override;
    void mouseReleaseEvent(KoPointerEvent *event) override;

    KisPopupWidgetInterface *popupWidget() override;

    void activate(const PkSet<KoShape *> &shapes) override;
    void deactivate() override;

private:
    void updateSelectedPath();

private:
    void addPoint(KoPointerEvent *event);
    // auxiliary function that sets m_angle
    void setAngle(KoPointerEvent *event);
    // auxiliary functions to calculate the dynamic parameters
    // returns the new point and sets speed to the speed
    PkPointF calculateNewPoint(const PkPointF &mousePos, PkPointF *speed);
    double calculateWidth(double pressure);
    double calculateAngle(const PkPointF &oldSpeed, const PkPointF &newSpeed);

    PkPointF m_lastPoint;
    KarbonCalligraphicShape *m_shape;

    // used to determine if the device supports tilt
    bool m_deviceSupportsTilt;

    bool m_usePath;         // follow selected path
    bool m_usePressure;     // use tablet pressure
    bool m_useAngle;        // use tablet angle
    double m_strokeWidth;
    double m_lastWidth;
    double m_customAngle;   // angle set by the user
    double m_angle;  // angle to use, may use the device angle, in radians!!!
    double m_fixation;
    double m_thinning;
    double m_caps;
    double m_mass;  // in raw format (not user friendly)
    double m_drag;  // from 0.0 to 1.0

    KoPathShape *m_selectedPath;
    PkPainterPath m_selectedPathOutline;
    double m_followPathPosition;
    bool m_endOfPath;
    PkPointF m_lastMousePos;

    bool m_isDrawing;
    int m_pointCount;

    // dynamic parameters
    PkPointF m_speed; // used as a vector
};

#endif // KARBONCALLIGRAPHYTOOL_H
