/*
 *  SPDX-FileCopyrightText: 2023 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef TESTCOMPOSITEOPINVERSION_H
#define TESTCOMPOSITEOPINVERSION_H

#include <PkTest.h>
// Q_OBJECT / private Q_SLOTS: 的 token 留给 pk_test_moc.py 扫描（Q 后跟 _ 不命中判据正则）。
// 宏展开与 pk/test/compat 的 Q_OBJECT 垫片同构；.cpp 经 <simpletest.h> 引入的 compat 定义与此相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS


class TestCompositeOpInversion : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void test();
    void test_data();

    void testColorPairSampler();
    void testColorPairSamplerRGB();

    void testF32ModesNaN();
    void testF32ModesNaN_data();

    // 原文件把 testU16ModesConsistent(+_data) 用 #if 0 关掉（声明在头、定义在 .cpp 同
    // 样 #if 0）。pk_test_moc.py 的槽扫描是纯正则、不认预处理分支，声明留在头里会让
    // 生成的 PkTestBinder 引用从不编译的定义。这里按官方语义（这两个函数不参与运行）
    // 直接不声明；.cpp 里的 #if 0 尸体照旧保留。

    void testF32vsU16ConsistencyInSDR_data();
    void testF32vsU16ConsistencyInSDR();

    void testPreservesStrictSdrRange();
    void testPreservesStrictSdrRange_data();

    void testPreservesLooseSdrRange();
    void testPreservesLooseSdrRange_data();

    void testSrcCannotMakeNegative();
    void testSrcCannotMakeNegative_data();

    void testPreservesStrictNegative();
    void testPreservesStrictNegative_data();

    void testPreservesLooseNegative();
    void testPreservesLooseNegative_data();

    void testToneMappingPositive();
    void testToneMappingNegative();


    void dumpOpCategories();

private:
    /// Uncomment if you want to generate the sample
    /// sheets of every blendmode to verify their contiguity
    void generateSampleSheetsLong_data();
    void generateSampleSheetsLong();

private:
    // TODO: disabled for now
    void testF16Modes();
    void testF16Modes_data();

    /// just a simple test case to test exactly one color
    /// during debugging
    void testColor();

private:
    void testNegativeImpl(bool useStrictZeroCheck);
    void testPreservesSdrRangeImpl(bool useStrictRange);
};

#endif // TESTCOMPOSITEOPINVERSION_H
