#pragma once

// Task 5 生成器自测的输入头：类声明与真实 Krita 测试类同形（Q_OBJECT +
// private Q_SLOTS: + 只声明不定义），供 pk_test_moc.py 扫描并生成
// PkTestBinder<SelfAssertCase> 特化，替换掉 Task 1 手写的那份。
// 定义放在 ../selftest_assert.cpp。
//
// 生成器靠扫描源文件文本里字面的 "Q_OBJECT" / "private Q_SLOTS:" 认出测试类，
// 所以这两个 token 必须真的出现在文件里。真正的 compat/QObject 垫片（把
// Q_OBJECT 展开成 friend 模板）由 Task 6 交付；这里只在本文件范围内做等价
// 展开，不进 compat/、不影响 Task 7 的真实试接。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../PkTest.h"

class SelfAssertCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void passingVerify();
    void failingVerify();
    void unconditionalFail();
    void stopsAtFirstFailure();
};

#undef Q_SLOTS
#undef Q_OBJECT
