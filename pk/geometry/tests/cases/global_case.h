#pragma once

// PkGlobal.h（标量工具）的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
//
// 这里在本文件范围内做等价展开，不走 pk/test/compat/QObject —— 走那条路要求
// 编译行有 -I pk/test/compat，而那恰好会让 <QtGlobal> 解析到 pk/test 那份垫片，
// 把本套测试要控制的 include 顺序（tests/coexist_*.cpp 的被测变量）搅进来。
// 先例：pk/test/tests/generator_cases/self_assert_case.h。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkGlobalCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void qrealIsDouble();
    void absMatchesQt();
    void minMaxBoundMatchQt();
    void roundMatchesQt();
    void roundFloatOverloadIsReallyFloat();
    void fuzzyCompareMatchesQt();
    void fuzzyCompareFloatOverloadIsReallyFloat();
    void fuzzyIsNullMatchesQt();
    void isNaNMatchesQt();
    void infMatchesQt();
    void coexistWithPkTestShimFirst();
    void coexistWithGeometryShimFirst();
    void coexistWithCompatRectFirst();
};

#undef Q_SLOTS
#undef Q_OBJECT
