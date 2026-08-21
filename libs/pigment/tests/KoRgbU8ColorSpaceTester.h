/*
 *  SPDX-FileCopyrightText: 2005 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#ifndef KORGBCOLORSPACETESTER_H
#define KORGBCOLORSPACETESTER_H

#include <PkTest.h>
// Q_OBJECT / private Q_SLOTS: 的 token 留给 pk_test_moc.py 扫描（Q 后跟 _ 不命中判据正则）。
// 宏展开与 pk/test/compat 的 Q_OBJECT 垫片同构；.cpp 经 <simpletest.h> 引入的 compat 定义与此相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS


class KoRgbU8ColorSpaceTester : public PkTestObject
{
    Q_OBJECT
    void testCompositeOps();
private Q_SLOTS:
    void testBasics();
    void testMixColors();
    void testMixColorsAverage();
    void testCompositeOpsWithChannelFlags();
    void testCompositeCopyDivisionByZero();
};

#endif

