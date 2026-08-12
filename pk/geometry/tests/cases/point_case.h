#pragma once

// PkPoint / PkPointF 的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
// 本地等价展开、不走 pk/test/compat/QObject 的理由与 global_case.h 相同
//（走那条路要求编译行有 -I pk/test/compat，会把 <QtGlobal> 的解析目标搅进来）。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkPointCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── PkPoint ──
    void pointConstruction();
    void pointAccessorsAndReferences();
    void pointIsNull();
    void pointTransposed();
    void pointManhattanLength();
    void pointAdditiveOperators();
    void pointScalingRoundsLikeQt();
    void pointFloatOverloadIsReallyFloat();
    void pointIntegerScaling();
    void pointDivisionByZeroMatchesQt();
    void pointEquality();
    void pointDotProduct();

    // ── PkPointF ──
    void pointfConstruction();
    void pointfPromotionFromPkPoint();
    void pointfAccessorsAndReferences();
    void pointfIsNull();
    void pointfTransposed();
    void pointfManhattanLength();
    void pointfAdditiveOperators();
    void pointfScalarOperators();
    void pointfSignedZeroIsBitExact();
    void pointfFuzzyEquality();
    void pointfFuzzyEqualityOnSpecialValues();
    void pointfToPointMatchesQt();
    void pointfDotProduct();

    // ── 跨切面 ──
    void fuzzyEqualityIsMacroProof();
};

#undef Q_SLOTS
#undef Q_OBJECT
