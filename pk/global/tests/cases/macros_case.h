#pragma once

// Q_UNUSED / Q_ASSERT 两个宏的单测类。
//
// Q_UNUSED 的「-Wall -Werror 下编过」由 CMake 对 test_macros.cpp 单独加
// -Wall -Werror 编译选项来守（见 CMakeLists.txt）；Q_ASSERT 的触发行为用 fork
// 子进程验证。
//
// 生成器规则同上（Q_OBJECT 与 "private Q_SLOTS:" 必须字面出现）。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkMacrosCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void unusedDoesNotWarn();
    void assertTrueIsNoop();
    void assertFalseAborts();
};

#undef Q_SLOTS
#undef Q_OBJECT
