#pragma once

// PkSharedPointer 的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。本地等价展开、
// 不走 pk/test/compat/QObject 的理由与 pk/geometry/tests/cases/point_case.h
// 相同——走那条路要求编译行有 -I pk/test/compat，会把这里没用到的解析目标
// 搅进来。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkSharedPointerCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void init();

    void defaultCtorIsNull();
    void ctorFromNullRawPointerIsNull();
    void refcountControlsDestruction();
    void resetReplacesAndReleases();
    void casts();
    void create();
    void customDeleter();
    void selfAssignKeepsAlive();
    void comparisons();
    void assignNullptrClears();
    void usableAsHashKey();
};

#undef Q_SLOTS
#undef Q_OBJECT
