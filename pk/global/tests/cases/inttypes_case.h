#pragma once

// PkGlobal.h 整数别名 / qreal 别名的编译期断言（static_assert + type_traits）。
//
// 生成器规则同上（Q_OBJECT 与 "private Q_SLOTS:" 必须字面出现）。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkInttypesCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    void sizesMatchStdInt();
    void signednessMatchesNames();
    void qrealIsDouble();
    void symmetricAliasesMatch();
    void cCompatibleAliasesMatch();
};

#undef Q_SLOTS
#undef Q_OBJECT
