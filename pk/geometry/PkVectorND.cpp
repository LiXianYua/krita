#include "PkVectorND.h"

// ⚠ **这个系统头必须在 oracle/geometry_difftest.cpp 顶部的系统头区里也出现过**
// —— 那份对拍把本 .cpp `#include` 进 `namespace pkoracle {}` 里，理由与
// PkLine.cpp/PkRect.cpp 顶部同一条纪律。<cmath> 是 std::sqrt 要的。
#include <cmath>

// ---------------------------------------------------------------------------
// 三个 N 维 float 向量族的 out-of-line 成员。**逐字抄自上游 qtbase 标签
// v5.15.7-lts-lgpl 的 src/gui/math3d/qvectornd.cpp**（本机装的 Qt 只有 .so，
// 源码取自上游同版本标签；float/double 精度不对称这条经反汇编 .so 实测确认，
// 见 PkVectorND.h 文件头「精度不对称」一节）。
// ---------------------------------------------------------------------------

// ── PkVector2D ─────────────────────────────────────────────────────────────

PkVector2D::PkVector2D(const PkVector3D &vector) : v{vector.v[0], vector.v[1]} {}

PkVector2D::PkVector2D(const PkVector4D &vector) : v{vector.v[0], vector.v[1]} {}

float PkVector2D::length() const
{
    // double 精度累加（不是 naive `sqrt(x*x+y*y)` 在 float 里溢/下溢）。
    double len = double(v[0]) * double(v[0]) +
                 double(v[1]) * double(v[1]);
    return float(std::sqrt(len));
}

float PkVector2D::lengthSquared() const
{
    // float 精度累加（见文件头「精度不对称」）。
    return v[0] * v[0] + v[1] * v[1];
}

PkVector2D PkVector2D::normalized() const
{
    // 三段分支：len≈1 返回 *this 原样（不引入一次无意义的除法舍入）；len≈0
    // 返回零向量（除以 0 会产生 inf/NaN）；否则各分量除以 sqrt(len)。
    // 除法在 double 精度做（反汇编实测），不是 `*this / float(sqrt(len))`
    // 那种 float 除法。
    double len = double(v[0]) * double(v[0]) +
                 double(v[1]) * double(v[1]);
    if (pkQtFuzzyIsNull(len - 1.0f))
        return *this;
    else if (!pkQtFuzzyIsNull(len))
        return PkVector2D(float(double(v[0]) / std::sqrt(len)),
                          float(double(v[1]) / std::sqrt(len)));
    else
        return PkVector2D();
}

// ⚠ **不是** `*this = normalized()`——两者在"len≈0 的微小非零向量"上语义不同：
// normalized() 返回零向量，normalize() **保持原样**（探针实测：
// normalize(1e-20,1e-20) 仍是 (1e-20,1e-20)，normalized(1e-20,1e-20) 是 (0,0)）。
// 照上游 qvectornd.cpp 的独立实现：len≈1 或 len≈0 都提前 return（no-op），
// 否则就地除以 sqrt(len)。
void PkVector2D::normalize()
{
    double len = double(v[0]) * double(v[0]) +
                 double(v[1]) * double(v[1]);
    if (pkQtFuzzyIsNull(len - 1.0f) || pkQtFuzzyIsNull(len))
        return;

    len = std::sqrt(len);

    v[0] = float(double(v[0]) / len);
    v[1] = float(double(v[1]) / len);
}

float PkVector2D::dotProduct(const PkVector2D &v1, const PkVector2D &v2)
{
    return v1.v[0] * v2.v[0] + v1.v[1] * v2.v[1];
}

float PkVector2D::distanceToPoint(const PkVector2D &point) const
{
    return (*this - point).length();
}

float PkVector2D::distanceToLine(const PkVector2D &point, const PkVector2D &direction) const
{
    if (direction.isNull())
        return (*this - point).length();
    PkVector2D p = point + dotProduct(*this - point, direction) / direction.lengthSquared() * direction;
    return (*this - p).length();
}

PkVector3D PkVector2D::toVector3D() const
{
    return PkVector3D(v[0], v[1], 0.0f);
}

PkVector4D PkVector2D::toVector4D() const
{
    return PkVector4D(v[0], v[1], 0.0f, 0.0f);
}

// ── PkVector3D ─────────────────────────────────────────────────────────────

PkVector3D::PkVector3D(const PkVector2D &vector) : v{vector.v[0], vector.v[1], 0.0f} {}

PkVector3D::PkVector3D(const PkVector2D &vector, float zpos) : v{vector.v[0], vector.v[1], zpos} {}

PkVector3D::PkVector3D(const PkVector4D &vector) : v{vector.v[0], vector.v[1], vector.v[2]} {}

float PkVector3D::length() const
{
    double len = double(v[0]) * double(v[0]) +
                 double(v[1]) * double(v[1]) +
                 double(v[2]) * double(v[2]);
    return float(std::sqrt(len));
}

float PkVector3D::lengthSquared() const
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2];
}

PkVector3D PkVector3D::normalized() const
{
    double len = double(v[0]) * double(v[0]) +
                 double(v[1]) * double(v[1]) +
                 double(v[2]) * double(v[2]);
    if (pkQtFuzzyIsNull(len - 1.0f))
        return *this;
    else if (!pkQtFuzzyIsNull(len))
        return PkVector3D(float(double(v[0]) / std::sqrt(len)),
                          float(double(v[1]) / std::sqrt(len)),
                          float(double(v[2]) / std::sqrt(len)));
    else
        return PkVector3D();
}

void PkVector3D::normalize()
{
    double len = double(v[0]) * double(v[0]) +
                 double(v[1]) * double(v[1]) +
                 double(v[2]) * double(v[2]);
    if (pkQtFuzzyIsNull(len - 1.0f) || pkQtFuzzyIsNull(len))
        return;

    len = std::sqrt(len);

    v[0] = float(double(v[0]) / len);
    v[1] = float(double(v[1]) / len);
    v[2] = float(double(v[2]) / len);
}

float PkVector3D::dotProduct(const PkVector3D &v1, const PkVector3D &v2)
{
    return v1.v[0] * v2.v[0] + v1.v[1] * v2.v[1] + v1.v[2] * v2.v[2];
}

PkVector3D PkVector3D::crossProduct(const PkVector3D &v1, const PkVector3D &v2)
{
    return PkVector3D(v1.v[1] * v2.v[2] - v1.v[2] * v2.v[1],
                      v1.v[2] * v2.v[0] - v1.v[0] * v2.v[2],
                      v1.v[0] * v2.v[1] - v1.v[1] * v2.v[0]);
}

PkVector3D PkVector3D::normal(const PkVector3D &v1, const PkVector3D &v2)
{
    return crossProduct(v1, v2).normalized();
}

PkVector3D PkVector3D::normal(const PkVector3D &v1, const PkVector3D &v2, const PkVector3D &v3)
{
    return crossProduct((v2 - v1), (v3 - v1)).normalized();
}

float PkVector3D::distanceToPoint(const PkVector3D &point) const
{
    return (*this - point).length();
}

float PkVector3D::distanceToPlane(const PkVector3D &plane, const PkVector3D &normal) const
{
    return dotProduct(*this - plane, normal) / normal.length();
}

float PkVector3D::distanceToPlane(const PkVector3D &plane1, const PkVector3D &plane2, const PkVector3D &plane3) const
{
    PkVector3D n = normal(plane2 - plane1, plane3 - plane1);
    return dotProduct(*this - plane1, n) / n.length();
}

float PkVector3D::distanceToLine(const PkVector3D &point, const PkVector3D &direction) const
{
    if (direction.isNull())
        return (*this - point).length();
    PkVector3D p = point + dotProduct(*this - point, direction) / direction.lengthSquared() * direction;
    return (*this - p).length();
}

PkVector2D PkVector3D::toVector2D() const
{
    return PkVector2D(v[0], v[1]);
}

PkVector4D PkVector3D::toVector4D() const
{
    return PkVector4D(v[0], v[1], v[2], 0.0f);
}

// ── PkVector4D ─────────────────────────────────────────────────────────────

PkVector4D::PkVector4D(const PkVector2D &vector) : v{vector.v[0], vector.v[1], 0.0f, 0.0f} {}

PkVector4D::PkVector4D(const PkVector2D &vector, float zpos, float wpos)
    : v{vector.v[0], vector.v[1], zpos, wpos} {}

PkVector4D::PkVector4D(const PkVector3D &vector) : v{vector.v[0], vector.v[1], vector.v[2], 0.0f} {}

PkVector4D::PkVector4D(const PkVector3D &vector, float wpos)
    : v{vector.v[0], vector.v[1], vector.v[2], wpos} {}

float PkVector4D::length() const
{
    double len = double(v[0]) * double(v[0]) +
                 double(v[1]) * double(v[1]) +
                 double(v[2]) * double(v[2]) +
                 double(v[3]) * double(v[3]);
    return float(std::sqrt(len));
}

float PkVector4D::lengthSquared() const
{
    return v[0] * v[0] + v[1] * v[1] + v[2] * v[2] + v[3] * v[3];
}

PkVector4D PkVector4D::normalized() const
{
    double len = double(v[0]) * double(v[0]) +
                 double(v[1]) * double(v[1]) +
                 double(v[2]) * double(v[2]) +
                 double(v[3]) * double(v[3]);
    if (pkQtFuzzyIsNull(len - 1.0f))
        return *this;
    else if (!pkQtFuzzyIsNull(len))
        return PkVector4D(float(double(v[0]) / std::sqrt(len)),
                          float(double(v[1]) / std::sqrt(len)),
                          float(double(v[2]) / std::sqrt(len)),
                          float(double(v[3]) / std::sqrt(len)));
    else
        return PkVector4D();
}

void PkVector4D::normalize()
{
    double len = double(v[0]) * double(v[0]) +
                 double(v[1]) * double(v[1]) +
                 double(v[2]) * double(v[2]) +
                 double(v[3]) * double(v[3]);
    if (pkQtFuzzyIsNull(len - 1.0f) || pkQtFuzzyIsNull(len))
        return;

    len = std::sqrt(len);

    v[0] = float(double(v[0]) / len);
    v[1] = float(double(v[1]) / len);
    v[2] = float(double(v[2]) / len);
    v[3] = float(double(v[3]) / len);
}

float PkVector4D::dotProduct(const PkVector4D &v1, const PkVector4D &v2)
{
    return v1.v[0] * v2.v[0] + v1.v[1] * v2.v[1] + v1.v[2] * v2.v[2] + v1.v[3] * v2.v[3];
}

PkVector2D PkVector4D::toVector2D() const
{
    return PkVector2D(v[0], v[1]);
}

// qIsNull（精确零），不是 qFuzzyIsNull——见 PkGlobal.h 里 qIsNull 的注释。
// 探针实测：w=1e-6f 时真 Qt 走除法分支（qIsNull(1e-6)=false），不是零向量。
PkVector2D PkVector4D::toVector2DAffine() const
{
    if (qIsNull(v[3]))
        return PkVector2D();
    return PkVector2D(v[0] / v[3], v[1] / v[3]);
}

PkVector3D PkVector4D::toVector3D() const
{
    return PkVector3D(v[0], v[1], v[2]);
}

PkVector3D PkVector4D::toVector3DAffine() const
{
    if (qIsNull(v[3]))
        return PkVector3D();
    return PkVector3D(v[0] / v[3], v[1] / v[3], v[2] / v[3]);
}

// ── 只在 TU 里落得了地的编译期断言 ──────────────────────────────────────

static_assert(sizeof(PkVector2D) == 2 * sizeof(float), "PkVector2D 必须是两个 float");
static_assert(sizeof(PkVector3D) == 3 * sizeof(float), "PkVector3D 必须是三个 float");
static_assert(sizeof(PkVector4D) == 4 * sizeof(float), "PkVector4D 必须是四个 float");
