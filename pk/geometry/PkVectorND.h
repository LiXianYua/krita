#ifndef PK_GEOMETRY_PKVECTORND_H
#define PK_GEOMETRY_PKVECTORND_H

#include "PkGlobal.h"
#include "PkPoint.h"

// ---------------------------------------------------------------------------
// PkVector2D / PkVector3D / PkVector4D —— QVector2D / QVector3D / QVector4D 的
// 零 Qt 替代（R-21 T3）。三个类同构（N 维 float 向量），合一个头。
//
// **逐字抄自真 Qt 5.15.7** 的 include/QtGui/qvector2d.h / qvector3d.h /
// qvector4d.h（class 声明 + inline 成员）与上游 qtbase 标签 v5.15.7-lts-lgpl 的
// src/gui/math3d/qvectornd.cpp（out-of-line 成员，本机只有 .so 没有那份源码）。
// 来源行号标在各项上方。
//
// ── ⚠ 精度不对称是照抄的语义，不是笔误 ──────────────────────────────────
//
// 反汇编真 `libQt5Gui.so.5` 实测钉死（本机没有 qvectornd.cpp 源码，直接读机器码）：
//   · `lengthSquared()` —— **float 精度**（`mulss`/`addss`）：`x*x+y*y` 在
//     float 里累加，溢出/舍入发生在 float 域。
//   · `length()` —— **double 精度**（`cvtss2sd`/`mulsd`/`addsd`/`sqrtsd`/
//     `cvtsd2ss`）：分量先升到 double、平方和开方、结果再降回 float。
//   · `dotProduct()` —— **float 精度**（`mulss`/`addss`）。
//   · `normalized()`/`normalize()` —— 各分量除以 `length()`（double 精度那版）。
// 三族一律照抄，不「顺手统一」成某一档精度——统一会让其中至少一族在极端量级
// 上与真 Qt 分家。
//
// ── 范围（判据①一项不多一项不少）────────────────────────────────────────
//
// 不实现：
//   · `operator QVariant()` / `QDataStream` 流式 / `QDebug` 流式 —— QVariant
//     是 R-06 的 PkVariant（不在 R-21 几何范围），流式实测调用点 0。
//   · `QVector3D::project`/`unproject` —— 签名吃 `QMatrix4x4`（R-21 T4），
//     且实测调用点 0（之前 grep 到的 `.project(` 全是 QEllipse/KoEllipse 的
//     投影方法，不是 QVector3D::project）。T4 交付 QMatrix4x4 后如确有调用
//     点再补。
//   · `QVector3D::crossProduct`/`normal`（静态）——实测调用点 0（grep 到的
//     `crossProduct` 全是 `KisAlgebra2D::crossProduct` 自定义函数）。**但保留
//     实现**：T4 的 `QMatrix4x4` 求逆要用 crossProduct/normal 吗？不，矩阵求逆
//     用伴随矩阵。这两个静态成员与 `distanceTo*` 系列一起按「Qt 头文件全集」
//     实现，理由与 PkRect 构造函数/运算符同一条（见该文件头）——它们在 Qt 的
//     public 面里、且实现是几行直写公式，砍掉省不了多少却让对拍的覆盖面对不上。
//     唯一按 0 用量砍掉的是 `project`/`unproject`（依赖 T4 的类型，不是几行公式）。
//   · `Qt::Initialization` 构造 —— 无类型调用点（它需要一个 `Qt::Initialization`
//     哨兵类型），不做。
// ---------------------------------------------------------------------------

class PkVector3D;
class PkVector4D;

class PkVector2D
{
public:
    // qvector2d.h:67-74 —— 构造函数。**全部只留声明、定义挪到类体外**
    // （与 PkLineF(const PkLine&) 同一处置）：初始化列表的圆括号会让
    // run_oracle.sh 规则三闸门的声明解析器 miss。
    constexpr PkVector2D();
    constexpr PkVector2D(float xpos, float ypos);
    constexpr explicit PkVector2D(const PkPoint &point);
    constexpr explicit PkVector2D(const PkPointF &point);
    explicit PkVector2D(const PkVector3D &vector);
    explicit PkVector2D(const PkVector4D &vector);

    // qvector2d.h:76 —— isNull = qIsNull(v[0]) && qIsNull(v[1])，其中
    // qIsNull 是**精确零**（==0.0f），不是 qFuzzyIsNull 的 1e-5 阈值。见
    // PkGlobal.h 里 qIsNull 的注释。
    bool isNull() const;

    constexpr float x() const;
    constexpr float y() const;
    void setX(float x);
    void setY(float y);

    float &operator[](int i);
    float operator[](int i) const;

    // qvector2d.cpp —— out-of-line，定义见 PkVectorND.cpp。
    float length() const;             // double 精度（见文件头「精度不对称」）
    float lengthSquared() const;      // float 精度
    PkVector2D normalized() const;
    void normalize();
    float distanceToPoint(const PkVector2D &point) const;
    float distanceToLine(const PkVector2D &point, const PkVector2D &direction) const;

    PkVector2D &operator+=(const PkVector2D &vector);
    PkVector2D &operator-=(const PkVector2D &vector);
    PkVector2D &operator*=(float factor);
    PkVector2D &operator*=(const PkVector2D &vector);
    PkVector2D &operator/=(float divisor);
    PkVector2D &operator/=(const PkVector2D &vector);

    static float dotProduct(const PkVector2D &v1, const PkVector2D &v2);

    friend constexpr bool operator==(const PkVector2D &v1, const PkVector2D &v2);
    friend constexpr bool operator!=(const PkVector2D &v1, const PkVector2D &v2);
    friend constexpr PkVector2D operator+(const PkVector2D &v1, const PkVector2D &v2);
    friend constexpr PkVector2D operator-(const PkVector2D &v1, const PkVector2D &v2);
    friend constexpr PkVector2D operator*(float factor, const PkVector2D &vector);
    friend constexpr PkVector2D operator*(const PkVector2D &vector, float factor);
    friend constexpr PkVector2D operator*(const PkVector2D &v1, const PkVector2D &v2);
    friend constexpr PkVector2D operator-(const PkVector2D &vector);
    friend constexpr PkVector2D operator/(const PkVector2D &vector, float divisor);
    friend constexpr PkVector2D operator/(const PkVector2D &vector, const PkVector2D &divisor);
    friend constexpr bool qFuzzyCompare(const PkVector2D &v1, const PkVector2D &v2);

    PkVector3D toVector3D() const;
    PkVector4D toVector4D() const;

    constexpr PkPoint toPoint() const;
    constexpr PkPointF toPointF() const;

private:
    float v[2];

    friend class PkVector3D;
    friend class PkVector4D;
};

class PkVector3D
{
public:
    constexpr PkVector3D();
    constexpr PkVector3D(float xpos, float ypos, float zpos);
    constexpr explicit PkVector3D(const PkPoint &point);
    constexpr explicit PkVector3D(const PkPointF &point);
    PkVector3D(const PkVector2D &vector);
    PkVector3D(const PkVector2D &vector, float zpos);
    explicit PkVector3D(const PkVector4D &vector);

    bool isNull() const;

    constexpr float x() const;
    constexpr float y() const;
    constexpr float z() const;
    void setX(float x);
    void setY(float y);
    void setZ(float z);

    float &operator[](int i);
    float operator[](int i) const;

    float length() const;             // double 精度
    float lengthSquared() const;      // float 精度
    PkVector3D normalized() const;
    void normalize();

    PkVector3D &operator+=(const PkVector3D &vector);
    PkVector3D &operator-=(const PkVector3D &vector);
    PkVector3D &operator*=(float factor);
    PkVector3D &operator*=(const PkVector3D &vector);
    PkVector3D &operator/=(float divisor);
    PkVector3D &operator/=(const PkVector3D &vector);

    static float dotProduct(const PkVector3D &v1, const PkVector3D &v2);
    static PkVector3D crossProduct(const PkVector3D &v1, const PkVector3D &v2);
    static PkVector3D normal(const PkVector3D &v1, const PkVector3D &v2);
    static PkVector3D normal(const PkVector3D &v1, const PkVector3D &v2, const PkVector3D &v3);

    float distanceToPoint(const PkVector3D &point) const;
    float distanceToPlane(const PkVector3D &plane, const PkVector3D &normal) const;
    float distanceToPlane(const PkVector3D &plane1, const PkVector3D &plane2, const PkVector3D &plane3) const;
    float distanceToLine(const PkVector3D &point, const PkVector3D &direction) const;

    friend constexpr bool operator==(const PkVector3D &v1, const PkVector3D &v2);
    friend constexpr bool operator!=(const PkVector3D &v1, const PkVector3D &v2);
    friend constexpr PkVector3D operator+(const PkVector3D &v1, const PkVector3D &v2);
    friend constexpr PkVector3D operator-(const PkVector3D &v1, const PkVector3D &v2);
    friend constexpr PkVector3D operator*(float factor, const PkVector3D &vector);
    friend constexpr PkVector3D operator*(const PkVector3D &vector, float factor);
    friend constexpr PkVector3D operator*(const PkVector3D &v1, const PkVector3D &v2);
    friend constexpr PkVector3D operator-(const PkVector3D &vector);
    friend constexpr PkVector3D operator/(const PkVector3D &vector, float divisor);
    friend constexpr PkVector3D operator/(const PkVector3D &vector, const PkVector3D &divisor);
    friend constexpr bool qFuzzyCompare(const PkVector3D &v1, const PkVector3D &v2);

    PkVector2D toVector2D() const;
    PkVector4D toVector4D() const;

    constexpr PkPoint toPoint() const;
    constexpr PkPointF toPointF() const;

private:
    float v[3];

    friend class PkVector2D;
    friend class PkVector4D;
};

class PkVector4D
{
public:
    constexpr PkVector4D();
    constexpr PkVector4D(float xpos, float ypos, float zpos, float wpos);
    constexpr explicit PkVector4D(const PkPoint &point);
    constexpr explicit PkVector4D(const PkPointF &point);
    PkVector4D(const PkVector2D &vector);
    PkVector4D(const PkVector2D &vector, float zpos, float wpos);
    PkVector4D(const PkVector3D &vector);
    PkVector4D(const PkVector3D &vector, float wpos);

    bool isNull() const;

    constexpr float x() const;
    constexpr float y() const;
    constexpr float z() const;
    constexpr float w() const;
    void setX(float x);
    void setY(float y);
    void setZ(float z);
    void setW(float w);

    float &operator[](int i);
    float operator[](int i) const;

    float length() const;             // double 精度
    float lengthSquared() const;      // float 精度
    PkVector4D normalized() const;
    void normalize();

    PkVector4D &operator+=(const PkVector4D &vector);
    PkVector4D &operator-=(const PkVector4D &vector);
    PkVector4D &operator*=(float factor);
    PkVector4D &operator*=(const PkVector4D &vector);
    PkVector4D &operator/=(float divisor);
    PkVector4D &operator/=(const PkVector4D &vector);

    static float dotProduct(const PkVector4D &v1, const PkVector4D &v2);

    friend constexpr bool operator==(const PkVector4D &v1, const PkVector4D &v2);
    friend constexpr bool operator!=(const PkVector4D &v1, const PkVector4D &v2);
    friend constexpr PkVector4D operator+(const PkVector4D &v1, const PkVector4D &v2);
    friend constexpr PkVector4D operator-(const PkVector4D &v1, const PkVector4D &v2);
    friend constexpr PkVector4D operator*(float factor, const PkVector4D &vector);
    friend constexpr PkVector4D operator*(const PkVector4D &vector, float factor);
    friend constexpr PkVector4D operator*(const PkVector4D &v1, const PkVector4D &v2);
    friend constexpr PkVector4D operator-(const PkVector4D &vector);
    friend constexpr PkVector4D operator/(const PkVector4D &vector, float divisor);
    friend constexpr PkVector4D operator/(const PkVector4D &vector, const PkVector4D &divisor);
    friend constexpr bool qFuzzyCompare(const PkVector4D &v1, const PkVector4D &v2);

    PkVector2D toVector2D() const;
    PkVector2D toVector2DAffine() const;
    PkVector3D toVector3D() const;
    PkVector3D toVector3DAffine() const;

    constexpr PkPoint toPoint() const;
    constexpr PkPointF toPointF() const;

private:
    float v[4];

    friend class PkVector2D;
    friend class PkVector3D;
};

// ══════════════════════════════════════════════════════════════════════════
// PkVector2D inline（qvector2d.h 类体内 / 类体外 inline）
// ══════════════════════════════════════════════════════════════════════════

constexpr inline PkVector2D::PkVector2D() : v{0.0f, 0.0f} {}

constexpr inline PkVector2D::PkVector2D(float xpos, float ypos) : v{xpos, ypos} {}

constexpr inline PkVector2D::PkVector2D(const PkPoint &point) : v{float(point.x()), float(point.y())} {}

constexpr inline PkVector2D::PkVector2D(const PkPointF &point) : v{float(point.x()), float(point.y())} {}

// qIsNull（精确零），不是 qFuzzyIsNull（模糊）——见 PkGlobal.h 里 qIsNull 的
// 注释：这两个名字语义不同，QVector2D::isNull 照 Qt 用精确零。
inline bool PkVector2D::isNull() const
{
    return qIsNull(v[0]) && qIsNull(v[1]);
}

constexpr inline float PkVector2D::x() const { return v[0]; }
constexpr inline float PkVector2D::y() const { return v[1]; }

inline void PkVector2D::setX(float aX) { v[0] = aX; }
inline void PkVector2D::setY(float aY) { v[1] = aY; }

inline float &PkVector2D::operator[](int i)
{
    return v[i];
}

inline float PkVector2D::operator[](int i) const
{
    return v[i];
}

inline PkVector2D &PkVector2D::operator+=(const PkVector2D &vector)
{
    v[0] += vector.v[0];
    v[1] += vector.v[1];
    return *this;
}

inline PkVector2D &PkVector2D::operator-=(const PkVector2D &vector)
{
    v[0] -= vector.v[0];
    v[1] -= vector.v[1];
    return *this;
}

inline PkVector2D &PkVector2D::operator*=(float factor)
{
    v[0] *= factor;
    v[1] *= factor;
    return *this;
}

inline PkVector2D &PkVector2D::operator*=(const PkVector2D &vector)
{
    v[0] *= vector.v[0];
    v[1] *= vector.v[1];
    return *this;
}

inline PkVector2D &PkVector2D::operator/=(float divisor)
{
    v[0] /= divisor;
    v[1] /= divisor;
    return *this;
}

inline PkVector2D &PkVector2D::operator/=(const PkVector2D &vector)
{
    v[0] /= vector.v[0];
    v[1] /= vector.v[1];
    return *this;
}

constexpr inline bool operator==(const PkVector2D &v1, const PkVector2D &v2)
{
    return v1.v[0] == v2.v[0] && v1.v[1] == v2.v[1];
}

constexpr inline bool operator!=(const PkVector2D &v1, const PkVector2D &v2)
{
    return v1.v[0] != v2.v[0] || v1.v[1] != v2.v[1];
}

constexpr inline PkVector2D operator+(const PkVector2D &v1, const PkVector2D &v2)
{
    return PkVector2D(v1.v[0] + v2.v[0], v1.v[1] + v2.v[1]);
}

constexpr inline PkVector2D operator-(const PkVector2D &v1, const PkVector2D &v2)
{
    return PkVector2D(v1.v[0] - v2.v[0], v1.v[1] - v2.v[1]);
}

constexpr inline PkVector2D operator*(float factor, const PkVector2D &vector)
{
    return PkVector2D(vector.v[0] * factor, vector.v[1] * factor);
}

constexpr inline PkVector2D operator*(const PkVector2D &vector, float factor)
{
    return PkVector2D(vector.v[0] * factor, vector.v[1] * factor);
}

constexpr inline PkVector2D operator*(const PkVector2D &v1, const PkVector2D &v2)
{
    return PkVector2D(v1.v[0] * v2.v[0], v1.v[1] * v2.v[1]);
}

constexpr inline PkVector2D operator-(const PkVector2D &vector)
{
    return PkVector2D(-vector.v[0], -vector.v[1]);
}

constexpr inline PkVector2D operator/(const PkVector2D &vector, float divisor)
{
    return PkVector2D(vector.v[0] / divisor, vector.v[1] / divisor);
}

constexpr inline PkVector2D operator/(const PkVector2D &vector, const PkVector2D &divisor)
{
    return PkVector2D(vector.v[0] / divisor.v[0], vector.v[1] / divisor.v[1]);
}

constexpr inline bool qFuzzyCompare(const PkVector2D &v1, const PkVector2D &v2)
{
    return pkQtFuzzyCompare(v1.v[0], v2.v[0]) && pkQtFuzzyCompare(v1.v[1], v2.v[1]);
}

constexpr inline PkPoint PkVector2D::toPoint() const
{
    return PkPoint(qRound(v[0]), qRound(v[1]));
}

constexpr inline PkPointF PkVector2D::toPointF() const
{
    return PkPointF(qreal(v[0]), qreal(v[1]));
}

// ══════════════════════════════════════════════════════════════════════════
// PkVector3D inline
// ══════════════════════════════════════════════════════════════════════════

constexpr inline PkVector3D::PkVector3D() : v{0.0f, 0.0f, 0.0f} {}

constexpr inline PkVector3D::PkVector3D(float xpos, float ypos, float zpos) : v{xpos, ypos, zpos} {}

constexpr inline PkVector3D::PkVector3D(const PkPoint &point) : v{float(point.x()), float(point.y()), 0.0f} {}

constexpr inline PkVector3D::PkVector3D(const PkPointF &point) : v{float(point.x()), float(point.y()), 0.0f} {}

inline bool PkVector3D::isNull() const
{
    return qIsNull(v[0]) && qIsNull(v[1]) && qIsNull(v[2]);
}

constexpr inline float PkVector3D::x() const { return v[0]; }
constexpr inline float PkVector3D::y() const { return v[1]; }
constexpr inline float PkVector3D::z() const { return v[2]; }

inline void PkVector3D::setX(float aX) { v[0] = aX; }
inline void PkVector3D::setY(float aY) { v[1] = aY; }
inline void PkVector3D::setZ(float aZ) { v[2] = aZ; }

inline float &PkVector3D::operator[](int i) { return v[i]; }
inline float PkVector3D::operator[](int i) const { return v[i]; }

inline PkVector3D &PkVector3D::operator+=(const PkVector3D &vector)
{
    v[0] += vector.v[0];
    v[1] += vector.v[1];
    v[2] += vector.v[2];
    return *this;
}

inline PkVector3D &PkVector3D::operator-=(const PkVector3D &vector)
{
    v[0] -= vector.v[0];
    v[1] -= vector.v[1];
    v[2] -= vector.v[2];
    return *this;
}

inline PkVector3D &PkVector3D::operator*=(float factor)
{
    v[0] *= factor;
    v[1] *= factor;
    v[2] *= factor;
    return *this;
}

inline PkVector3D &PkVector3D::operator*=(const PkVector3D &vector)
{
    v[0] *= vector.v[0];
    v[1] *= vector.v[1];
    v[2] *= vector.v[2];
    return *this;
}

inline PkVector3D &PkVector3D::operator/=(float divisor)
{
    v[0] /= divisor;
    v[1] /= divisor;
    v[2] /= divisor;
    return *this;
}

inline PkVector3D &PkVector3D::operator/=(const PkVector3D &vector)
{
    v[0] /= vector.v[0];
    v[1] /= vector.v[1];
    v[2] /= vector.v[2];
    return *this;
}

constexpr inline bool operator==(const PkVector3D &v1, const PkVector3D &v2)
{
    return v1.v[0] == v2.v[0] && v1.v[1] == v2.v[1] && v1.v[2] == v2.v[2];
}

constexpr inline bool operator!=(const PkVector3D &v1, const PkVector3D &v2)
{
    return v1.v[0] != v2.v[0] || v1.v[1] != v2.v[1] || v1.v[2] != v2.v[2];
}

constexpr inline PkVector3D operator+(const PkVector3D &v1, const PkVector3D &v2)
{
    return PkVector3D(v1.v[0] + v2.v[0], v1.v[1] + v2.v[1], v1.v[2] + v2.v[2]);
}

constexpr inline PkVector3D operator-(const PkVector3D &v1, const PkVector3D &v2)
{
    return PkVector3D(v1.v[0] - v2.v[0], v1.v[1] - v2.v[1], v1.v[2] - v2.v[2]);
}

constexpr inline PkVector3D operator*(float factor, const PkVector3D &vector)
{
    return PkVector3D(vector.v[0] * factor, vector.v[1] * factor, vector.v[2] * factor);
}

constexpr inline PkVector3D operator*(const PkVector3D &vector, float factor)
{
    return PkVector3D(vector.v[0] * factor, vector.v[1] * factor, vector.v[2] * factor);
}

constexpr inline PkVector3D operator*(const PkVector3D &v1, const PkVector3D &v2)
{
    return PkVector3D(v1.v[0] * v2.v[0], v1.v[1] * v2.v[1], v1.v[2] * v2.v[2]);
}

constexpr inline PkVector3D operator-(const PkVector3D &vector)
{
    return PkVector3D(-vector.v[0], -vector.v[1], -vector.v[2]);
}

constexpr inline PkVector3D operator/(const PkVector3D &vector, float divisor)
{
    return PkVector3D(vector.v[0] / divisor, vector.v[1] / divisor, vector.v[2] / divisor);
}

constexpr inline PkVector3D operator/(const PkVector3D &vector, const PkVector3D &divisor)
{
    return PkVector3D(vector.v[0] / divisor.v[0], vector.v[1] / divisor.v[1], vector.v[2] / divisor.v[2]);
}

constexpr inline bool qFuzzyCompare(const PkVector3D &v1, const PkVector3D &v2)
{
    return pkQtFuzzyCompare(v1.v[0], v2.v[0]) && pkQtFuzzyCompare(v1.v[1], v2.v[1])
        && pkQtFuzzyCompare(v1.v[2], v2.v[2]);
}

constexpr inline PkPoint PkVector3D::toPoint() const
{
    return PkPoint(qRound(v[0]), qRound(v[1]));
}

constexpr inline PkPointF PkVector3D::toPointF() const
{
    return PkPointF(qreal(v[0]), qreal(v[1]));
}

// ══════════════════════════════════════════════════════════════════════════
// PkVector4D inline
// ══════════════════════════════════════════════════════════════════════════

constexpr inline PkVector4D::PkVector4D() : v{0.0f, 0.0f, 0.0f, 0.0f} {}

constexpr inline PkVector4D::PkVector4D(float xpos, float ypos, float zpos, float wpos)
    : v{xpos, ypos, zpos, wpos} {}

constexpr inline PkVector4D::PkVector4D(const PkPoint &point)
    : v{float(point.x()), float(point.y()), 0.0f, 0.0f} {}

constexpr inline PkVector4D::PkVector4D(const PkPointF &point)
    : v{float(point.x()), float(point.y()), 0.0f, 0.0f} {}

inline bool PkVector4D::isNull() const
{
    return qIsNull(v[0]) && qIsNull(v[1]) && qIsNull(v[2]) && qIsNull(v[3]);
}

constexpr inline float PkVector4D::x() const { return v[0]; }
constexpr inline float PkVector4D::y() const { return v[1]; }
constexpr inline float PkVector4D::z() const { return v[2]; }
constexpr inline float PkVector4D::w() const { return v[3]; }

inline void PkVector4D::setX(float aX) { v[0] = aX; }
inline void PkVector4D::setY(float aY) { v[1] = aY; }
inline void PkVector4D::setZ(float aZ) { v[2] = aZ; }
inline void PkVector4D::setW(float aW) { v[3] = aW; }

inline float &PkVector4D::operator[](int i) { return v[i]; }
inline float PkVector4D::operator[](int i) const { return v[i]; }

inline PkVector4D &PkVector4D::operator+=(const PkVector4D &vector)
{
    v[0] += vector.v[0];
    v[1] += vector.v[1];
    v[2] += vector.v[2];
    v[3] += vector.v[3];
    return *this;
}

inline PkVector4D &PkVector4D::operator-=(const PkVector4D &vector)
{
    v[0] -= vector.v[0];
    v[1] -= vector.v[1];
    v[2] -= vector.v[2];
    v[3] -= vector.v[3];
    return *this;
}

inline PkVector4D &PkVector4D::operator*=(float factor)
{
    v[0] *= factor;
    v[1] *= factor;
    v[2] *= factor;
    v[3] *= factor;
    return *this;
}

inline PkVector4D &PkVector4D::operator*=(const PkVector4D &vector)
{
    v[0] *= vector.v[0];
    v[1] *= vector.v[1];
    v[2] *= vector.v[2];
    v[3] *= vector.v[3];
    return *this;
}

inline PkVector4D &PkVector4D::operator/=(float divisor)
{
    v[0] /= divisor;
    v[1] /= divisor;
    v[2] /= divisor;
    v[3] /= divisor;
    return *this;
}

inline PkVector4D &PkVector4D::operator/=(const PkVector4D &vector)
{
    v[0] /= vector.v[0];
    v[1] /= vector.v[1];
    v[2] /= vector.v[2];
    v[3] /= vector.v[3];
    return *this;
}

constexpr inline bool operator==(const PkVector4D &v1, const PkVector4D &v2)
{
    return v1.v[0] == v2.v[0] && v1.v[1] == v2.v[1] && v1.v[2] == v2.v[2] && v1.v[3] == v2.v[3];
}

constexpr inline bool operator!=(const PkVector4D &v1, const PkVector4D &v2)
{
    return v1.v[0] != v2.v[0] || v1.v[1] != v2.v[1] || v1.v[2] != v2.v[2] || v1.v[3] != v2.v[3];
}

constexpr inline PkVector4D operator+(const PkVector4D &v1, const PkVector4D &v2)
{
    return PkVector4D(v1.v[0] + v2.v[0], v1.v[1] + v2.v[1], v1.v[2] + v2.v[2], v1.v[3] + v2.v[3]);
}

constexpr inline PkVector4D operator-(const PkVector4D &v1, const PkVector4D &v2)
{
    return PkVector4D(v1.v[0] - v2.v[0], v1.v[1] - v2.v[1], v1.v[2] - v2.v[2], v1.v[3] - v2.v[3]);
}

constexpr inline PkVector4D operator*(float factor, const PkVector4D &vector)
{
    return PkVector4D(vector.v[0] * factor, vector.v[1] * factor, vector.v[2] * factor, vector.v[3] * factor);
}

constexpr inline PkVector4D operator*(const PkVector4D &vector, float factor)
{
    return PkVector4D(vector.v[0] * factor, vector.v[1] * factor, vector.v[2] * factor, vector.v[3] * factor);
}

constexpr inline PkVector4D operator*(const PkVector4D &v1, const PkVector4D &v2)
{
    return PkVector4D(v1.v[0] * v2.v[0], v1.v[1] * v2.v[1], v1.v[2] * v2.v[2], v1.v[3] * v2.v[3]);
}

constexpr inline PkVector4D operator-(const PkVector4D &vector)
{
    return PkVector4D(-vector.v[0], -vector.v[1], -vector.v[2], -vector.v[3]);
}

constexpr inline PkVector4D operator/(const PkVector4D &vector, float divisor)
{
    return PkVector4D(vector.v[0] / divisor, vector.v[1] / divisor, vector.v[2] / divisor, vector.v[3] / divisor);
}

constexpr inline PkVector4D operator/(const PkVector4D &vector, const PkVector4D &divisor)
{
    return PkVector4D(vector.v[0] / divisor.v[0], vector.v[1] / divisor.v[1], vector.v[2] / divisor.v[2], vector.v[3] / divisor.v[3]);
}

constexpr inline bool qFuzzyCompare(const PkVector4D &v1, const PkVector4D &v2)
{
    return pkQtFuzzyCompare(v1.v[0], v2.v[0]) && pkQtFuzzyCompare(v1.v[1], v2.v[1])
        && pkQtFuzzyCompare(v1.v[2], v2.v[2]) && pkQtFuzzyCompare(v1.v[3], v2.v[3]);
}

constexpr inline PkPoint PkVector4D::toPoint() const
{
    return PkPoint(qRound(v[0]), qRound(v[1]));
}

constexpr inline PkPointF PkVector4D::toPointF() const
{
    return PkPointF(qreal(v[0]), qreal(v[1]));
}

#endif // PK_GEOMETRY_PKVECTORND_H
