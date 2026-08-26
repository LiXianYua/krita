/*
 *  SPDX-FileCopyrightText: 2012 Arjen Hiemstra <ahiemstra@heimr.nl>
 *  SPDX-FileCopyrightText: 2019 Sharaf Zaman <sharafzaz121@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISINPUTTRANSFORMPOLICY_H
#define KISINPUTTRANSFORMPOLICY_H

#include <kritainput_export.h>

#include <PkPoint.h>

namespace KisInputTransformPolicy
{

KRITAINPUT_EXPORT qreal continuousZoom(qreal startZoom,
                                       const PkPointF &dragDelta,
                                       bool horizontal,
                                       bool inverted);

KRITAINPUT_EXPORT int consumeDiscreteZoomSteps(qreal axisDelta, qreal &accumulatedSteps);

struct KRITAINPUT_EXPORT PinchZoomState
{
    bool hasReference {false};
    float lastDistance {0.0f};
};

struct KRITAINPUT_EXPORT PinchZoomResult
{
    bool shouldApply {false};
    qreal zoom {1.0};
};

KRITAINPUT_EXPORT PinchZoomResult updatePinchZoom(PinchZoomState &state,
                                                 const PkPointF &firstPoint,
                                                 const PkPointF &secondPoint,
                                                 bool anyPointReleased,
                                                 qreal currentZoom);

enum class CombinedRotationMode {
    Continuous,
    Discrete
};

struct KRITAINPUT_EXPORT CombinedRotationState
{
    float lastDistance {0.0f};
    qreal previousAngle {0.0};
    qreal initialReferenceAngle {0.0};
    qreal accumulatedSnapRotation {0.0};
};

struct KRITAINPUT_EXPORT CombinedGestureResult
{
    float scaleDelta {1.0f};
    qreal rotationDelta {0.0};
};

KRITAINPUT_EXPORT qreal angleForSnapping(qreal angle);
KRITAINPUT_EXPORT qreal updateCombinedRotation(CombinedRotationState &state,
                                               CombinedRotationMode mode,
                                               qreal currentAngleRadians,
                                               qreal currentCanvasRotationDegrees);
KRITAINPUT_EXPORT CombinedGestureResult updateCombinedGesture(
    CombinedRotationState &state,
    CombinedRotationMode mode,
    const PkPointF &firstPoint,
    const PkPointF &secondPoint,
    qreal currentCanvasRotationDegrees);

struct KRITAINPUT_EXPORT DiscreteCanvasRotationState
{
    qreal snapRotation {0.0};
    bool allowRotation {false};
};

KRITAINPUT_EXPORT DiscreteCanvasRotationState beginDiscreteCanvasRotation(qreal startRotationDegrees);
KRITAINPUT_EXPORT qreal updateDiscreteCanvasRotation(DiscreteCanvasRotationState &state,
                                                     qreal dragRotationDegrees);

struct KRITAINPUT_EXPORT TouchRotationState
{
    bool hasPreviousAngle {false};
    qreal previousAngle {0.0};
    qreal accumulatedRotation {0.0};
};

struct KRITAINPUT_EXPORT TouchRotationResult
{
    bool shouldApply {false};
    qreal rotation {0.0};
};

KRITAINPUT_EXPORT TouchRotationResult updateTouchRotation(TouchRotationState &state,
                                                          const PkPointF &firstPoint,
                                                          const PkPointF &secondPoint,
                                                          bool anyPointReleased);

} // namespace KisInputTransformPolicy

#endif // KISINPUTTRANSFORMPOLICY_H
