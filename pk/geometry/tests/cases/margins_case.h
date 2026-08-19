#pragma once

// PkMargins / PkMarginsF 的单测类，加 PkRect/PkRectF 与它们的四个互操作成员。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
// 本地等价展开、不走 pk/test/compat/QObject 的理由与 rect_case.h 相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkMarginsCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── PkMargins 构造与取值 ──
    void marginsDefaultCtorIsZero();
    void marginsFourArgCtor();
    void marginsSetters();
    void marginsIsNull();
    void marginsEquality();
    void marginsLayoutIsFourInt();

    // ── PkMargins 算术 ──
    void marginsAddSubMargins();
    void marginsAddSubInt();
    void marginsMulDivInt();
    void marginsMulDivQrealRounds();
    void marginsUnaryPlusMinus();

    // ── PkMarginsF ──
    void marginsFDefaultAndCtor();
    void marginsFPromotionFromMarginsIsImplicit();
    void marginsFIsNullIsFuzzy();
    void marginsFEqualityIsFuzzy();
    void marginsFArithmetic();
    void marginsFToMarginsRounds();

    // ── 与 PkRect / PkRectF 的互操作（R-21 T1 解锁的四个成员）──
    void rectMarginsAddedAndRemoved();
    void rectOperatorPlusMinusMargins();
    void rectFMarginsAddedAndRemoved();
    void rectFOperatorPlusMinusMarginsF();
    void rectFMarginsAddedAcceptsMarginsPromotion();
};

#undef Q_SLOTS
#undef Q_OBJECT
