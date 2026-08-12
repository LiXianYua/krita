#pragma once

// PkTransform 的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
// 本地等价展开、不走 pk/test/compat/QObject 的理由与 rectf_case.h 相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkTransformCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── 枚举与布局 ──
    void transformEnumIsBitFlagsNotZeroToFive();
    void transformLayoutIsNineQrealPlusCache();

    // ── 三个构造：矩阵值 **与缓存初值** ──
    void transformDefaultIsIdentity();
    void transformNineArgCtorTakesRowsOfThree();
    void transformSixArgCtorLeavesProjectiveRowIdentity();
    void transformCtorsSeedDifferentDirtyLevels();

    // ── 行向量约定（搞反了每个 map 都错）──
    void transformMapUsesRowVectorConvention();
    void transformGetterNamesMatchStorage();
    void transformDeterminantIsThirdOrderExpansion();

    // ── type()：六档 + dot 判据 ──
    void transformTypeLadderCoversAllSixLevels();
    void transformTypeSplitsRotateFromShearByDotProduct();
    void transformTypeAfterRotateRoundTripIsNone();

    // ── type() 的惰性缓存**是可观测语义** ──
    void transformTypeCacheCanGoStaleAndItShows();
    void transformStaleCacheAlsoDrivesMapAndIsAffine();
    void transformStaleCacheSurvivesCopy();
    void transformSetMatrixAndResetClearTheCache();

    // ── rotate ──
    void transformRotateRightAnglesAreExact();
    void transformRotateMinus180IsNotSpecialCased();
    void transformRotateZeroReturnsEarly();
    void transformRotateRadiansHasNoSpecialCaseAndNoEarlyReturn();
    void transformRotateDispatchesOnCurrentType();
    void transformRotateAboutYAndXAxisGoesProjective();

    // ── translate / scale / shear 的分档 ──
    void transformTranslateDispatchesOnCurrentType();
    void transformScaleDispatchesOnCurrentType();
    void transformShearDispatchesOnCurrentType();
    void transformMutatorsReturnEarlyOnNeutralArgs();
    void transformFromTranslateAndFromScalePinTheCache();

    // ── 乘法 ──
    void transformMultiplicationOrderMatters();
    void transformMultiplyReturnsEarlyOnIdentitySide();
    void transformProjectiveProductUsesFullNineTerms();
    void transformStarAndStarEqualsDifferOnNegativeZeroDy();

    // ── inverted：三条路径 + 失败路径 ──
    void transformInvertedTranslatePath();
    void transformInvertedScalePathUsesFuzzyNullPerAxis();
    void transformInvertedAffinePathUsesExactZeroDeterminant();
    void transformInvertedProjectivePathUsesAdjointOverDet();
    void transformInvertedFailureReturnsIdentityAndDropsType();
    void transformInvertedKeepsTypeOnSuccess();

    // ── 标量运算符 ──
    void transformScalarOperatorsMatchQt();
    void transformScalarOperatorsReturnEarlyOnNeutral();
    void transformDivideIsMultiplyByReciprocal();
    void transformFreeScalarOperatorsMatchCompound();

    // ── map 的四个重载：夹持与不夹持 ──
    void transformMapPointOverloadsDoNotClampW();
    void transformMapPointerOverloadsClampW();
    void transformMapOverloadsAgreeOnAffineMatrices();
    void transformFreePointOperatorsForwardToMap();

    // ── mapRect ──
    void transformMapRectTranslateFastPathRoundsTheOffset();
    void transformMapRectScaleFastPathFlipsNegativeExtent();
    void transformMapRectIntegerUsesRightPlusOne();
    void transformMapRectFloatHasNoOffByOne();
    void transformMapRectPerspectiveClipIsADeclaredGap();

    // ── 相等 ──
    void transformEqualityIsExactNotFuzzy();
    void transformEqualityIgnoresCacheState();
    void transformFuzzyCompareIsTheFuzzyOne();

    // ── transposed / reset ──
    void transformTransposedSwapsAcrossDiagonal();

    // ── 跨切面 ──
    void transformNoexceptSurfaceMatchesQt();
    void transformConstexprSurfaceIsGatedByText();
};

#undef Q_SLOTS
#undef Q_OBJECT
