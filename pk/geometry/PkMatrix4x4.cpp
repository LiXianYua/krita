#include "PkMatrix4x4.h"

// ⚠ **这个系统头必须在 oracle/geometry_difftest.cpp 顶部的系统头区里也出现过**
// —— 那份对拍把本 .cpp `#include` 进 `namespace pkoracle {}` 里，理由与
// PkLine.cpp/PkRect.cpp 顶部同一条纪律。<cmath> 是 std::cos/std::sin/std::sqrt
// 要的。
#include <cmath>

// ---------------------------------------------------------------------------
// 4×4 矩阵的 out-of-line 成员。**逐字抄自上游 qtbase 标签 v5.15.7-lts-lgpl 的
// src/gui/math3d/qmatrix4x4.cpp**（本机装的 Qt 只有 .so，源码取自上游同版本
// 标签）。行列式/求逆用 double 中间量（float 存储、double 计算），与真 Qt 一致。
// ---------------------------------------------------------------------------

static const float pk_m4_inv_dist_to_plane = 1.0f / 1024.0f;

static inline double pkMatrixDet2(const double m[4][4], int col0, int col1, int row0, int row1)
{
    return m[col0][row0] * m[col1][row1] - m[col0][row1] * m[col1][row0];
}

// 3×3 子矩阵行列式：det(M) = A*(EI-HF) - B*(DI-GF) + C*(DH-GE)
static inline double pkMatrixDet3(const double m[4][4], int col0, int col1, int col2,
                                  int row0, int row1, int row2)
{
    return m[col0][row0] * pkMatrixDet2(m, col1, col2, row1, row2)
         - m[col1][row0] * pkMatrixDet2(m, col0, col2, row1, row2)
         + m[col2][row0] * pkMatrixDet2(m, col0, col1, row1, row2);
}

static inline double pkMatrixDet4(const double m[4][4])
{
    double det;
    det  = m[0][0] * pkMatrixDet3(m, 1, 2, 3, 1, 2, 3);
    det -= m[1][0] * pkMatrixDet3(m, 0, 2, 3, 1, 2, 3);
    det += m[2][0] * pkMatrixDet3(m, 0, 1, 3, 1, 2, 3);
    det -= m[3][0] * pkMatrixDet3(m, 0, 1, 2, 1, 2, 3);
    return det;
}

static inline void pkCopyToDoubles(const float m[4][4], double mm[4][4])
{
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            mm[i][j] = double(m[i][j]);
}

// qmatrix4x4.cpp:62 —— "1" 表示不加载单位阵。
PkMatrix4x4::PkMatrix4x4(int)
{
}

// qmatrix4x4.cpp:237-265 —— 3×3 → 4×4 提升。
PkMatrix4x4::PkMatrix4x4(const PkTransform &transform)
{
    m[0][0] = float(transform.m11());
    m[0][1] = float(transform.m12());
    m[0][2] = 0.0f;
    m[0][3] = float(transform.m13());
    m[1][0] = float(transform.m21());
    m[1][1] = float(transform.m22());
    m[1][2] = 0.0f;
    m[1][3] = float(transform.m23());
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 1.0f;
    m[2][3] = 0.0f;
    m[3][0] = float(transform.dx());
    m[3][1] = float(transform.dy());
    m[3][2] = 0.0f;
    m[3][3] = float(transform.m33());
    flagBits = General;
}

// qmatrix4x4.cpp:413-510 —— 4×4 求逆（伴随矩阵法，double 中间量）。快速路径
// 依次：单位阵 / 纯平移 / 平移+缩放 / 正交 / 3×3 仿射 / 全 4×4。
PkMatrix4x4 PkMatrix4x4::inverted(bool *invertible) const
{
    if (flagBits == Identity) {
        if (invertible)
            *invertible = true;
        return PkMatrix4x4();
    } else if (flagBits == Translation) {
        PkMatrix4x4 inv(1);
        inv.m[3][0] = -m[3][0];
        inv.m[3][1] = -m[3][1];
        inv.m[3][2] = -m[3][2];
        inv.flagBits = Translation;
        if (invertible)
            *invertible = true;
        return inv;
    } else if (flagBits < Rotation2D) {
        // Translation | Scale
        if (m[0][0] == 0 || m[1][1] == 0 || m[2][2] == 0) {
            if (invertible)
                *invertible = false;
            return PkMatrix4x4();
        }
        PkMatrix4x4 inv(1);
        inv.m[0][0] = 1.0f / m[0][0];
        inv.m[1][1] = 1.0f / m[1][1];
        inv.m[2][2] = 1.0f / m[2][2];
        inv.m[3][0] = -m[3][0] * inv.m[0][0];
        inv.m[3][1] = -m[3][1] * inv.m[1][1];
        inv.m[3][2] = -m[3][2] * inv.m[2][2];
        inv.flagBits = flagBits;

        if (invertible)
            *invertible = true;
        return inv;
    } else if ((flagBits & ~(Translation | Rotation2D | Rotation)) == Identity) {
        if (invertible)
            *invertible = true;
        return orthonormalInverse();
    } else if (flagBits < Perspective) {
        PkMatrix4x4 inv(1);

        double mm[4][4];
        pkCopyToDoubles(m, mm);

        double det = pkMatrixDet3(mm, 0, 1, 2, 0, 1, 2);
        if (det == 0.0f) {
            if (invertible)
                *invertible = false;
            return PkMatrix4x4();
        }
        det = 1.0f / det;

        inv.m[0][0] =  pkMatrixDet2(mm, 1, 2, 1, 2) * det;
        inv.m[0][1] = -pkMatrixDet2(mm, 0, 2, 1, 2) * det;
        inv.m[0][2] =  pkMatrixDet2(mm, 0, 1, 1, 2) * det;
        inv.m[0][3] = 0;
        inv.m[1][0] = -pkMatrixDet2(mm, 1, 2, 0, 2) * det;
        inv.m[1][1] =  pkMatrixDet2(mm, 0, 2, 0, 2) * det;
        inv.m[1][2] = -pkMatrixDet2(mm, 0, 1, 0, 2) * det;
        inv.m[1][3] = 0;
        inv.m[2][0] =  pkMatrixDet2(mm, 1, 2, 0, 1) * det;
        inv.m[2][1] = -pkMatrixDet2(mm, 0, 2, 0, 1) * det;
        inv.m[2][2] =  pkMatrixDet2(mm, 0, 1, 0, 1) * det;
        inv.m[2][3] = 0;
        inv.m[3][0] = -inv.m[0][0] * m[3][0] - inv.m[1][0] * m[3][1] - inv.m[2][0] * m[3][2];
        inv.m[3][1] = -inv.m[0][1] * m[3][0] - inv.m[1][1] * m[3][1] - inv.m[2][1] * m[3][2];
        inv.m[3][2] = -inv.m[0][2] * m[3][0] - inv.m[1][2] * m[3][1] - inv.m[2][2] * m[3][2];
        inv.m[3][3] = 1;
        inv.flagBits = flagBits;

        if (invertible)
            *invertible = true;
        return inv;
    }

    PkMatrix4x4 inv(1);

    double mm[4][4];
    pkCopyToDoubles(m, mm);

    double det = pkMatrixDet4(mm);
    if (det == 0.0f) {
        if (invertible)
            *invertible = false;
        return PkMatrix4x4();
    }
    det = 1.0f / det;

    inv.m[0][0] =  pkMatrixDet3(mm, 1, 2, 3, 1, 2, 3) * det;
    inv.m[0][1] = -pkMatrixDet3(mm, 0, 2, 3, 1, 2, 3) * det;
    inv.m[0][2] =  pkMatrixDet3(mm, 0, 1, 3, 1, 2, 3) * det;
    inv.m[0][3] = -pkMatrixDet3(mm, 0, 1, 2, 1, 2, 3) * det;
    inv.m[1][0] = -pkMatrixDet3(mm, 1, 2, 3, 0, 2, 3) * det;
    inv.m[1][1] =  pkMatrixDet3(mm, 0, 2, 3, 0, 2, 3) * det;
    inv.m[1][2] = -pkMatrixDet3(mm, 0, 1, 3, 0, 2, 3) * det;
    inv.m[1][3] =  pkMatrixDet3(mm, 0, 1, 2, 0, 2, 3) * det;
    inv.m[2][0] =  pkMatrixDet3(mm, 1, 2, 3, 0, 1, 3) * det;
    inv.m[2][1] = -pkMatrixDet3(mm, 0, 2, 3, 0, 1, 3) * det;
    inv.m[2][2] =  pkMatrixDet3(mm, 0, 1, 3, 0, 1, 3) * det;
    inv.m[2][3] = -pkMatrixDet3(mm, 0, 1, 2, 0, 1, 3) * det;
    inv.m[3][0] = -pkMatrixDet3(mm, 1, 2, 3, 0, 1, 2) * det;
    inv.m[3][1] =  pkMatrixDet3(mm, 0, 2, 3, 0, 1, 2) * det;
    inv.m[3][2] = -pkMatrixDet3(mm, 0, 1, 3, 0, 1, 2) * det;
    inv.m[3][3] =  pkMatrixDet3(mm, 0, 1, 2, 0, 1, 2) * det;
    inv.flagBits = flagBits;

    if (invertible)
        *invertible = true;
    return inv;
}

// qmatrix4x4.cpp:1913-1967 —— 正交矩阵的逆 = 转置。
PkMatrix4x4 PkMatrix4x4::orthonormalInverse() const
{
    PkMatrix4x4 result(1);

    result.m[0][0] = m[0][0];
    result.m[1][0] = m[0][1];
    result.m[2][0] = m[0][2];

    result.m[0][1] = m[1][0];
    result.m[1][1] = m[1][1];
    result.m[2][1] = m[1][2];

    result.m[0][2] = m[2][0];
    result.m[1][2] = m[2][1];
    result.m[2][2] = m[2][2];

    result.m[0][3] = 0.0f;
    result.m[1][3] = 0.0f;
    result.m[2][3] = 0.0f;

    result.m[3][0] = -(result.m[0][0] * m[3][0] + result.m[1][0] * m[3][1] + result.m[2][0] * m[3][2]);
    result.m[3][1] = -(result.m[0][1] * m[3][0] + result.m[1][1] * m[3][1] + result.m[2][1] * m[3][2]);
    result.m[3][2] = -(result.m[0][2] * m[3][0] + result.m[1][2] * m[3][1] + result.m[2][2] * m[3][2]);
    result.m[3][3] = 1.0f;

    result.flagBits = flagBits;

    return result;
}

// qmatrix4x4.cpp:827-985 —— 缩放（四重载）。
void PkMatrix4x4::scale(const PkVector3D &vector)
{
    float vx = vector.x();
    float vy = vector.y();
    float vz = vector.z();
    if (flagBits < Scale) {
        m[0][0] = vx;
        m[1][1] = vy;
        m[2][2] = vz;
    } else if (flagBits < Rotation2D) {
        m[0][0] *= vx;
        m[1][1] *= vy;
        m[2][2] *= vz;
    } else if (flagBits < Rotation) {
        m[0][0] *= vx;
        m[0][1] *= vx;
        m[1][0] *= vy;
        m[1][1] *= vy;
        m[2][2] *= vz;
    } else {
        m[0][0] *= vx;
        m[0][1] *= vx;
        m[0][2] *= vx;
        m[0][3] *= vx;
        m[1][0] *= vy;
        m[1][1] *= vy;
        m[1][2] *= vy;
        m[1][3] *= vy;
        m[2][0] *= vz;
        m[2][1] *= vz;
        m[2][2] *= vz;
        m[2][3] *= vz;
    }
    flagBits |= Scale;
}

void PkMatrix4x4::scale(float x, float y)
{
    if (flagBits < Scale) {
        m[0][0] = x;
        m[1][1] = y;
    } else if (flagBits < Rotation2D) {
        m[0][0] *= x;
        m[1][1] *= y;
    } else if (flagBits < Rotation) {
        m[0][0] *= x;
        m[0][1] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
    } else {
        m[0][0] *= x;
        m[0][1] *= x;
        m[0][2] *= x;
        m[0][3] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
        m[1][2] *= y;
        m[1][3] *= y;
    }
    flagBits |= Scale;
}

void PkMatrix4x4::scale(float x, float y, float z)
{
    if (flagBits < Scale) {
        m[0][0] = x;
        m[1][1] = y;
        m[2][2] = z;
    } else if (flagBits < Rotation2D) {
        m[0][0] *= x;
        m[1][1] *= y;
        m[2][2] *= z;
    } else if (flagBits < Rotation) {
        m[0][0] *= x;
        m[0][1] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
        m[2][2] *= z;
    } else {
        m[0][0] *= x;
        m[0][1] *= x;
        m[0][2] *= x;
        m[0][3] *= x;
        m[1][0] *= y;
        m[1][1] *= y;
        m[1][2] *= y;
        m[1][3] *= y;
        m[2][0] *= z;
        m[2][1] *= z;
        m[2][2] *= z;
        m[2][3] *= z;
    }
    flagBits |= Scale;
}

void PkMatrix4x4::scale(float factor)
{
    if (flagBits < Scale) {
        m[0][0] = factor;
        m[1][1] = factor;
        m[2][2] = factor;
    } else if (flagBits < Rotation2D) {
        m[0][0] *= factor;
        m[1][1] *= factor;
        m[2][2] *= factor;
    } else if (flagBits < Rotation) {
        m[0][0] *= factor;
        m[0][1] *= factor;
        m[1][0] *= factor;
        m[1][1] *= factor;
        m[2][2] *= factor;
    } else {
        m[0][0] *= factor;
        m[0][1] *= factor;
        m[0][2] *= factor;
        m[0][3] *= factor;
        m[1][0] *= factor;
        m[1][1] *= factor;
        m[1][2] *= factor;
        m[1][3] *= factor;
        m[2][0] *= factor;
        m[2][1] *= factor;
        m[2][2] *= factor;
        m[2][3] *= factor;
    }
    flagBits |= Scale;
}

// qmatrix4x4.cpp:989-1103 —— 平移（三重载）。
void PkMatrix4x4::translate(const PkVector3D &vector)
{
    float vx = vector.x();
    float vy = vector.y();
    float vz = vector.z();
    if (flagBits == Identity) {
        m[3][0] = vx;
        m[3][1] = vy;
        m[3][2] = vz;
    } else if (flagBits == Translation) {
        m[3][0] += vx;
        m[3][1] += vy;
        m[3][2] += vz;
    } else if (flagBits == Scale) {
        m[3][0] = m[0][0] * vx;
        m[3][1] = m[1][1] * vy;
        m[3][2] = m[2][2] * vz;
    } else if (flagBits == (Translation | Scale)) {
        m[3][0] += m[0][0] * vx;
        m[3][1] += m[1][1] * vy;
        m[3][2] += m[2][2] * vz;
    } else if (flagBits < Rotation) {
        m[3][0] += m[0][0] * vx + m[1][0] * vy;
        m[3][1] += m[0][1] * vx + m[1][1] * vy;
        m[3][2] += m[2][2] * vz;
    } else {
        m[3][0] += m[0][0] * vx + m[1][0] * vy + m[2][0] * vz;
        m[3][1] += m[0][1] * vx + m[1][1] * vy + m[2][1] * vz;
        m[3][2] += m[0][2] * vx + m[1][2] * vy + m[2][2] * vz;
        m[3][3] += m[0][3] * vx + m[1][3] * vy + m[2][3] * vz;
    }
    flagBits |= Translation;
}

void PkMatrix4x4::translate(float x, float y)
{
    if (flagBits == Identity) {
        m[3][0] = x;
        m[3][1] = y;
    } else if (flagBits == Translation) {
        m[3][0] += x;
        m[3][1] += y;
    } else if (flagBits == Scale) {
        m[3][0] = m[0][0] * x;
        m[3][1] = m[1][1] * y;
    } else if (flagBits == (Translation | Scale)) {
        m[3][0] += m[0][0] * x;
        m[3][1] += m[1][1] * y;
    } else if (flagBits < Rotation) {
        m[3][0] += m[0][0] * x + m[1][0] * y;
        m[3][1] += m[0][1] * x + m[1][1] * y;
    } else {
        m[3][0] += m[0][0] * x + m[1][0] * y;
        m[3][1] += m[0][1] * x + m[1][1] * y;
        m[3][2] += m[0][2] * x + m[1][2] * y;
        m[3][3] += m[0][3] * x + m[1][3] * y;
    }
    flagBits |= Translation;
}

void PkMatrix4x4::translate(float x, float y, float z)
{
    if (flagBits == Identity) {
        m[3][0] = x;
        m[3][1] = y;
        m[3][2] = z;
    } else if (flagBits == Translation) {
        m[3][0] += x;
        m[3][1] += y;
        m[3][2] += z;
    } else if (flagBits == Scale) {
        m[3][0] = m[0][0] * x;
        m[3][1] = m[1][1] * y;
        m[3][2] = m[2][2] * z;
    } else if (flagBits == (Translation | Scale)) {
        m[3][0] += m[0][0] * x;
        m[3][1] += m[1][1] * y;
        m[3][2] += m[2][2] * z;
    } else if (flagBits < Rotation) {
        m[3][0] += m[0][0] * x + m[1][0] * y;
        m[3][1] += m[0][1] * x + m[1][1] * y;
        m[3][2] += m[2][2] * z;
    } else {
        m[3][0] += m[0][0] * x + m[1][0] * y + m[2][0] * z;
        m[3][1] += m[0][1] * x + m[1][1] * y + m[2][1] * z;
        m[3][2] += m[0][2] * x + m[1][2] * y + m[2][2] * z;
        m[3][3] += m[0][3] * x + m[1][3] * y + m[2][3] * z;
    }
    flagBits |= Translation;
}

// qmatrix4x4.cpp:1105-1310 —— 绕向量旋转。
void PkMatrix4x4::rotate(float angle, const PkVector3D &vector)
{
    rotate(angle, vector.x(), vector.y(), vector.z());
}

void PkMatrix4x4::rotate(float angle, float x, float y, float z)
{
    if (angle == 0.0f)
        return;
    float c, s;
    if (angle == 90.0f || angle == -270.0f) {
        s = 1.0f;
        c = 0.0f;
    } else if (angle == -90.0f || angle == 270.0f) {
        s = -1.0f;
        c = 0.0f;
    } else if (angle == 180.0f || angle == -180.0f) {
        s = 0.0f;
        c = -1.0f;
    } else {
        float a = angle * float(M_PI / 180);   // qDegreesToRadians(float)
        c = std::cos(a);
        s = std::sin(a);
    }
    if (x == 0.0f) {
        if (y == 0.0f) {
            if (z != 0.0f) {
                // 绕 Z 轴。
                if (z < 0)
                    s = -s;
                float tmp;
                m[0][0] = (tmp = m[0][0]) * c + m[1][0] * s;
                m[1][0] = m[1][0] * c - tmp * s;
                m[0][1] = (tmp = m[0][1]) * c + m[1][1] * s;
                m[1][1] = m[1][1] * c - tmp * s;
                m[0][2] = (tmp = m[0][2]) * c + m[1][2] * s;
                m[1][2] = m[1][2] * c - tmp * s;
                m[0][3] = (tmp = m[0][3]) * c + m[1][3] * s;
                m[1][3] = m[1][3] * c - tmp * s;

                flagBits |= Rotation2D;
                return;
            }
        } else if (z == 0.0f) {
            // 绕 Y 轴。
            if (y < 0)
                s = -s;
            float tmp;
            m[2][0] = (tmp = m[2][0]) * c + m[0][0] * s;
            m[0][0] = m[0][0] * c - tmp * s;
            m[2][1] = (tmp = m[2][1]) * c + m[0][1] * s;
            m[0][1] = m[0][1] * c - tmp * s;
            m[2][2] = (tmp = m[2][2]) * c + m[0][2] * s;
            m[0][2] = m[0][2] * c - tmp * s;
            m[2][3] = (tmp = m[2][3]) * c + m[0][3] * s;
            m[0][3] = m[0][3] * c - tmp * s;

            flagBits |= Rotation;
            return;
        }
    } else if (y == 0.0f && z == 0.0f) {
        // 绕 X 轴。
        if (x < 0)
            s = -s;
        float tmp;
        m[1][0] = (tmp = m[1][0]) * c + m[2][0] * s;
        m[2][0] = m[2][0] * c - tmp * s;
        m[1][1] = (tmp = m[1][1]) * c + m[2][1] * s;
        m[2][1] = m[2][1] * c - tmp * s;
        m[1][2] = (tmp = m[1][2]) * c + m[2][2] * s;
        m[2][2] = m[2][2] * c - tmp * s;
        m[1][3] = (tmp = m[1][3]) * c + m[2][3] * s;
        m[2][3] = m[2][3] * c - tmp * s;

        flagBits |= Rotation;
        return;
    }

    double len = double(x) * double(x) +
                 double(y) * double(y) +
                 double(z) * double(z);
    if (!pkQtFuzzyCompare(len, 1.0) && !pkQtFuzzyIsNull(len)) {
        len = std::sqrt(len);
        x = float(double(x) / len);
        y = float(double(y) / len);
        z = float(double(z) / len);
    }
    float ic = 1.0f - c;
    PkMatrix4x4 rot(1);
    rot.m[0][0] = x * x * ic + c;
    rot.m[1][0] = x * y * ic - z * s;
    rot.m[2][0] = x * z * ic + y * s;
    rot.m[3][0] = 0.0f;
    rot.m[0][1] = y * x * ic + z * s;
    rot.m[1][1] = y * y * ic + c;
    rot.m[2][1] = y * z * ic - x * s;
    rot.m[3][1] = 0.0f;
    rot.m[0][2] = x * z * ic - y * s;
    rot.m[1][2] = y * z * ic + x * s;
    rot.m[2][2] = z * z * ic + c;
    rot.m[3][2] = 0.0f;
    rot.m[0][3] = 0.0f;
    rot.m[1][3] = 0.0f;
    rot.m[2][3] = 0.0f;
    rot.m[3][3] = 1.0f;
    rot.flagBits = Rotation;
    *this *= rot;
}

// qmatrix4x4.cpp:1697-1747 —— 降回 3×3。
PkTransform PkMatrix4x4::toTransform() const
{
    return PkTransform(qreal(m[0][0]), qreal(m[0][1]), qreal(m[0][3]),
                       qreal(m[1][0]), qreal(m[1][1]), qreal(m[1][3]),
                       qreal(m[3][0]), qreal(m[3][1]), qreal(m[3][3]));
}

PkTransform PkMatrix4x4::toTransform(float distanceToPlane) const
{
    if (distanceToPlane == 1024.0f) {
        return PkTransform(qreal(m[0][0]), qreal(m[0][1]), qreal(m[0][3] - m[0][2] * pk_m4_inv_dist_to_plane),
                           qreal(m[1][0]), qreal(m[1][1]), qreal(m[1][3] - m[1][2] * pk_m4_inv_dist_to_plane),
                           qreal(m[3][0]), qreal(m[3][1]), qreal(m[3][3] - m[3][2] * pk_m4_inv_dist_to_plane));
    } else if (distanceToPlane != 0.0f) {
        float d = 1.0f / distanceToPlane;
        return PkTransform(qreal(m[0][0]), qreal(m[0][1]), qreal(m[0][3] - m[0][2] * d),
                           qreal(m[1][0]), qreal(m[1][1]), qreal(m[1][3] - m[1][2] * d),
                           qreal(m[3][0]), qreal(m[3][1]), qreal(m[3][3] - m[3][2] * d));
    } else {
        return PkTransform(qreal(m[0][0]), qreal(m[0][1]), qreal(m[0][3]),
                           qreal(m[1][0]), qreal(m[1][1]), qreal(m[1][3]),
                           qreal(m[3][0]), qreal(m[3][1]), qreal(m[3][3]));
    }
}

// qmatrix4x4.h:431-528 —— 矩阵复合赋值（私有 helper，rotate 的 Rodrigues 用它）。
void PkMatrix4x4::operator*=(const PkMatrix4x4 &o)
{
    const PkMatrix4x4 other = o;
    flagBits |= other.flagBits;

    if (flagBits < Rotation2D) {
        m[3][0] += m[0][0] * other.m[3][0];
        m[3][1] += m[1][1] * other.m[3][1];
        m[3][2] += m[2][2] * other.m[3][2];

        m[0][0] *= other.m[0][0];
        m[1][1] *= other.m[1][1];
        m[2][2] *= other.m[2][2];
        return;
    }

    float m0, m1, m2;
    m0 = m[0][0] * other.m[0][0] + m[1][0] * other.m[0][1] + m[2][0] * other.m[0][2] + m[3][0] * other.m[0][3];
    m1 = m[0][0] * other.m[1][0] + m[1][0] * other.m[1][1] + m[2][0] * other.m[1][2] + m[3][0] * other.m[1][3];
    m2 = m[0][0] * other.m[2][0] + m[1][0] * other.m[2][1] + m[2][0] * other.m[2][2] + m[3][0] * other.m[2][3];
    m[3][0] = m[0][0] * other.m[3][0] + m[1][0] * other.m[3][1] + m[2][0] * other.m[3][2] + m[3][0] * other.m[3][3];
    m[0][0] = m0;
    m[1][0] = m1;
    m[2][0] = m2;

    m0 = m[0][1] * other.m[0][0] + m[1][1] * other.m[0][1] + m[2][1] * other.m[0][2] + m[3][1] * other.m[0][3];
    m1 = m[0][1] * other.m[1][0] + m[1][1] * other.m[1][1] + m[2][1] * other.m[1][2] + m[3][1] * other.m[1][3];
    m2 = m[0][1] * other.m[2][0] + m[1][1] * other.m[2][1] + m[2][1] * other.m[2][2] + m[3][1] * other.m[2][3];
    m[3][1] = m[0][1] * other.m[3][0] + m[1][1] * other.m[3][1] + m[2][1] * other.m[3][2] + m[3][1] * other.m[3][3];
    m[0][1] = m0;
    m[1][1] = m1;
    m[2][1] = m2;

    m0 = m[0][2] * other.m[0][0] + m[1][2] * other.m[0][1] + m[2][2] * other.m[0][2] + m[3][2] * other.m[0][3];
    m1 = m[0][2] * other.m[1][0] + m[1][2] * other.m[1][1] + m[2][2] * other.m[1][2] + m[3][2] * other.m[1][3];
    m2 = m[0][2] * other.m[2][0] + m[1][2] * other.m[2][1] + m[2][2] * other.m[2][2] + m[3][2] * other.m[2][3];
    m[3][2] = m[0][2] * other.m[3][0] + m[1][2] * other.m[3][1] + m[2][2] * other.m[3][2] + m[3][2] * other.m[3][3];
    m[0][2] = m0;
    m[1][2] = m1;
    m[2][2] = m2;

    m0 = m[0][3] * other.m[0][0] + m[1][3] * other.m[0][1] + m[2][3] * other.m[0][2] + m[3][3] * other.m[0][3];
    m1 = m[0][3] * other.m[1][0] + m[1][3] * other.m[1][1] + m[2][3] * other.m[1][2] + m[3][3] * other.m[1][3];
    m2 = m[0][3] * other.m[2][0] + m[1][3] * other.m[2][1] + m[2][3] * other.m[2][2] + m[3][3] * other.m[2][3];
    m[3][3] = m[0][3] * other.m[3][0] + m[1][3] * other.m[3][1] + m[2][3] * other.m[3][2] + m[3][3] * other.m[3][3];
    m[0][3] = m0;
    m[1][3] = m1;
    m[2][3] = m2;
}

// qmatrix4x4.h:638-727 —— 矩阵乘矩阵。
PkMatrix4x4 operator*(const PkMatrix4x4 &m1, const PkMatrix4x4 &m2)
{
    int flagBits = m1.flagBits | m2.flagBits;
    if (flagBits < PkMatrix4x4::Rotation2D) {
        PkMatrix4x4 m = m1;
        m.m[3][0] += m.m[0][0] * m2.m[3][0];
        m.m[3][1] += m.m[1][1] * m2.m[3][1];
        m.m[3][2] += m.m[2][2] * m2.m[3][2];

        m.m[0][0] *= m2.m[0][0];
        m.m[1][1] *= m2.m[1][1];
        m.m[2][2] *= m2.m[2][2];
        m.flagBits = flagBits;
        return m;
    }

    PkMatrix4x4 m(1);
    m.m[0][0] = m1.m[0][0] * m2.m[0][0] + m1.m[1][0] * m2.m[0][1] + m1.m[2][0] * m2.m[0][2] + m1.m[3][0] * m2.m[0][3];
    m.m[0][1] = m1.m[0][1] * m2.m[0][0] + m1.m[1][1] * m2.m[0][1] + m1.m[2][1] * m2.m[0][2] + m1.m[3][1] * m2.m[0][3];
    m.m[0][2] = m1.m[0][2] * m2.m[0][0] + m1.m[1][2] * m2.m[0][1] + m1.m[2][2] * m2.m[0][2] + m1.m[3][2] * m2.m[0][3];
    m.m[0][3] = m1.m[0][3] * m2.m[0][0] + m1.m[1][3] * m2.m[0][1] + m1.m[2][3] * m2.m[0][2] + m1.m[3][3] * m2.m[0][3];

    m.m[1][0] = m1.m[0][0] * m2.m[1][0] + m1.m[1][0] * m2.m[1][1] + m1.m[2][0] * m2.m[1][2] + m1.m[3][0] * m2.m[1][3];
    m.m[1][1] = m1.m[0][1] * m2.m[1][0] + m1.m[1][1] * m2.m[1][1] + m1.m[2][1] * m2.m[1][2] + m1.m[3][1] * m2.m[1][3];
    m.m[1][2] = m1.m[0][2] * m2.m[1][0] + m1.m[1][2] * m2.m[1][1] + m1.m[2][2] * m2.m[1][2] + m1.m[3][2] * m2.m[1][3];
    m.m[1][3] = m1.m[0][3] * m2.m[1][0] + m1.m[1][3] * m2.m[1][1] + m1.m[2][3] * m2.m[1][2] + m1.m[3][3] * m2.m[1][3];

    m.m[2][0] = m1.m[0][0] * m2.m[2][0] + m1.m[1][0] * m2.m[2][1] + m1.m[2][0] * m2.m[2][2] + m1.m[3][0] * m2.m[2][3];
    m.m[2][1] = m1.m[0][1] * m2.m[2][0] + m1.m[1][1] * m2.m[2][1] + m1.m[2][1] * m2.m[2][2] + m1.m[3][1] * m2.m[2][3];
    m.m[2][2] = m1.m[0][2] * m2.m[2][0] + m1.m[1][2] * m2.m[2][1] + m1.m[2][2] * m2.m[2][2] + m1.m[3][2] * m2.m[2][3];
    m.m[2][3] = m1.m[0][3] * m2.m[2][0] + m1.m[1][3] * m2.m[2][1] + m1.m[2][3] * m2.m[2][2] + m1.m[3][3] * m2.m[2][3];

    m.m[3][0] = m1.m[0][0] * m2.m[3][0] + m1.m[1][0] * m2.m[3][1] + m1.m[2][0] * m2.m[3][2] + m1.m[3][0] * m2.m[3][3];
    m.m[3][1] = m1.m[0][1] * m2.m[3][0] + m1.m[1][1] * m2.m[3][1] + m1.m[2][1] * m2.m[3][2] + m1.m[3][1] * m2.m[3][3];
    m.m[3][2] = m1.m[0][2] * m2.m[3][0] + m1.m[1][2] * m2.m[3][1] + m1.m[2][2] * m2.m[3][2] + m1.m[3][2] * m2.m[3][3];
    m.m[3][3] = m1.m[0][3] * m2.m[3][0] + m1.m[1][3] * m2.m[3][1] + m1.m[2][3] * m2.m[3][2] + m1.m[3][3] * m2.m[3][3];
    m.flagBits = flagBits;
    return m;
}

// qmatrix4x4.h:728-840 —— 矩阵乘向量。
PkVector3D operator*(const PkMatrix4x4 &matrix, const PkVector3D &vector)
{
    float x, y, z, w;
    if (matrix.flagBits == PkMatrix4x4::Identity) {
        return vector;
    } else if (matrix.flagBits < PkMatrix4x4::Rotation2D) {
        return PkVector3D(vector.x() * matrix.m[0][0] + matrix.m[3][0],
                          vector.y() * matrix.m[1][1] + matrix.m[3][1],
                          vector.z() * matrix.m[2][2] + matrix.m[3][2]);
    } else if (matrix.flagBits < PkMatrix4x4::Rotation) {
        return PkVector3D(vector.x() * matrix.m[0][0] + vector.y() * matrix.m[1][0] + matrix.m[3][0],
                          vector.x() * matrix.m[0][1] + vector.y() * matrix.m[1][1] + matrix.m[3][1],
                          vector.z() * matrix.m[2][2] + matrix.m[3][2]);
    } else {
        x = vector.x() * matrix.m[0][0] + vector.y() * matrix.m[1][0] + vector.z() * matrix.m[2][0] + matrix.m[3][0];
        y = vector.x() * matrix.m[0][1] + vector.y() * matrix.m[1][1] + vector.z() * matrix.m[2][1] + matrix.m[3][1];
        z = vector.x() * matrix.m[0][2] + vector.y() * matrix.m[1][2] + vector.z() * matrix.m[2][2] + matrix.m[3][2];
        w = vector.x() * matrix.m[0][3] + vector.y() * matrix.m[1][3] + vector.z() * matrix.m[2][3] + matrix.m[3][3];
        if (w == 1.0f)
            return PkVector3D(x, y, z);
        else
            return PkVector3D(x / w, y / w, z / w);
    }
}

PkVector3D operator*(const PkVector3D &vector, const PkMatrix4x4 &matrix)
{
    float x, y, z, w;
    x = vector.x() * matrix.m[0][0] + vector.y() * matrix.m[0][1] + vector.z() * matrix.m[0][2] + matrix.m[0][3];
    y = vector.x() * matrix.m[1][0] + vector.y() * matrix.m[1][1] + vector.z() * matrix.m[1][2] + matrix.m[1][3];
    z = vector.x() * matrix.m[2][0] + vector.y() * matrix.m[2][1] + vector.z() * matrix.m[2][2] + matrix.m[2][3];
    w = vector.x() * matrix.m[3][0] + vector.y() * matrix.m[3][1] + vector.z() * matrix.m[3][2] + matrix.m[3][3];
    if (w == 1.0f)
        return PkVector3D(x, y, z);
    else
        return PkVector3D(x / w, y / w, z / w);
}

PkVector4D operator*(const PkVector4D &vector, const PkMatrix4x4 &matrix)
{
    float x, y, z, w;
    x = vector.x() * matrix.m[0][0] + vector.y() * matrix.m[0][1] + vector.z() * matrix.m[0][2] + vector.w() * matrix.m[0][3];
    y = vector.x() * matrix.m[1][0] + vector.y() * matrix.m[1][1] + vector.z() * matrix.m[1][2] + vector.w() * matrix.m[1][3];
    z = vector.x() * matrix.m[2][0] + vector.y() * matrix.m[2][1] + vector.z() * matrix.m[2][2] + vector.w() * matrix.m[2][3];
    w = vector.x() * matrix.m[3][0] + vector.y() * matrix.m[3][1] + vector.z() * matrix.m[3][2] + vector.w() * matrix.m[3][3];
    return PkVector4D(x, y, z, w);
}

PkVector4D operator*(const PkMatrix4x4 &matrix, const PkVector4D &vector)
{
    float x, y, z, w;
    x = vector.x() * matrix.m[0][0] + vector.y() * matrix.m[1][0] + vector.z() * matrix.m[2][0] + vector.w() * matrix.m[3][0];
    y = vector.x() * matrix.m[0][1] + vector.y() * matrix.m[1][1] + vector.z() * matrix.m[2][1] + vector.w() * matrix.m[3][1];
    z = vector.x() * matrix.m[0][2] + vector.y() * matrix.m[1][2] + vector.z() * matrix.m[2][2] + vector.w() * matrix.m[3][2];
    w = vector.x() * matrix.m[0][3] + vector.y() * matrix.m[1][3] + vector.z() * matrix.m[2][3] + vector.w() * matrix.m[3][3];
    return PkVector4D(x, y, z, w);
}
