/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_INPUT_TRANSFORM_POLICY_TEST_H_
#define KIS_INPUT_TRANSFORM_POLICY_TEST_H_

#include <QObject>

class KisInputTransformPolicyTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void testContinuousAndDiscreteZoomMapping();
    void testPinchZoomValidationAndState();
    void testCombinedGestureScaleAndRotation();
    void testCombinedRotationModes();
    void testDiscreteCanvasRotation();
    void testTouchRotationState();
};

#endif // KIS_INPUT_TRANSFORM_POLICY_TEST_H_
