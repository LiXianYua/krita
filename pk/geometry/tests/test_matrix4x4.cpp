#include "cases/matrix4x4_case.h"
#include "../PkMatrix4x4.h"

#include <cmath>
#include <cstdint>
#include <type_traits>

#include "pk_binder_matrix4x4_case.inc"

// ---------------------------------------------------------------------------
// 期望值全部取自**真 Qt 5.15.7** 的行为（探针链
// /mnt/ssd-disk/liyang/projects/krita-ci-env/_install 的 libQt5Gui；列主序存储、
// 惰性 flagBits 快速路径、double 中间量求逆这三条照抄语义由对拍
// geometry_difftest.cpp 的 Matrix4x4 族逐输入钉死）。本文件只钉结构级的自洽
//（单位阵、往返、列主序布局），逐位对齐交给对拍。
// ---------------------------------------------------------------------------

void PkMatrix4x4Case::defaultCtorIsIdentity()
{
    PkMatrix4x4 m;
    PK_VERIFY(m.isIdentity());
    PK_VERIFY(m(0, 0) == 1.0f && m(1, 1) == 1.0f && m(2, 2) == 1.0f && m(3, 3) == 1.0f);
    PK_VERIFY(m(0, 1) == 0.0f && m(3, 0) == 0.0f);
}

void PkMatrix4x4Case::ctorFromTransformPromotes3x3()
{
    PkTransform t(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 5.0, -3.0, 1.0);
    PkMatrix4x4 m(t);
    PK_VERIFY(m(0, 0) == 1.0f && m(0, 1) == 0.0f);
    PK_VERIFY(m(1, 0) == 0.0f && m(1, 1) == 1.0f);
    PK_VERIFY(m(2, 2) == 1.0f);   // 齐次 w 通道
    PK_VERIFY(m(0, 3) == 5.0f && m(1, 3) == -3.0f);   // 平移 dx/dy（列主序末列）
    PK_VERIFY(m(3, 3) == 1.0f);
}

void PkMatrix4x4Case::operatorParenIsColumnMajor()
{
    PkMatrix4x4 m;
    // 列主序：m(0,1) 是第一行第二列 = m[1][0]。
    m(0, 1) = 7.0f;
    PK_VERIFY(m(0, 1) == 7.0f);
    PK_VERIFY(m(1, 0) == 0.0f);
    PK_VERIFY(m(0, 0) == 1.0f);   // 对角线不受影响
}

void PkMatrix4x4Case::isIdentityAndSetToIdentity()
{
    PkMatrix4x4 m;
    PK_VERIFY(m.isIdentity());
    m(0, 0) = 2.0f;
    PK_VERIFY(!m.isIdentity());
    m.setToIdentity();
    PK_VERIFY(m.isIdentity());
}

void PkMatrix4x4Case::translateXy()
{
    PkMatrix4x4 m;
    m.translate(5.0f, -3.0f);
    PK_VERIFY(m(0, 3) == 5.0f && m(1, 3) == -3.0f);
    PK_VERIFY(m.isIdentity() == false);
}

void PkMatrix4x4Case::translateXyz()
{
    PkMatrix4x4 m;
    m.translate(1.0f, 2.0f, 3.0f);
    PK_VERIFY(m(0, 3) == 1.0f && m(1, 3) == 2.0f && m(2, 3) == 3.0f);
}

void PkMatrix4x4Case::scaleFactor()
{
    PkMatrix4x4 m;
    m.scale(2.0f);
    PK_VERIFY(m(0, 0) == 2.0f && m(1, 1) == 2.0f && m(2, 2) == 2.0f);
}

void PkMatrix4x4Case::scaleXyz()
{
    PkMatrix4x4 m;
    m.scale(2.0f, 3.0f, 4.0f);
    PK_VERIFY(m(0, 0) == 2.0f && m(1, 1) == 3.0f && m(2, 2) == 4.0f);
}

void PkMatrix4x4Case::rotateZAxis()
{
    PkMatrix4x4 m;
    m.rotate(90.0f, 0.0f, 0.0f, 1.0f);
    // 绕 Z 轴 90 度：x 轴 (1,0) → (0,1)，y 轴 (0,1) → (-1,0)。
    PK_VERIFY(m(0, 0) < 1e-6f && m(0, 0) > -1e-6f);
    PK_VERIFY(m(1, 0) > 1.0f - 1e-6f && m(1, 0) < 1.0f + 1e-6f);  // (1,0)→(0,1)
}

void PkMatrix4x4Case::rotateXAxis()
{
    PkMatrix4x4 m;
    m.rotate(180.0f, 1.0f, 0.0f, 0.0f);
    // 绕 X 轴 180 度：y、z 翻转。
    PK_VERIFY(m(1, 1) < -1.0f + 1e-6f && m(1, 1) > -1.0f - 1e-6f);
    PK_VERIFY(m(2, 2) < -1.0f + 1e-6f && m(2, 2) > -1.0f - 1e-6f);
}

void PkMatrix4x4Case::rotateArbitraryAxis()
{
    PkMatrix4x4 m;
    m.rotate(180.0f, PkVector3D(1.0f, 0.0f, 0.0f));
    // 绕 X 轴 180 度，与 rotateXAxis 同结果（走 Rodrigues 快速路径前的特判）。
    PK_VERIFY(m(1, 1) < -1.0f + 1e-6f && m(1, 1) > -1.0f - 1e-6f);
}

void PkMatrix4x4Case::invertedOfIdentityIsIdentity()
{
    PkMatrix4x4 m;
    PkMatrix4x4 inv = m.inverted();
    PK_VERIFY(inv.isIdentity());
}

void PkMatrix4x4Case::invertedOfTranslation()
{
    PkMatrix4x4 m;
    m.translate(5.0f, -3.0f, 2.0f);
    PkMatrix4x4 inv = m.inverted();
    PK_VERIFY(inv(0, 3) == -5.0f && inv(1, 3) == 3.0f && inv(2, 3) == -2.0f);
}

void PkMatrix4x4Case::invertedOfScale()
{
    PkMatrix4x4 m;
    m.scale(2.0f, 4.0f, 8.0f);
    PkMatrix4x4 inv = m.inverted();
    PK_VERIFY(inv(0, 0) == 0.5f && inv(1, 1) == 0.25f && inv(2, 2) == 0.125f);
}

void PkMatrix4x4Case::invertedOfGeneralIsRoundTrip()
{
    // 绕 X 轴 30 度 + 平移 + 缩放，求逆后再乘回去应近似单位阵。
    PkMatrix4x4 m;
    m.rotate(30.0f, 1.0f, 0.0f, 0.0f);
    m.scale(2.0f, 3.0f, 4.0f);
    m.translate(1.0f, 2.0f, 3.0f);
    PkMatrix4x4 inv = m.inverted();
    PkMatrix4x4 prod = m * inv;
    for (int r = 0; r < 4; ++r)
        for (int c = 0; c < 4; ++c) {
            float want = (r == c) ? 1.0f : 0.0f;
            PK_VERIFY(prod(r, c) > want - 1e-4f && prod(r, c) < want + 1e-4f);
        }
}

void PkMatrix4x4Case::invertedOfSingularReturnsIdentity()
{
    PkMatrix4x4 m;
    m(0, 0) = 0.0f; m(1, 1) = 0.0f; m(2, 2) = 0.0f; m(3, 3) = 0.0f;
    bool invertible = true;
    PkMatrix4x4 inv = m.inverted(&invertible);
    PK_VERIFY(!invertible);
    PK_VERIFY(inv.isIdentity());
}

void PkMatrix4x4Case::mapVector3D()
{
    PkMatrix4x4 m;
    m.translate(1.0f, 2.0f, 3.0f);
    PkVector3D v(10.0f, 20.0f, 30.0f);
    PkVector3D r = m.map(v);
    PK_VERIFY(r.x() == 11.0f && r.y() == 22.0f && r.z() == 33.0f);
}

void PkMatrix4x4Case::mapVector4D()
{
    PkMatrix4x4 m;
    m.scale(2.0f);
    PkVector4D v(1.0f, 2.0f, 3.0f, 4.0f);
    PkVector4D r = m.map(v);
    PK_VERIFY(r.x() == 2.0f && r.y() == 4.0f && r.z() == 6.0f && r.w() == 4.0f);
}

void PkMatrix4x4Case::matrixMultiplyAssociates()
{
    PkMatrix4x4 a; a.translate(1.0f, 2.0f, 3.0f);
    PkMatrix4x4 b; b.scale(2.0f);
    PkMatrix4x4 c = a * b;
    PkVector3D v(1.0f, 1.0f, 1.0f);
    PkVector3D r = c.map(v);
    // (a*b).map(v) == a.map(b.map(v))：先缩放 (2,2,2) 再平移 (1,2,3) = (3,4,5)。
    PK_VERIFY(r.x() == 3.0f && r.y() == 4.0f && r.z() == 5.0f);
}

void PkMatrix4x4Case::toTransformDropsThirdRowColumn()
{
    PkMatrix4x4 m;
    m.translate(5.0f, -3.0f);
    m.scale(2.0f);
    PkTransform t = m.toTransform();
    PK_VERIFY(t.m11() == 2.0 && t.m22() == 2.0);
    PK_VERIFY(t.dx() == 5.0 && t.dy() == -3.0);
}

void PkMatrix4x4Case::toTransformWithDistance()
{
    PkMatrix4x4 m;
    m.translate(1.0f, 2.0f, 3.0f);
    PkTransform t = m.toTransform(1024.0f);
    PK_VERIFY(t.dx() == 1.0 && t.dy() == 2.0);
}

int run_matrix4x4_tests()
{
    PkMatrix4x4Case tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
