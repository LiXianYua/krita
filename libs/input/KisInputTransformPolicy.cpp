/*
 *  SPDX-FileCopyrightText: 2012 Arjen Hiemstra <ahiemstra@heimr.nl>
 *  SPDX-FileCopyrightText: 2019 Sharaf Zaman <sharafzaz121@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisInputTransformPolicy.h"

#include <QLineF>

#include <cmath>

namespace KisInputTransformPolicy
{
namespace
{
constexpr int ContinuousZoomStep = 100;
constexpr int DiscreteZoomStep = 50;
constexpr qreal DiscreteRotationStep = 15.0;
}

qreal continuousZoom(qreal startZoom, const QPointF &dragDelta, bool horizontal, bool inverted)
{
    const qreal axisDelta = horizontal ? -dragDelta.x() : dragDelta.y();
    const qreal logarithmicDistance = std::pow(2.0, axisDelta / qreal(ContinuousZoomStep));
    return inverted ? startZoom / logarithmicDistance : startZoom * logarithmicDistance;
}

int consumeDiscreteZoomSteps(qreal axisDelta, qreal &accumulatedSteps)
{
    qreal currentDelta = axisDelta / DiscreteZoomStep - accumulatedSteps;
    const bool zoomIn = currentDelta > 0;
    int steps = 0;

    while (std::abs(currentDelta) > 1.0) {
        const int step = zoomIn ? 1 : -1;
        steps += step;
        accumulatedSteps += step;
        currentDelta = axisDelta / DiscreteZoomStep - accumulatedSteps;
    }

    return steps;
}

PinchZoomResult updatePinchZoom(PinchZoomState &state,
                                const QPointF &firstPoint,
                                const QPointF &secondPoint,
                                bool anyPointReleased,
                                qreal currentZoom)
{
    if (anyPointReleased) {
        state.hasReference = false;
        return {};
    }

    if ((firstPoint - secondPoint).manhattanLength() < 10) {
        state.hasReference = false;
        return {};
    }

    if (!state.hasReference) {
        state.hasReference = true;
        state.lastDistance = 0.0f;
        return {};
    }

    const float distance = QLineF(firstPoint, secondPoint).length();
    const float delta = qFuzzyCompare(1.0f, 1.0f + state.lastDistance)
        ? 1.0f
        : distance / state.lastDistance;

    if (std::abs(delta) < 0.8f || std::abs(delta) > 1.2f) {
        return {};
    }

    state.lastDistance = distance;
    return {true, currentZoom * delta};
}

qreal angleForSnapping(qreal angle)
{
    if (angle < 0) {
        return std::fmod(angle - 2, 45) + 2;
    }
    return std::fmod(angle + 2, 45) - 2;
}

qreal updateCombinedRotation(CombinedRotationState &state,
                             CombinedRotationMode mode,
                             qreal currentAngleRadians,
                             qreal currentCanvasRotationDegrees)
{
    if (mode == CombinedRotationMode::Continuous) {
        if (!state.previousAngle) {
            state.previousAngle = currentAngleRadians;
            return 0.0;
        }

        qreal rotationAngle = (180.0 / M_PI) * (currentAngleRadians - state.previousAngle);
        state.previousAngle = currentAngleRadians;

        const qreal snapDelta = angleForSnapping(currentCanvasRotationDegrees + rotationAngle);
        if (std::abs(snapDelta) <= 2 && std::abs(state.accumulatedSnapRotation) <= 2) {
            state.accumulatedSnapRotation += rotationAngle;
            rotationAngle -= snapDelta;
        } else {
            rotationAngle += state.accumulatedSnapRotation;
            state.accumulatedSnapRotation = 0;
        }
        return rotationAngle;
    }

    if (!state.initialReferenceAngle) {
        state.initialReferenceAngle = currentAngleRadians;
        return 0.0;
    }

    const qreal relativeAngle = (180.0 / M_PI) * (currentAngleRadians - state.initialReferenceAngle);
    constexpr qreal RotationThreshold = 15.0;
    if (std::abs(relativeAngle) >= RotationThreshold &&
        std::abs(relativeAngle) <= 360.0 - RotationThreshold) {
        state.initialReferenceAngle = currentAngleRadians;
        return std::abs(relativeAngle) <= 180.0
            ? std::copysign(15.0, relativeAngle)
            : std::copysign(15.0, -relativeAngle);
    }
    return 0.0;
}

CombinedGestureResult updateCombinedGesture(CombinedRotationState &state,
                                            CombinedRotationMode mode,
                                            const QPointF &firstPoint,
                                            const QPointF &secondPoint,
                                            qreal currentCanvasRotationDegrees)
{
    const QPointF slope = secondPoint - firstPoint;
    const qreal currentAngle = std::atan2(slope.y(), slope.x());
    const qreal rotationDelta = updateCombinedRotation(state,
                                                       mode,
                                                       currentAngle,
                                                       currentCanvasRotationDegrees);

    const float distance = QLineF(firstPoint, secondPoint).length();
    const float scaleDelta = qFuzzyCompare(1.0f, 1.0f + state.lastDistance)
        ? 1.0f
        : distance / state.lastDistance;
    state.lastDistance = distance;

    return {scaleDelta, rotationDelta};
}

DiscreteCanvasRotationState beginDiscreteCanvasRotation(qreal startRotationDegrees)
{
    DiscreteCanvasRotationState state;
    state.snapRotation = startRotationDegrees -
        std::trunc(startRotationDegrees / DiscreteRotationStep) * DiscreteRotationStep;
    return state;
}

qreal updateDiscreteCanvasRotation(DiscreteCanvasRotationState &state, qreal dragRotationDegrees)
{
    if (std::abs(dragRotationDegrees) > 0.5 * DiscreteRotationStep || state.allowRotation) {
        state.allowRotation = true;
        return std::round((dragRotationDegrees + state.snapRotation) / DiscreteRotationStep) *
            DiscreteRotationStep - state.snapRotation;
    }
    return 0.0;
}

TouchRotationResult updateTouchRotation(TouchRotationState &state,
                                        const QPointF &firstPoint,
                                        const QPointF &secondPoint,
                                        bool anyPointReleased)
{
    if (anyPointReleased || (firstPoint - secondPoint).manhattanLength() < 10) {
        return {};
    }

    const QPointF slope = secondPoint - firstPoint;
    const qreal newAngle = std::atan2(slope.y(), slope.x());

    if (!state.hasPreviousAngle) {
        state.hasPreviousAngle = true;
        state.previousAngle = newAngle;
        return {};
    }

    state.accumulatedRotation += (180.0 / M_PI) * (newAngle - state.previousAngle);
    state.previousAngle = newAngle;
    return {true, state.accumulatedRotation};
}

} // namespace KisInputTransformPolicy
