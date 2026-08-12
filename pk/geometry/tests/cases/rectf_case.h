#pragma once

// PkRectF 的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
// 本地等价展开、不走 pk/test/compat/QObject 的理由与 rect_case.h 相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkRectFCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── 构造与布局 ──
    void rectfDefaultIsAllZero();
    void rectfFiveConstructors();
    void rectfLayoutIsFourQreal();

    // ── 与 PkRect 的差一之别（本 Task 最著名的坑）──
    void rectfRightBottomHaveNoOffByOne();
    void rectfSideBySideWithPkRect();

    // ── 三谓词：门槛与 PkRect 全不同 ──
    void rectfThreePredicatesUseFloatThresholds();
    void rectfPredicatesOnSignedZeroAndSubnormal();
    void rectfPredicatesAreNotComplementsOnNan();

    // ── 取值器 ──
    void rectfCornersAndSize();
    void rectfCenterIsHalfWidthNotTruncated();

    // ── 修改器：set* 保对边、move* 保宽高 ──
    void rectfSetEdgesKeepOppositeEdge();
    void rectfSetXSetYAreSetLeftSetTop();
    void rectfSetTopLeftAndBottomRight();
    void rectfSetWidthSetHeightAnchorTopLeft();
    void rectfSetSizeAnchorsTopLeft();
    void rectfSetRectAndGetRect();
    void rectfSetCoordsAndGetCoords();

    // ── 平移一族 ──
    void rectfMoveToKeepsSize();
    void rectfMoveLeftMoveTopKeepSize();
    void rectfMoveTopLeftIsTwoMoves();
    void rectfMoveCenterUsesHalfWidth();
    void rectfTranslateAndTranslated();

    // ── adjust ──
    void rectfAdjustUsesDeltaOfDeltas();

    // ── normalized ──
    void rectfNormalizedSwapBoundaryIsStrictlyNegative();
    void rectfNormalizedOnSpecialValues();

    // ── 集合运算 ──
    void rectfUnitedWithNullIsAsymmetric();
    void rectfUnitedNormalizesNegativeDims();
    void rectfIntersectedOnDegenerate();
    void rectfIntersectsOnDegenerate();
    void rectfOperatorAssignMatchesNamed();
    void rectfSetOpsOnNanShortCircuit();

    // ── contains ──
    void rectfContainsPointIsClosedInterval();
    void rectfContainsPointOnNegativeDims();
    void rectfContainsRectIsNotIntersects();
    void rectfContainsOnNan();

    // ── 取整：toRect vs toAlignedRect ──
    void rectfToRectRoundsFourEdges();
    void rectfToAlignedRectExpandsOutward();
    void rectfToAlignedRectCeilDoesNotBumpOnExactInteger();
    void rectfToRectAndToAlignedRectDiffer();
    void rectfToAlignedRectDoesNotNormalize();

    // ── 提升与相等 ──
    void rectfFromPkRectUsesWidthNotRight();
    void rectfEqualityIsFuzzy();
    void rectfInequalityMatchesNegation();
    void rectfEqualityIsMacroProof();

    // ── 跨切面 ──
    void rectfNoexceptSurfaceMatchesQt();
    void rectfConstexprSurfaceMatchesQt();
};

#undef Q_SLOTS
#undef Q_OBJECT
