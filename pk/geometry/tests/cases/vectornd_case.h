#pragma once

// PkVector2D / PkVector3D / PkVector4D 的单测类。
//
// 生成器 pk_test_moc.py 只扫 .h，且只认类体里字面出现的 "Q_OBJECT" 与
// "private Q_SLOTS:"，所以这两个 token 必须真的出现在本文件里。
// 本地等价展开、不走 pk/test/compat/QObject 的理由与 rect_case.h 相同。
#define Q_OBJECT template <typename PkTestBinderArgT> friend struct PkTestBinder;
#define Q_SLOTS

#include "../../../test/PkTest.h"

class PkVectorNdCase : public PkTestObject
{
    Q_OBJECT
private Q_SLOTS:
    // ── 构造与布局 ──
    void vec2DDefaultAndTwoArgCtor();
    void vec2DStructFromPointPointF();
    void vec3DDefaultAndCtor();
    void vec4DDefaultAndCtor();
    void layoutIsPlainFloatArray();

    // ── 分量存取 / 设置 ──
    void componentAccessors();
    void componentSetters();

    // ── isNull ──
    void isNullUsesExactZero();

    // ── length / lengthSquared（精度不对称：length double、lengthSquared float）──
    void lengthSquaredIsFloatAccumulation();
    void lengthIsDoubleAccumulation();

    // ── normalized / normalize ──
    void normalizedOfUnitVectorIsIdentity();
    void normalizedOfZeroIsZeroVector();
    void normalizeMutatesInPlace();

    // ── dotProduct（float 精度）──
    void dotProduct2D();
    void dotProduct3D();
    void dotProduct4D();

    // ── crossProduct / normal（3D）──
    void crossProductOrthogonal();
    void crossProductSelfIsZero();
    void normalOfTwoVectorsIsUnit();

    // ── 算术运算符 ──
    void arithmeticAddSubMulDiv();
    void componentWiseMulDiv();
    void unaryMinus();
    void compoundAssignments();

    // ── 转换 ──
    void toPointToPointFRound();
    void toVector2DAffinePerspectiveDivide();
    void toVector2DAffineZeroWIsZero();
    void toVector3DAffine();
    void crossTypeConversion();
};
