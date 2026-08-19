#include "cases/vectornd_case.h"
#include "../PkVectorND.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <type_traits>

#include "pk_binder_vectornd_case.inc"

// ---------------------------------------------------------------------------
// 期望值全部取自**真 Qt 5.15.7** 的行为（探针链
// /mnt/ssd-disk/liyang/projects/krita-ci-env/_install 的 libQt5Gui；float/double
// 精度不对称这条经反汇编 .so 实测确认，见 PkVectorND.h 文件头）。对齐口径与
// 前几族一致：与 Qt 的任何行为差异默认都是缺陷。
//
// **float 比较不用 PK_COMPARE**：它走 pk/test 的模糊比较（相对 1e-12），会把
// length() 的 double 累加 vs lengthSquared() 的 float 累加这两个**刻意不同**
// 的精度路径在 float 域上抹平（它们差异远小于 1e-12）。凡是要钉住 float 语义
// 的断言一律 PK_VERIFY + 显式 ==/关系式，只有 sizeof/layout 这类结构断言用
// PK_COMPARE。
// ---------------------------------------------------------------------------

static bool floatEq(float a, float b) { return a == b; }

void PkVectorNdCase::vec2DDefaultAndTwoArgCtor()
{
    PkVector2D d;
    PK_VERIFY(d.x() == 0.0f && d.y() == 0.0f);

    PkVector2D v(1.5f, -2.5f);
    PK_VERIFY(v.x() == 1.5f && v.y() == -2.5f);
}

void PkVectorNdCase::vec2DStructFromPointPointF()
{
    PkPoint ip(3, -4);
    PkVector2D vi(ip);
    PK_VERIFY(vi.x() == 3.0f && vi.y() == -4.0f);

    PkPointF fp(0.5, 1.5);
    PkVector2D vf(fp);
    PK_VERIFY(vf.x() == 0.5f && vf.y() == 1.5f);
}

void PkVectorNdCase::vec3DDefaultAndCtor()
{
    PkVector3D d;
    PK_VERIFY(d.x() == 0.0f && d.y() == 0.0f && d.z() == 0.0f);

    PkVector3D v(1.0f, 2.0f, 3.0f);
    PK_VERIFY(v.x() == 1.0f && v.y() == 2.0f && v.z() == 3.0f);

    PkVector2D v2(4.0f, 5.0f);
    PkVector3D from2(v2);
    PK_VERIFY(from2.z() == 0.0f);
    PkVector3D from2z(v2, 9.0f);
    PK_VERIFY(from2z.z() == 9.0f);
}

void PkVectorNdCase::vec4DDefaultAndCtor()
{
    PkVector4D d;
    PK_VERIFY(d.x() == 0.0f && d.y() == 0.0f && d.z() == 0.0f && d.w() == 0.0f);

    PkVector4D v(1.0f, 2.0f, 3.0f, 4.0f);
    PK_VERIFY(v.x() == 1.0f && v.y() == 2.0f && v.z() == 3.0f && v.w() == 4.0f);
}

void PkVectorNdCase::layoutIsPlainFloatArray()
{
    PK_COMPARE(sizeof(PkVector2D), (std::size_t)2 * sizeof(float));
    PK_COMPARE(sizeof(PkVector3D), (std::size_t)3 * sizeof(float));
    PK_COMPARE(sizeof(PkVector4D), (std::size_t)4 * sizeof(float));
    PK_VERIFY(std::is_trivially_copyable<PkVector2D>::value);
    PK_VERIFY(std::is_trivially_copyable<PkVector3D>::value);
    PK_VERIFY(std::is_trivially_copyable<PkVector4D>::value);
}

void PkVectorNdCase::componentAccessors()
{
    PkVector3D v(1.0f, 2.0f, 3.0f);
    PK_VERIFY(v[0] == 1.0f && v[1] == 2.0f && v[2] == 3.0f);
    const PkVector3D &cv = v;
    PK_VERIFY(cv[0] == 1.0f && cv[1] == 2.0f && cv[2] == 3.0f);
}

void PkVectorNdCase::componentSetters()
{
    PkVector4D v;
    v.setX(1.0f); v.setY(2.0f); v.setZ(3.0f); v.setW(4.0f);
    PK_VERIFY(v.x() == 1.0f && v.y() == 2.0f && v.z() == 3.0f && v.w() == 4.0f);
    v[0] = 5.0f;
    PK_VERIFY(v.x() == 5.0f);
}

void PkVectorNdCase::isNullUsesExactZero()
{
    // isNull 是**精确零**（qIsNull == 0.0f），不是 qFuzzyIsNull 的 1e-5 阈值。
    // 探针实测：qIsNull(1e-6f)=false、qFuzzyIsNull(1e-6f)=true，两者语义不同。
    PK_VERIFY(PkVector2D().isNull());
    PK_VERIFY(PkVector2D(-0.0f, 0.0f).isNull());     // ±0 都是精确零
    PK_VERIFY(!PkVector2D(1e-6f, 0.0f).isNull());    // 1e-6 不是精确零
    PK_VERIFY(PkVector3D().isNull());
    PK_VERIFY(PkVector4D().isNull());
    PK_VERIFY(!PkVector4D(0.0f, 0.0f, 0.0f, 1e-6f).isNull());
}

void PkVectorNdCase::lengthSquaredIsFloatAccumulation()
{
    // 3-4-5 直角三角形：lengthSquared = 25（float 精确）。
    PkVector2D v(3.0f, 4.0f);
    PK_VERIFY(v.lengthSquared() == 25.0f);
    PkVector3D v3(3.0f, 4.0f, 0.0f);
    PK_VERIFY(v3.lengthSquared() == 25.0f);
}

void PkVectorNdCase::lengthIsDoubleAccumulation()
{
    PkVector2D v(3.0f, 4.0f);
    PK_VERIFY(v.length() == 5.0f);
    PkVector3D v3(0.0f, 0.0f, 5.0f);
    PK_VERIFY(v3.length() == 5.0f);
    // 极大量级不溢出成 inf（double 累加）：1e19 的平方是 1e38，float 里
    // 1e19f * 1e19f 已经溢出，double 累加不溢出。真 Qt 实测 length(1e19,0)≈1e19。
    PkVector2D huge(1e19f, 0.0f);
    PK_VERIFY(!std::isinf(huge.length()));
    PK_VERIFY(huge.length() == 1e19f);
}

void PkVectorNdCase::normalizedOfUnitVectorIsIdentity()
{
    // 单位向量归一化返回 *this 原样（len≈1 分支，不做除法）。
    PkVector2D u(1.0f, 0.0f);
    PkVector2D n = u.normalized();
    PK_VERIFY(n.x() == 1.0f && n.y() == 0.0f);
}

void PkVectorNdCase::normalizedOfZeroIsZeroVector()
{
    PkVector2D z;
    PkVector2D n = z.normalized();
    PK_VERIFY(n.x() == 0.0f && n.y() == 0.0f);
}

void PkVectorNdCase::normalizeMutatesInPlace()
{
    PkVector3D v(0.0f, 0.0f, 5.0f);
    v.normalize();
    PK_VERIFY(v.x() == 0.0f && v.y() == 0.0f && v.z() == 1.0f);
}

void PkVectorNdCase::dotProduct2D()
{
    PkVector2D a(1.0f, 2.0f), b(3.0f, 4.0f);
    PK_VERIFY(PkVector2D::dotProduct(a, b) == 11.0f);  // 1*3 + 2*4
    PK_VERIFY(PkVector2D::dotProduct(a, b) == a.x()*b.x() + a.y()*b.y());
}

void PkVectorNdCase::dotProduct3D()
{
    PkVector3D a(1.0f, 2.0f, 3.0f), b(4.0f, 5.0f, 6.0f);
    PK_VERIFY(PkVector3D::dotProduct(a, b) == 32.0f);  // 4+10+18
}

void PkVectorNdCase::dotProduct4D()
{
    PkVector4D a(1.0f, 2.0f, 3.0f, 4.0f), b(5.0f, 6.0f, 7.0f, 8.0f);
    PK_VERIFY(PkVector4D::dotProduct(a, b) == 70.0f);  // 5+12+21+32
}

void PkVectorNdCase::crossProductOrthogonal()
{
    PkVector3D x(1.0f, 0.0f, 0.0f), y(0.0f, 1.0f, 0.0f);
    PkVector3D c = PkVector3D::crossProduct(x, y);
    PK_VERIFY(c.x() == 0.0f && c.y() == 0.0f && c.z() == 1.0f);
}

void PkVectorNdCase::crossProductSelfIsZero()
{
    PkVector3D v(1.0f, 2.0f, 3.0f);
    PkVector3D c = PkVector3D::crossProduct(v, v);
    PK_VERIFY(c.x() == 0.0f && c.y() == 0.0f && c.z() == 0.0f);
}

void PkVectorNdCase::normalOfTwoVectorsIsUnit()
{
    PkVector3D a(1.0f, 0.0f, 0.0f), b(0.0f, 1.0f, 0.0f);
    PkVector3D n = PkVector3D::normal(a, b);
    PK_VERIFY(n.x() == 0.0f && n.y() == 0.0f && n.z() == 1.0f);
}

void PkVectorNdCase::arithmeticAddSubMulDiv()
{
    PkVector2D a(1.0f, 2.0f), b(3.0f, 4.0f);
    PK_VERIFY((a + b).x() == 4.0f && (a + b).y() == 6.0f);
    PK_VERIFY((b - a).x() == 2.0f && (b - a).y() == 2.0f);
    PkVector2D s = a * 2.0f;
    PK_VERIFY(s.x() == 2.0f && s.y() == 4.0f);
    PkVector2D s2 = 3.0f * a;
    PK_VERIFY(s2.x() == 3.0f && s2.y() == 6.0f);
    PkVector2D d = b / 2.0f;
    PK_VERIFY(d.x() == 1.5f && d.y() == 2.0f);
}

void PkVectorNdCase::componentWiseMulDiv()
{
    PkVector2D a(2.0f, 3.0f), b(4.0f, 5.0f);
    PkVector2D m = a * b;
    PK_VERIFY(m.x() == 8.0f && m.y() == 15.0f);
    PkVector2D d = a / b;
    PK_VERIFY(d.x() == 0.5f && d.y() == 0.6f);
}

void PkVectorNdCase::unaryMinus()
{
    PkVector2D a(1.0f, -2.0f);
    PkVector2D n = -a;
    PK_VERIFY(n.x() == -1.0f && n.y() == 2.0f);
}

void PkVectorNdCase::compoundAssignments()
{
    PkVector3D v(1.0f, 2.0f, 3.0f);
    v += PkVector3D(1.0f, 1.0f, 1.0f);
    PK_VERIFY(v.x() == 2.0f && v.y() == 3.0f && v.z() == 4.0f);
    v *= 2.0f;
    PK_VERIFY(v.x() == 4.0f && v.y() == 6.0f && v.z() == 8.0f);
    v /= 2.0f;
    PK_VERIFY(v.x() == 2.0f && v.y() == 3.0f && v.z() == 4.0f);
}

void PkVectorNdCase::toPointToPointFRound()
{
    PkVector2D v(1.6f, -1.4f);
    PK_VERIFY(v.toPoint().x() == 2 && v.toPoint().y() == -1);
    // toPointF() 是 float → qreal 的宽化（qreal 是 double），结果 = (double)1.6f
    // = 1.600000023841858…，不是字面量 1.6。期望值写 qreal(1.6f) 才对齐。
    PK_VERIFY(v.toPointF().x() == qreal(1.6f) && v.toPointF().y() == qreal(-1.4f));
}

void PkVectorNdCase::toVector2DAffinePerspectiveDivide()
{
    PkVector4D v(6.0f, 8.0f, 10.0f, 2.0f);
    PkVector2D a = v.toVector2DAffine();
    PK_VERIFY(a.x() == 3.0f && a.y() == 4.0f);
}

void PkVectorNdCase::toVector2DAffineZeroWIsZero()
{
    PkVector4D v(6.0f, 8.0f, 10.0f, 0.0f);
    PkVector2D a = v.toVector2DAffine();
    PK_VERIFY(a.x() == 0.0f && a.y() == 0.0f);
}

void PkVectorNdCase::toVector3DAffine()
{
    PkVector4D v(6.0f, 8.0f, 10.0f, 2.0f);
    PkVector3D a = v.toVector3DAffine();
    PK_VERIFY(a.x() == 3.0f && a.y() == 4.0f && a.z() == 5.0f);
}

void PkVectorNdCase::crossTypeConversion()
{
    PkVector3D v3(1.0f, 2.0f, 3.0f);
    PkVector2D v2(v3);
    PK_VERIFY(v2.x() == 1.0f && v2.y() == 2.0f);
    PkVector4D v4(v3, 9.0f);
    PK_VERIFY(v4.x() == 1.0f && v4.y() == 2.0f && v4.z() == 3.0f && v4.w() == 9.0f);
    PK_VERIFY(v3.toVector2D().x() == 1.0f);
    PK_VERIFY(v3.toVector4D().w() == 0.0f);
}

int run_vectornd_tests()
{
    PkVectorNdCase tc;
    const char *argv[] = {"test_pkgeometry"};
    return PkTest::qExec(&tc, 1, const_cast<char **>(argv));
}
