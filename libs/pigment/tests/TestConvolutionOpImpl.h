/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef _TEST_CONVOLUTION_OP_IMPL_H_
#define _TEST_CONVOLUTION_OP_IMPL_H_

#include <PkTest.h>
// Q_OBJECT / private Q_SLOTS: 的 token 留给 pk_test_moc.py 扫描（Q 后跟 _ 不命中判据正则）。
// 宏展开与 pk/test/compat 的 Q_OBJECT 垫片同构；.cpp 经 <simpletest.h> 引入的 compat 定义与此相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS


class TestConvolutionOpImpl : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void testConvolutionOpImpl();
    void testOneSemiTransparent();
    void testOneFullyTransparent();
};

#endif

