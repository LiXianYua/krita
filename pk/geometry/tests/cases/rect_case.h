#pragma once

// PkRect 的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
// 本地等价展开、不走 pk/test/compat/QObject 的理由与 point_case.h 相同
//（走那条路要求编译行有 -I pk/test/compat，会把 <QtGlobal> 的解析目标搅进来）。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkRectCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── 构造与布局 ──
    void rectDefaultIsNullSentinel();
    void rectFourConstructors();
    void rectLayoutIsFourInts();

    // ── 差一（本族最著名的坑）──
    void rectRightBottomAreOffByOne();
    void rectWidthHeightAreSpanPlusOne();

    // ── 退化矩形的三谓词 ──
    void rectThreePredicatesAreIndependent();
    void rectIsValidIsNotQSizeIsValid();

    // ── 取值器 ──
    void rectCornersAndSize();
    void rectCenterRoundsTowardTopLeft();
    void rectCenterUsesInt64Intermediate();

    // ── 修改器 ──
    void rectSetEdges();
    void rectSetXSetY();
    void rectSetTopLeftAndBottomRight();
    void rectSetWidthSetHeightAnchorTopLeft();
    void rectSetSizeAnchorsTopLeft();
    void rectSetRectAndGetRect();
    void rectSetCoordsAndGetCoords();

    // ── 平移一族 ──
    void rectMoveToKeepsSize();
    void rectMoveLeftMoveTopKeepSize();
    void rectMoveTopLeftIsTwoMoves();
    void rectMoveCenterUsesHalfSpan();
    void rectTranslateAndTranslated();

    // ── adjust ──
    void rectAdjustAndAdjusted();
    void rectAdjustedCanGoDegenerate();

    // ── normalized ──
    void rectNormalizedSwapBoundaryIsX1MinusOne();
    void rectNormalizedLeavesNullAlone();

    // ── 集合运算 ──
    void rectUnitedWithNullIsAsymmetric();
    void rectUnitedNormalizesNegativeDims();
    void rectIntersectedOnDegenerate();
    void rectIntersectsOnDegenerate();
    void rectOperatorAssignMatchesNamed();

    // ── contains ──
    void rectContainsPointBoundary();
    void rectContainsPointOnNegativeDims();
    void rectContainsRect();

    // ── 运算符与溢出 ──
    void rectEquality();
    void rectSpanOverflowWraps();

    // ── 跨切面 ──
    void rectNoexceptSurfaceMatchesQt();
    void rectConstexprSurfaceMatchesQt();
};

#undef Q_SLOTS
#undef Q_OBJECT
