/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt boud @valdyas.org
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_KRA_SAVER_TEST_H
#define KIS_KRA_SAVER_TEST_H

#include <PkTest.h>
// Q_OBJECT / private Q_SLOTS: 的 token 留给 pk_test_moc.py 扫描（Q 后跟 _ 不命中判据正则）。
// 宏展开与 pk/test/compat 的 Q_OBJECT 垫片同构；.cpp 经 PkTest.h 引入的声明与此相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS


class KisKraSaverTest : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:

    void initTestCase();

    void testCrashyShapeLayer();

    // XXX: Also test roundtripping of metadata
    void testRoundTrip();

    void testSaveEmpty();
    void testRoundTripFillLayerColor();
    void testRoundTripFillLayerPattern();

    void testRoundTripLayerStyles();

    void testRoundTripAnimation();

    void testRoundTripColorizeMask();

    void testRoundTripShapeLayer();
    void testRoundTripShapeSelection();
    void testRoundTripStoryboard();

    void testExportToReadonly();

};

#endif
