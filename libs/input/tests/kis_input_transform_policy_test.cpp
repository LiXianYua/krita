/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_input_transform_policy_test.h"

#include <simpletest.h>

#include "KisInputTransformPolicy.h"

#include <cmath>

namespace
{
bool closeEnough(qreal lhs, qreal rhs)
{
    return std::abs(lhs - rhs) < 1e-6;
}
}

void KisInputTransformPolicyTest::testContinuousAndDiscreteZoomMapping()
{
    using namespace KisInputTransformPolicy;

    QVERIFY(closeEnough(continuousZoom(2.0, PkPointF(0.0, 100.0), false, false), 4.0));
    QVERIFY(closeEnough(continuousZoom(2.0, PkPointF(-100.0, 0.0), true, false), 4.0));
    QVERIFY(closeEnough(continuousZoom(2.0, PkPointF(0.0, 100.0), false, true), 1.0));

    qreal accumulator = 0.0;
    QCOMPARE(consumeDiscreteZoomSteps(50.0, accumulator), 0);
    QCOMPARE(accumulator, 0.0);
    QCOMPARE(consumeDiscreteZoomSteps(50.001, accumulator), 1);
    QCOMPARE(accumulator, 1.0);
    QCOMPARE(consumeDiscreteZoomSteps(-100.001, accumulator), -3);
    QCOMPARE(accumulator, -2.0);
}

void KisInputTransformPolicyTest::testPinchZoomValidationAndState()
{
    using namespace KisInputTransformPolicy;

    PinchZoomState state;
    auto result = updatePinchZoom(state, PkPointF(0, 0), PkPointF(5, 4), false, 2.0);
    QVERIFY(!result.shouldApply);
    QVERIFY(!state.hasReference);

    PinchZoomState exactThresholdState;
    result = updatePinchZoom(exactThresholdState, PkPointF(0, 0), PkPointF(6, 4), false, 2.0);
    QVERIFY(!result.shouldApply);
    QVERIFY(exactThresholdState.hasReference);

    result = updatePinchZoom(state, PkPointF(0, 0), PkPointF(100, 0), false, 2.0);
    QVERIFY(!result.shouldApply);
    QVERIFY(state.hasReference);

    result = updatePinchZoom(state, PkPointF(0, 0), PkPointF(100, 0), false, 2.0);
    QVERIFY(result.shouldApply);
    QVERIFY(closeEnough(result.zoom, 2.0));

    result = updatePinchZoom(state, PkPointF(0, 0), PkPointF(79, 0), false, 2.0);
    QVERIFY(!result.shouldApply);
    result = updatePinchZoom(state, PkPointF(0, 0), PkPointF(80, 0), false, 2.0);
    QVERIFY(result.shouldApply);
    QVERIFY(closeEnough(result.zoom, 1.6));

    result = updatePinchZoom(state, PkPointF(0, 0), PkPointF(97, 0), false, result.zoom);
    QVERIFY(!result.shouldApply);
    result = updatePinchZoom(state, PkPointF(0, 0), PkPointF(96, 0), false, 1.6);
    QVERIFY(result.shouldApply);
    QVERIFY(closeEnough(result.zoom, 1.92));

    result = updatePinchZoom(state, PkPointF(), PkPointF(), true, 1.92);
    QVERIFY(!result.shouldApply);
    QVERIFY(!state.hasReference);
}

void KisInputTransformPolicyTest::testCombinedGestureScaleAndRotation()
{
    using namespace KisInputTransformPolicy;

    CombinedRotationState state;
    const qreal initialAngle = 30.0 * M_PI / 180.0;
    const qreal nextAngle = 45.0 * M_PI / 180.0;

    auto result = updateCombinedGesture(
        state,
        CombinedRotationMode::Discrete,
        PkPointF(),
        PkPointF(100.0 * std::cos(initialAngle), 100.0 * std::sin(initialAngle)),
        0.0);
    QCOMPARE(result.scaleDelta, 1.0f);
    QCOMPARE(result.rotationDelta, 0.0);
    QCOMPARE(state.lastDistance, 100.0f);

    result = updateCombinedGesture(
        state,
        CombinedRotationMode::Discrete,
        PkPointF(),
        PkPointF(120.0 * std::cos(nextAngle), 120.0 * std::sin(nextAngle)),
        0.0);
    QVERIFY(closeEnough(result.scaleDelta, 1.2));
    QCOMPARE(result.rotationDelta, 15.0);
    QCOMPARE(state.lastDistance, 120.0f);
}

void KisInputTransformPolicyTest::testCombinedRotationModes()
{
    using namespace KisInputTransformPolicy;

    CombinedRotationState continuous;
    QCOMPARE(updateCombinedRotation(continuous, CombinedRotationMode::Continuous, 0.5, 0.0), 0.0);
    QVERIFY(closeEnough(updateCombinedRotation(continuous, CombinedRotationMode::Continuous, 0.52, 43.0), 2.0));
    QVERIFY(closeEnough(continuous.accumulatedSnapRotation, 180.0 / M_PI * 0.02));
    QVERIFY(closeEnough(updateCombinedRotation(continuous, CombinedRotationMode::Continuous, 0.6, 43.0),
                        180.0 / M_PI * 0.10));
    QCOMPARE(continuous.accumulatedSnapRotation, 0.0);

    CombinedRotationState discrete;
    QCOMPARE(updateCombinedRotation(discrete, CombinedRotationMode::Discrete, 0.5, 0.0), 0.0);
    QCOMPARE(updateCombinedRotation(discrete, CombinedRotationMode::Discrete, 0.5 + 14.9 * M_PI / 180.0, 0.0), 0.0);
    QCOMPARE(updateCombinedRotation(discrete, CombinedRotationMode::Discrete, 0.5 + 15.001 * M_PI / 180.0, 0.0), 15.0);

    CombinedRotationState wrappedDiscrete;
    QCOMPARE(updateCombinedRotation(wrappedDiscrete, CombinedRotationMode::Discrete, 0.5, 0.0), 0.0);
    QCOMPARE(updateCombinedRotation(wrappedDiscrete, CombinedRotationMode::Discrete, 0.5 + 341.0 * M_PI / 180.0, 0.0), -15.0);

    QCOMPARE(angleForSnapping(43.0), -2.0);
    QCOMPARE(angleForSnapping(-43.0), 2.0);

    CombinedRotationState positiveBoundary;
    positiveBoundary.previousAngle = 1.0;
    positiveBoundary.accumulatedSnapRotation = 2.0;
    QVERIFY(closeEnough(updateCombinedRotation(positiveBoundary,
                                               CombinedRotationMode::Continuous,
                                               1.0 + M_PI / 180.0,
                                               42.0),
                        3.0));

    CombinedRotationState negativeBoundary;
    negativeBoundary.previousAngle = 1.0;
    negativeBoundary.accumulatedSnapRotation = -2.0;
    QVERIFY(closeEnough(updateCombinedRotation(negativeBoundary,
                                               CombinedRotationMode::Continuous,
                                               1.0 - M_PI / 180.0,
                                               -42.0),
                        -3.0));
}

void KisInputTransformPolicyTest::testDiscreteCanvasRotation()
{
    using namespace KisInputTransformPolicy;

    DiscreteCanvasRotationState state = beginDiscreteCanvasRotation(-23.0);
    QVERIFY(closeEnough(state.snapRotation, -8.0));
    QCOMPARE(updateDiscreteCanvasRotation(state, 7.5), 0.0);
    QCOMPARE(updateDiscreteCanvasRotation(state, 7.5001), 8.0);
    QCOMPARE(updateDiscreteCanvasRotation(state, -1.0), -7.0);
}

void KisInputTransformPolicyTest::testTouchRotationState()
{
    using namespace KisInputTransformPolicy;

    TouchRotationState state;
    auto result = updateTouchRotation(state, PkPointF(0, 0), PkPointF(4, 5), false);
    QVERIFY(!result.shouldApply);
    QVERIFY(!state.hasPreviousAngle);

    TouchRotationState exactThresholdState;
    result = updateTouchRotation(exactThresholdState, PkPointF(0, 0), PkPointF(6, 4), false);
    QVERIFY(!result.shouldApply);
    QVERIFY(exactThresholdState.hasPreviousAngle);

    result = updateTouchRotation(state, PkPointF(0, 0), PkPointF(100, 0), false);
    QVERIFY(!result.shouldApply);
    QVERIFY(state.hasPreviousAngle);

    result = updateTouchRotation(state, PkPointF(0, 0), PkPointF(0, 100), false);
    QVERIFY(result.shouldApply);
    QVERIFY(closeEnough(result.rotation, 90.0));

    result = updateTouchRotation(state, PkPointF(), PkPointF(), true);
    QVERIFY(!result.shouldApply);
    QVERIFY(state.hasPreviousAngle);
}

#include "pk_binder_kis_input_transform_policy_test.inc"

SIMPLE_TEST_MAIN(KisInputTransformPolicyTest)
