/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef TestColorConversionSystem_H
#define TestColorConversionSystem_H

#include <PkTest.h>
// Q_OBJECT / private Q_SLOTS: 的 token 留给 pk_test_moc.py 扫描（Q 后跟 _ 不命中判据正则）。
// 宏展开与 pk/test/compat 的 Q_OBJECT 垫片同构；.cpp 经 <simpletest.h> 引入的 compat 定义与此相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include <PkList.h>
#include <PkString.h>

#include <KoColorConversionSystem.h>

struct ModelDepthProfile {
    ModelDepthProfile(const PkString& _model, const PkString& _depth, const PkString& _profile)
            : model(_model), depth(_depth), profile(_profile) {
    }
    PkString model;
    PkString depth;
    PkString profile;
};

class TestColorConversionSystem : public PkTestObject
{
    Q_OBJECT
public:
    TestColorConversionSystem();
private Q_SLOTS:
    void testConnections();
    void testGoodConnections();
    void testAlphaConnectionPaths();
    void testAlphaConversions();
    void testAlphaU16Conversions();
    void testGrayAConnectionPaths();
    void testGrayAConversions();
    void benchmarkAlphaToRgbConversion();
    void benchmarkRgbToAlphaConversion();

    void testCmykBitnessConversion();

private:
    std::vector<KoColorConversionSystem::NodeKey> calcPath(const std::vector<KoColorConversionSystem::NodeKey> &expectedPath);

private:
    PkList< ModelDepthProfile > listModels;
};

#endif
