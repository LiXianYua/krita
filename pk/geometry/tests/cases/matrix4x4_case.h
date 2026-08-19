#pragma once

// PkMatrix4x4 的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkMatrix4x4Case : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── 构造 / 布局 / 元素访问 ──
    void defaultCtorIsIdentity();
    void ctorFromTransformPromotes3x3();
    void operatorParenIsColumnMajor();
    void isIdentityAndSetToIdentity();

    // ── 平移 / 缩放 ──
    void translateXy();
    void translateXyz();
    void scaleFactor();
    void scaleXyz();

    // ── 旋转（绕坐标轴 + 绕任意向量）──
    void rotateZAxis();
    void rotateXAxis();
    void rotateArbitraryAxis();

    // ── 求逆（含退化 / 平移 / 缩放快速路径）──
    void invertedOfIdentityIsIdentity();
    void invertedOfTranslation();
    void invertedOfScale();
    void invertedOfGeneralIsRoundTrip();
    void invertedOfSingularReturnsIdentity();

    // ── 矩阵乘向量 / 矩阵乘矩阵 ──
    void mapVector3D();
    void mapVector4D();
    void matrixMultiplyAssociates();

    // ── toTransform 降回 3×3 ──
    void toTransformDropsThirdRowColumn();
    void toTransformWithDistance();
};
