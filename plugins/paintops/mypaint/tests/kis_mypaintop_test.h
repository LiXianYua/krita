/*
 *  SPDX-FileCopyrightText: 2020 Ashwin Dhakaita <ashwingpdhakaita@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_MYPAINTOP_TEST_H
#define KIS_MYPAINTOP_TEST_H

#include <PkTest.h>
#include <PkTestObject.h>

#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

class KisMyPaintOpTest : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void testDab();
    void testGetColor();
    void testLoading();
    void testParseBufferIsNulTerminatedWithoutChangingRawBytes();
    void testSlowTrackingPolicyPreservesFreehandValue();
    void testSlowTrackingPolicyDefaultsToHeadlessClear();
    void testSlowTrackingPolicyIsNotPersisted();
    void testInvalidRawPresetFallsBackWithoutChangingRawBytes();
};

#undef Q_SLOTS
#undef Q_OBJECT

#endif // KIS_MYPAINTOP_TEST_H
