#ifndef PK_GEOMETRY_PKMATRIX4X4_H
#define PK_GEOMETRY_PKMATRIX4X4_H

#include "PkGlobal.h"
#include "PkPoint.h"
#include "PkVectorND.h"
#include "PkTransform.h"

// ---------------------------------------------------------------------------
// PkMatrix4x4 —— QMatrix4x4 的零 Qt 替代（R-21 T4）。
//
// **逐字抄自真 Qt 5.15.7** 的 include/QtGui/qmatrix4x4.h（class 声明 + inline
// 成员）与上游 qtbase 标签 v5.15.7-lts-lgpl 的 src/gui/math3d/qmatrix4x4.cpp
//（out-of-line 成员，本机只有 .so 没有那份源码，源码取自上游同版本标签）。
// 来源行号标在各项上方。
//
// ── 存储与 flagBits ───────────────────────────────────────────────────────
//
// `float m[4][4]` **列主序**（column-major，与 OpenGL 一致），`int flagBits`
// 是惰性分类位（Identity/Translation/Scale/Rotation2D/Rotation/Perspective/
// General）。惰性分类是**可观测语义**：`inverted()`/`map()`/`rotate()` 都在
// flagBits 上走不同的快速路径，改错一位整族在极端输入上与真 Qt 分家——与
// PkTransform 的惰性 m_type 是同一条纪律。
//
// ── 范围（判据①一项不多一项不少）────────────────────────────────────────
//
// 只实现**真实调用点**（见 plan.md T4「补充实测」）：
//   · 构造：默认（单位阵）、`PkMatrix4x4(const PkTransform&)`（真实调用点
//     `kis_perspective_transform_strategy.cpp:146,542`、
//     `tool_transform_args.cc:287`、`kis_transform_utils.cpp:179,195`）。
//   · `operator()(row,col)`：`KisColorimetryUtils.cpp` 的 matrixFromColumns 与
//     bradford/inverseBradford/s_xyzToDolbyLMS 四个 static 初始化 lambda 用
//     它逐元素填矩阵。
//   · `inverted(bool*)`：真实调用点 ≥5 处（`s_inverseDolbyLMS = ...inverted()`、
//     `m_fromXYZ(m_toXYZ.inverted())` 等）——4×4 伴随矩阵求逆，见 .cpp 顶部。
//   · `map(PkVector3D)`：`bradford.map(destinationWhitepoint.asVector())` 等。
//   · `map(PkVector4D)` / `operator*(矩阵, 向量)`：透视变换的中间量
//     （`realMatrix * v`、`unprojectedMatrix * v`）。
//   · `rotate(angle, PkVector3D)`：`P.rotate(angle, QVector3D(1,0,0))` 等。
//   · `scale(factor)`：`tool_transform_args.cc:627` `m.scale(scale)`。
//   · `toTransform()` / `toTransform(distance)`：`P.toTransform(cameraPos.z())`、
//     `m.toTransform(distance)`。
//   · `operator*(矩阵, 矩阵)`：`QMatrix4x4(m.T) * m.P * QMatrix4x4(...)` 链乘。
//
// **明确不实现**（真实调用点 0，逐个 grep 核实过）：`determinant()`（公开版，
// inverted 内部有私有 static helper）、`transposed`/`normalMatrix`/`isAffine`/
// `column`/`row`/`setColumn`/`setRow`/`fill`/`data`/`constData`/`optimize`/
// `copyDataTo`/`mapRect`/`mapVector`/`ortho`/`frustum`/`perspective`/`lookAt`/
// `viewport`/`flipCoordinates`/`operator QVariant()`/`QDataStream`/`QDebug`/
// `QGenericMatrix` 模板族/`QQuaternion` 旋转。这些里 `.data()`/`.fill()` 的
// grep 命中数看着大，实测全是 `QSharedPointer::data()`/`QVector::fill()`/
// `QImage::fill()` 等其它类型，QMatrix4x4 接收者一个都没有。
// ---------------------------------------------------------------------------

class PkMatrix4x4
{
public:
    // qmatrix4x4.cpp:73 —— Qt 原文 `inline QMatrix4x4() { setToIdentity(); }`。
    inline PkMatrix4x4();

    // qmatrix4x4.cpp:237-265 —— 3×3 齐次矩阵提升成 4×4：m11/m12/m13 填第一行、
    // m21/m22/m23 填第二行、m31/m32/m33 填最后一行（dx/dy 进 m[3][0]/m[3][1]），
    // 第三行第三列是 1.0（齐次 w 通道）。真实调用点见文件头。
    explicit PkMatrix4x4(const PkTransform &transform);

    // qmatrix4x4.h:272-278 —— 逐元素访问（列主序：`m[col][row]`）。非 const 版
    // 会把 flagBits 钉成 General（直接改元素后惰性分类失效）。
    inline const float &operator()(int row, int column) const;
    inline float &operator()(int row, int column);

    // qmatrix4x4.h:328-343 —— 精确 float 比较（不是 qFuzzyCompare）。
    inline bool isIdentity() const;

    // qmatrix4x4.h:345-367。
    inline void setToIdentity();

    // qmatrix4x4.cpp:413-510 —— 4×4 求逆，见 .cpp 顶部。
    PkMatrix4x4 inverted(bool *invertible = nullptr) const;

    // qmatrix4x4.cpp:827-985 —— 乘进缩放（四个重载）。
    void scale(const PkVector3D &vector);
    void scale(float x, float y);
    void scale(float x, float y, float z);
    void scale(float factor);

    // qmatrix4x4.cpp:989-1103 —— 乘进平移（三个重载）。
    void translate(const PkVector3D &vector);
    void translate(float x, float y);
    void translate(float x, float y, float z);

    // qmatrix4x4.cpp:1105-1310 —— 绕向量/轴旋转（Rodrigues，含整 90/180 度与
    // 三根坐标轴的快速路径）。
    void rotate(float angle, const PkVector3D &vector);
    void rotate(float angle, float x, float y, float z = 0.0f);

    // qmatrix4x4.cpp:1697-1747 —— 降回 3×3。distanceToPlane=0 直接丢第三行第三列；
    // 非零按透视投影预乘 `|1 0 0 0| |0 1 0 0| |0 0 1 0| |0 0 d 1|`（d=-1/distance）
    // 再丢第三行第三列。
    PkTransform toTransform() const;
    PkTransform toTransform(float distanceToPlane) const;

    // qmatrix4x4.h:1041-1085 —— 都转发到自由 operator*（矩阵 × 向量）。
    inline PkVector3D map(const PkVector3D &point) const;
    inline PkVector4D map(const PkVector4D &point) const;

    // qmatrix4x4.h:638-727 —— 矩阵乘矩阵（惰性快速路径 + 全 4×4 展开）。
    friend PkMatrix4x4 operator*(const PkMatrix4x4 &m1, const PkMatrix4x4 &m2);
    // qmatrix4x4.h:728-840 —— 矩阵乘向量（w==1 免除法，否则透视除法）。
    friend PkVector3D operator*(const PkMatrix4x4 &matrix, const PkVector3D &vector);
    friend PkVector3D operator*(const PkVector3D &vector, const PkMatrix4x4 &matrix);
    friend PkVector4D operator*(const PkVector4D &vector, const PkMatrix4x4 &matrix);
    friend PkVector4D operator*(const PkMatrix4x4 &matrix, const PkVector4D &vector);

    // qmatrix4x4.h:552 —— 精确 float 逐元素比较（成员函数，不是自由函数）。
    inline bool operator==(const PkMatrix4x4 &other) const;
    inline bool operator!=(const PkMatrix4x4 &other) const;

private:
    float m[4][4];          // 列主序（与 OpenGL 一致）
    int flagBits;

    enum {
        Identity        = 0x0000, // 单位阵
        Translation     = 0x0001, // 含平移
        Scale           = 0x0002, // 含缩放
        Rotation2D      = 0x0004, // 含绕 Z 轴旋转
        Rotation        = 0x0008, // 含任意旋转
        Perspective     = 0x0010, // 最后一行 ≠ (0,0,0,1)
        General         = 0x001f  // 未知内容
    };

    // qmatrix4x4.cpp —— "1" 表示**不**加载单位阵（inverted/transposed 等内部的
    // 快速路径用它避免先 setToIdentity 再覆盖的开销）。
    explicit PkMatrix4x4(int);

    // qmatrix4x4.cpp:1913-1967 —— 正交矩阵的转置即逆。
    PkMatrix4x4 orthonormalInverse() const;

    // 内部：rotate() 的 Rodrigues 旋转结果要 `*this *= rot`，这个 operator*= 是
    // 私有 helper（qmatrix4x4.h:431-528 的公开版实现，本类不对外声明矩阵复合
    // 赋值——真实调用点 0）。
    void operator*=(const PkMatrix4x4 &other);
};

// ══════════════════════════════════════════════════════════════════════════
// PkMatrix4x4 inline
// ══════════════════════════════════════════════════════════════════════════

inline PkMatrix4x4::PkMatrix4x4()
{
    setToIdentity();
}

inline const float &PkMatrix4x4::operator()(int aRow, int aColumn) const
{
    return m[aColumn][aRow];
}

inline float &PkMatrix4x4::operator()(int aRow, int aColumn)
{
    flagBits = General;
    return m[aColumn][aRow];
}

inline bool PkMatrix4x4::isIdentity() const
{
    if (flagBits == Identity)
        return true;
    if (m[0][0] != 1.0f || m[0][1] != 0.0f || m[0][2] != 0.0f)
        return false;
    if (m[0][3] != 0.0f || m[1][0] != 0.0f || m[1][1] != 1.0f)
        return false;
    if (m[1][2] != 0.0f || m[1][3] != 0.0f || m[2][0] != 0.0f)
        return false;
    if (m[2][1] != 0.0f || m[2][2] != 1.0f || m[2][3] != 0.0f)
        return false;
    if (m[3][0] != 0.0f || m[3][1] != 0.0f || m[3][2] != 0.0f)
        return false;
    return (m[3][3] == 1.0f);
}

inline void PkMatrix4x4::setToIdentity()
{
    m[0][0] = 1.0f;
    m[0][1] = 0.0f;
    m[0][2] = 0.0f;
    m[0][3] = 0.0f;
    m[1][0] = 0.0f;
    m[1][1] = 1.0f;
    m[1][2] = 0.0f;
    m[1][3] = 0.0f;
    m[2][0] = 0.0f;
    m[2][1] = 0.0f;
    m[2][2] = 1.0f;
    m[2][3] = 0.0f;
    m[3][0] = 0.0f;
    m[3][1] = 0.0f;
    m[3][2] = 0.0f;
    m[3][3] = 1.0f;
    flagBits = Identity;
}

inline PkVector3D PkMatrix4x4::map(const PkVector3D &point) const
{
    return *this * point;
}

inline PkVector4D PkMatrix4x4::map(const PkVector4D &point) const
{
    return *this * point;
}

inline bool PkMatrix4x4::operator==(const PkMatrix4x4 &other) const
{
    return m[0][0] == other.m[0][0] &&
           m[0][1] == other.m[0][1] &&
           m[0][2] == other.m[0][2] &&
           m[0][3] == other.m[0][3] &&
           m[1][0] == other.m[1][0] &&
           m[1][1] == other.m[1][1] &&
           m[1][2] == other.m[1][2] &&
           m[1][3] == other.m[1][3] &&
           m[2][0] == other.m[2][0] &&
           m[2][1] == other.m[2][1] &&
           m[2][2] == other.m[2][2] &&
           m[2][3] == other.m[2][3] &&
           m[3][0] == other.m[3][0] &&
           m[3][1] == other.m[3][1] &&
           m[3][2] == other.m[3][2] &&
           m[3][3] == other.m[3][3];
}

inline bool PkMatrix4x4::operator!=(const PkMatrix4x4 &other) const
{
    return !(*this == other);
}

#endif // PK_GEOMETRY_PKMATRIX4X4_H
