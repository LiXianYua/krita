#pragma once

// PkSize / PkSizeF 的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
// 本地等价展开、不走 pk/test/compat/QObject 的理由与 point_case.h 相同
//（走那条路要求编译行有 -I pk/test/compat，会把 <QtGlobal> 的解析目标搅进来）。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkSizeCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── PkSize ──
    void sizeDefaultIsMinusOne();
    void sizeConstructionAndLayout();
    void sizeAccessorsAndReferences();
    void sizeThreePredicates();
    void sizeExpandedTo();
    void sizeScaledThreeModes();
    void sizeScaledDegenerateSource();
    void sizeScaledUsesInt64Intermediate();
    void sizeScaleInPlace();
    void sizeAdditiveOperators();
    void sizeScalingRoundsLikeQt();
    void sizeDivision();
    void sizeEquality();

    // ── PkSizeF ──
    void sizefDefaultIsMinusOne();
    void sizefConstructionAndLayout();
    void sizefPromotionFromPkSize();
    void sizefAccessorsAndReferences();
    void sizefThreePredicates();
    void sizefExpandedTo();
    void sizefScaledThreeModes();
    void sizefScaledDegenerateSource();
    void sizefScaledSpecialValues();
    void sizefScaleInPlace();
    void sizefArithmeticOperators();
    void sizefFuzzyEquality();
    void sizefFuzzyEqualityOnSpecialValues();
    void sizefToSizeMatchesQt();
    void sizefDivision();

    // ── 跨切面 ──
    void aspectRatioModeEnumValues();
    void noexceptSurfaceMatchesQt();
    void sizeFuzzyEqualityIsMacroProof();
};

#undef Q_SLOTS
#undef Q_OBJECT
