/*
 *  SPDX-FileCopyrightText: 2020 Ashwin Dhakaita <ashwingpdhakaita@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_MYPAINTOP_TEST_H
#define KIS_MYPAINTOP_TEST_H

#include <pk/test/compat/QObject>
#include <pk/test/compat/QTest>

class KisMyPaintOpTest : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void testDab();
    void testGetColor();
    void testLoading();
    void testParseBufferIsNulTerminatedWithoutChangingRawBytes();
    void testSlowTrackingPolicyPreservesFreehandValue();
    void testSlowTrackingPolicyDefaultsToHeadlessClear();
    void testSlowTrackingPolicyIsNotPersisted();
    void testInvalidRawPresetFallsBackWithoutChangingRawBytes();
};

#endif // KIS_MYPAINTOP_TEST_H
