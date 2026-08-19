#include "PkTransform.h"

// ⚠ **这两个系统头必须在 oracle/geometry_difftest.cpp 顶部的系统头区里也出现过**
// —— 那份对拍把本 .cpp `#include` 进 `namespace pkoracle {}` 里，头文件守卫已经
// 点掉的 include 才会空转；没出现过的话会造出 pkoracle::std（与 PkSize.cpp /
// PkRect.cpp 顶部同一条纪律）。两个都在（<cmath> 与 <type_traits>）。
// <cmath> 是 rotate / rotateRadians 的 std::sin / std::cos 要的。
#include <cmath>
#include <type_traits>

// ---------------------------------------------------------------------------
// PkTransform 的 out-of-line 部分。**逐字抄自 qtbase 标签 v5.15.7-lts-lgpl 的
// src/gui/painting/qtransform.cpp**（本机装的 Qt 只有 .so；源码取自上游同版本
// 标签，取值靠 oracle/ 逐输入对拍核对）。来源行号标在各项上方。
//
// 三处与 Qt 的形态差异（**都不构成行为差异**，完整说明在 PkTransform.h 头部）：
//   ① affine 的六个分量摊成 m_11/m_12/m_21/m_22/m_dx/m_dy，不用 QMatrix；
//      `affine.inverted(&inv)` 就地展开成 invertedAffine()（qmatrix.cpp:947-965）
//   ② 不留 Qt5 那个恒为 nullptr 的 `Private *d`
//   ③ **不复刻 `#ifndef QT_NO_DEBUG` 的七处 NaN 早退分支** —— 实测本机
//      libQt5Gui.so 是带 QT_NO_DEBUG 编的（探针 §B），那些分支不在里面
//
// 一处**真实行为偏离**（登记在 oracle/geometry.deviation 与 README）：
//   · `mapRect` 在「TxProject 且需要透视裁剪」时 Qt 改走 QPainterPath，
//     本类落回四角包围盒。见 mapRect 上方那段。
// ---------------------------------------------------------------------------

// qtransform.cpp:64 —— qreal 是 double，所以取 0.000001 那一支。
// 这个夹持**只作用于 PK_MAP**（map(int*)/map(qreal*)/mapRect），
// map(PkPoint)/map(PkPointF) 自己那份 1/w **没有**夹持。
#define PK_NEAR_CLIP 0.000001

// qtransform.cpp:69-99 —— MAP 宏。写成宏而不是函数是照抄：它依赖调用点作用域里
// 那个叫 `t` 的局部变量（Qt 就是这么用的），改成函数就得多传一个参数，
// 而多传参数本身没问题、但「哪一档走哪条公式」的对应关系照抄最不容易错。
// **文件末尾 #undef**：本 .cpp 会被 oracle 的 TU `#include` 进 namespace，
// 宏泄漏出去会污染后面的翻译单元。
#define PK_MAP(x, y, nx, ny) \
    do { \
        qreal FX_ = x; \
        qreal FY_ = y; \
        switch (t) { \
        case TxNone: \
            nx = FX_; \
            ny = FY_; \
            break; \
        case TxTranslate: \
            nx = FX_ + m_dx; \
            ny = FY_ + m_dy; \
            break; \
        case TxScale: \
            nx = m_11 * FX_ + m_dx; \
            ny = m_22 * FY_ + m_dy; \
            break; \
        case TxRotate: \
        case TxShear: \
        case TxProject: \
            nx = m_11 * FX_ + m_21 * FY_ + m_dx; \
            ny = m_12 * FX_ + m_22 * FY_ + m_dy; \
            if (t == TxProject) { \
                qreal w = (m_13 * FX_ + m_23 * FY_ + m_33); \
                if (w < qreal(PK_NEAR_CLIP)) w = qreal(PK_NEAR_CLIP); \
                w = 1. / w; \
                nx *= w; \
                ny *= w; \
            } \
        } \
    } while (0)

// ── 构造 ───────────────────────────────────────────────────────────────────

// qtransform.cpp:284-293。m_type 与 m_dirty **都是 TxNone**：默认构造出来的
// 单位阵不需要重算。
PkTransform::PkTransform()
    : m_11(1.), m_12(0.)
    , m_21(0.), m_22(1.)
    , m_dx(0.), m_dy(0.)
    , m_13(0), m_23(0), m_33(1)
    , m_type(TxNone)
    , m_dirty(TxNone)
{
}

// qtransform.cpp:303-314。m_dirty = **TxProject** —— 九参构造什么都可能是，
// 第一次 type() 从最高档全量重算。
PkTransform::PkTransform(qreal h11, qreal h12, qreal h13,
                         qreal h21, qreal h22, qreal h23,
                         qreal h31, qreal h32, qreal h33)
    : m_11(h11), m_12(h12)
    , m_21(h21), m_22(h22)
    , m_dx(h31), m_dy(h32)
    , m_13(h13), m_23(h23), m_33(h33)
    , m_type(TxNone)
    , m_dirty(TxProject)
{
}

// qtransform.cpp:323-333。m_dirty = **TxShear** —— 六参构造摸不到 m13/m23/m33，
// 于是 type() 的重算**跳过投影那一档**（switch 从 TxShear 进）。
// 抄成 TxProject 不会错到取值上（m13/m23/m33 就是 0/0/1），但那是运气不是理由。
PkTransform::PkTransform(qreal h11, qreal h12, qreal h21,
                         qreal h22, qreal dx, qreal dy)
    : m_11(h11), m_12(h12)
    , m_21(h21), m_22(h22)
    , m_dx(dx), m_dy(dy)
    , m_13(0), m_23(0), m_33(1)
    , m_type(TxNone)
    , m_dirty(TxShear)
{
}

// qtransform.h:175-186 —— 私有十参构造，m_dirty = TxProject。
PkTransform::PkTransform(qreal h11, qreal h12, qreal h13,
                         qreal h21, qreal h22, qreal h23,
                         qreal h31, qreal h32, qreal h33, bool)
    : m_11(h11), m_12(h12)
    , m_21(h21), m_22(h22)
    , m_dx(h31), m_dy(h32)
    , m_13(h13), m_23(h23), m_33(h33)
    , m_type(TxNone)
    , m_dirty(TxProject)
{
}

// qtransform.h:187-196 加 qmatrix.h:122-128 —— **单位阵**，且 m_dirty=TxNone。
// operator* 里 `t.m_dy += ...` 那个 `+=` 的左值初值（+0.0）就来自这里。
PkTransform::PkTransform(bool)
    : m_11(1.), m_12(0.)
    , m_21(0.), m_22(1.)
    , m_dx(0.), m_dy(0.)
    , m_13(0), m_23(0), m_33(1)
    , m_type(TxNone)
    , m_dirty(TxNone)
{
}

// ── type()：有状态的重算 ───────────────────────────────────────────────────

// qtransform.cpp:2135-2177。
// ⚠ 三条都别改：
//   · 第一行的短路 `m_dirty < m_type` 会返回**过期**的档位（实测可观测，
//     说明在 PkTransform.h 头部「惰性缓存」一节）；
//   · switch 是**从 m_dirty 那一档往下贯穿**的，不是从最高档开始 ——
//     所以 m_dirty 抄错了取值就错；
//   · dot 是 `m11*m21 + m12*m22`（**不是** m11*m12 + m21*m22）。
//     实测 t(2,1,-1,2,0,0) 的 dot = 2*(-1)+1*2 = 0 → TxRotate，
//     而 t(2,1,2,1,0,0) 的 dot = 4+1 = 5 → TxShear（探针 §D5）。
// 用 pkQtFuzzyIsNull 而不是 qFuzzyIsNull：后者在「pk/test 垫片先进 TU」的路径上
// 是个 #define，会把这里的门槛静默换成 pk/test 那套（理由见 PkGlobal.h）。
PkTransform::TransformationType PkTransform::type() const
{
    if (m_dirty == TxNone || m_dirty < m_type)
        return static_cast<TransformationType>(m_type);

    switch (static_cast<TransformationType>(m_dirty)) {
    case TxProject:
        if (!pkQtFuzzyIsNull(m_13) || !pkQtFuzzyIsNull(m_23) || !pkQtFuzzyIsNull(m_33 - 1)) {
            m_type = TxProject;
            break;
        }
        [[fallthrough]];   // Q_FALLTHROUGH() 的 C++17 等价物
    case TxShear:
    case TxRotate:
        if (!pkQtFuzzyIsNull(m_12) || !pkQtFuzzyIsNull(m_21)) {
            const qreal dot = m_11 * m_21 + m_12 * m_22;
            if (pkQtFuzzyIsNull(dot))
                m_type = TxRotate;
            else
                m_type = TxShear;
            break;
        }
        [[fallthrough]];   // Q_FALLTHROUGH() 的 C++17 等价物
    case TxScale:
        if (!pkQtFuzzyIsNull(m_11 - 1) || !pkQtFuzzyIsNull(m_22 - 1)) {
            m_type = TxScale;
            break;
        }
        [[fallthrough]];   // Q_FALLTHROUGH() 的 C++17 等价物
    case TxTranslate:
        if (!pkQtFuzzyIsNull(m_dx) || !pkQtFuzzyIsNull(m_dy)) {
            m_type = TxTranslate;
            break;
        }
        [[fallthrough]];   // Q_FALLTHROUGH() 的 C++17 等价物
    case TxNone:
        m_type = TxNone;
        break;
    }

    m_dirty = TxNone;
    return static_cast<TransformationType>(m_type);
}

// ── setMatrix / reset ──────────────────────────────────────────────────────

// qtransform.cpp:1923-1932
void PkTransform::setMatrix(qreal m11, qreal m12, qreal m13,
                            qreal m21, qreal m22, qreal m23,
                            qreal m31, qreal m32, qreal m33)
{
    m_11 = m11; m_12 = m12; m_13 = m13;
    m_21 = m21; m_22 = m22; m_23 = m23;
    m_dx = m31; m_dy = m32; m_33 = m33;
    m_type = TxNone;
    m_dirty = TxProject;
}

// qtransform.cpp:1082-1088
void PkTransform::reset()
{
    m_11 = m_22 = m_33 = 1.0;
    m_12 = m_13 = m_21 = m_23 = m_dx = m_dy = 0;
    m_type = TxNone;
    m_dirty = TxNone;
}

// ── adjoint / transposed / inverted ────────────────────────────────────────

// qtransform.cpp:359-377。九个余子式的展开顺序逐字照抄。
PkTransform PkTransform::adjoint() const
{
    qreal h11, h12, h13,
        h21, h22, h23,
        h31, h32, h33;
    h11 = m_22 * m_33 - m_23 * m_dy;
    h21 = m_23 * m_dx - m_21 * m_33;
    h31 = m_21 * m_dy - m_22 * m_dx;
    h12 = m_13 * m_dy - m_12 * m_33;
    h22 = m_11 * m_33 - m_13 * m_dx;
    h32 = m_12 * m_dx - m_11 * m_dy;
    h13 = m_12 * m_23 - m_13 * m_22;
    h23 = m_13 * m_21 - m_11 * m_23;
    h33 = m_11 * m_22 - m_12 * m_21;

    return PkTransform(h11, h12, h13,
                       h21, h22, h23,
                       h31, h32, h33, true);
}

// qtransform.cpp:382-388
PkTransform PkTransform::transposed() const
{
    PkTransform t(m_11, m_21, m_dx,
                  m_12, m_22, m_dy,
                  m_13, m_23, m_33, true);
    return t;
}

// qmatrix.cpp:947-965 就地展开 —— QTransform::inverted 的 TxRotate/TxShear
// 分支写的是 `invert.affine = affine.inverted(&inv);`，**只覆盖六个仿射分量**，
// m_13/m_23/m_33 保持 invert 那边的单位阵初值（0/0/1）。这里照同样的范围写。
// ⚠ 判据是 **`dtr == 0.0` 精确零**，不是 qFuzzyIsNull —— 与 inverted() 另外
// 两条路径的门槛不同，统一了就抹掉一整片差异。
// ⚠ 行列式是 QMatrix 的二阶式 `_m11*_m22 - _m12*_m21`（qmatrix.h:109），
// **不是** PkTransform::determinant() 那个三阶式。
void PkTransform::invertedAffine(PkTransform &out, bool *invertible) const
{
    const qreal dtr = m_11 * m_22 - m_12 * m_21;
    if (dtr == 0.0) {
        if (invertible)
            *invertible = false;
        out.m_11 = 1.; out.m_12 = 0.;
        out.m_21 = 0.; out.m_22 = 1.;
        out.m_dx = 0.; out.m_dy = 0.;
    } else {
        if (invertible)
            *invertible = true;
        const qreal dinv = 1.0 / dtr;
        out.m_11 = (m_22 * dinv);
        out.m_12 = (-m_12 * dinv);
        out.m_21 = (-m_21 * dinv);
        out.m_22 = (m_11 * dinv);
        out.m_dx = ((m_21 * m_dy - m_22 * m_dx) * dinv);
        out.m_dy = ((m_12 * m_dx - m_11 * m_dy) * dinv);
    }
}

// qtransform.cpp:400-445。
// ⚠ 四条要点，每一条都被对拍单独钉着：
//   · 失败时返回的是 **PkTransform(bool) 那个单位阵**，不是原矩阵、不是零矩阵；
//   · 失败时 **m_type/m_dirty 不从源拷贝**（`if (inv)` 之内才拷），
//     于是失败结果的 type() 是 TxNone；
//   · TxScale 一档判的是 m11/m22 **各自** qFuzzyIsNull，不是行列式；
//   · `invert = adjoint() / det` 走的是自由函数 operator/(PkTransform, qreal)，
//     它内部是 `1/det` 再乘，不是逐个除 —— 取值不同。
PkTransform PkTransform::inverted(bool *invertible) const
{
    PkTransform invert(true);
    bool inv = true;

    switch (inline_type()) {
    case TxNone:
        break;
    case TxTranslate:
        invert.m_dx = -m_dx;
        invert.m_dy = -m_dy;
        break;
    case TxScale:
        inv = !pkQtFuzzyIsNull(m_11);
        inv &= !pkQtFuzzyIsNull(m_22);
        if (inv) {
            invert.m_11 = 1. / m_11;
            invert.m_22 = 1. / m_22;
            invert.m_dx = -m_dx * invert.m_11;
            invert.m_dy = -m_dy * invert.m_22;
        }
        break;
    case TxRotate:
    case TxShear:
        invertedAffine(invert, &inv);
        break;
    default:
        // general case
        qreal det = determinant();
        inv = !pkQtFuzzyIsNull(det);
        if (inv)
            invert = adjoint() / det;
        break;
    }

    if (invertible)
        *invertible = inv;

    if (inv) {
        // inverting doesn't change the type
        invert.m_type = m_type;
        invert.m_dirty = m_dirty;
    }

    return invert;
}

// ── translate / scale / shear / rotate ─────────────────────────────────────

// qtransform.cpp:453-489。⚠ TxProject 那一档**先改 m_33 再贯穿**到 TxShear/
// TxRotate 的公式；末尾是条件抬升 `if (m_dirty < TxTranslate)`。
PkTransform &PkTransform::translate(qreal dx, qreal dy)
{
    if (dx == 0 && dy == 0)
        return *this;

    switch (inline_type()) {
    case TxNone:
        m_dx = dx;
        m_dy = dy;
        break;
    case TxTranslate:
        m_dx += dx;
        m_dy += dy;
        break;
    case TxScale:
        m_dx += dx * m_11;
        m_dy += dy * m_22;
        break;
    case TxProject:
        m_33 += dx * m_13 + dy * m_23;
        [[fallthrough]];   // Q_FALLTHROUGH() 的 C++17 等价物
    case TxShear:
    case TxRotate:
        m_dx += dx * m_11 + dy * m_21;
        m_dy += dy * m_22 + dx * m_12;
        break;
    }
    if (m_dirty < TxTranslate)
        m_dirty = TxTranslate;
    return *this;
}

// qtransform.cpp:498-513。⚠ 不走构造那条重算路径：m_type 直接钉死、
// m_dirty 钉成 TxNone。**判据是 `dx == 0 && dy == 0`（裸 ==，-0.0 也算 0）**。
PkTransform PkTransform::fromTranslate(qreal dx, qreal dy)
{
    PkTransform transform(1, 0, 0, 0, 1, 0, dx, dy, 1, true);
    if (dx == 0 && dy == 0)
        transform.m_type = TxNone;
    else
        transform.m_type = TxTranslate;
    transform.m_dirty = TxNone;
    return transform;
}

// qtransform.cpp:521-555。⚠ TxProject 与 TxRotate/TxShear 两档是**贯穿**到
// TxScale 的，所以 m_11/m_22 那两条乘法在四档里都会执行。
PkTransform &PkTransform::scale(qreal sx, qreal sy)
{
    if (sx == 1 && sy == 1)
        return *this;

    switch (inline_type()) {
    case TxNone:
    case TxTranslate:
        m_11 = sx;
        m_22 = sy;
        break;
    case TxProject:
        m_13 *= sx;
        m_23 *= sy;
        [[fallthrough]];   // Q_FALLTHROUGH() 的 C++17 等价物
    case TxRotate:
    case TxShear:
        m_12 *= sx;
        m_21 *= sy;
        [[fallthrough]];   // Q_FALLTHROUGH() 的 C++17 等价物
    case TxScale:
        m_11 *= sx;
        m_22 *= sy;
        break;
    }
    if (m_dirty < TxScale)
        m_dirty = TxScale;
    return *this;
}

// qtransform.cpp:564-579。⚠ 判据是 `sx == 1. && sy == 1.`。
PkTransform PkTransform::fromScale(qreal sx, qreal sy)
{
    PkTransform transform(sx, 0, 0, 0, sy, 0, 0, 0, 1, true);
    if (sx == 1. && sy == 1.)
        transform.m_type = TxNone;
    else
        transform.m_type = TxScale;
    transform.m_dirty = TxNone;
    return transform;
}

// qtransform.cpp:587-629。⚠ TxRotate/TxShear 那一档先把四个乘积算进临时量
// 再一起写回 —— 顺序改了取值就变（m_11 被覆盖之后 m_12 的公式就读到新值了）。
PkTransform &PkTransform::shear(qreal sh, qreal sv)
{
    if (sh == 0 && sv == 0)
        return *this;

    switch (inline_type()) {
    case TxNone:
    case TxTranslate:
        m_12 = sv;
        m_21 = sh;
        break;
    case TxScale:
        m_12 = sv * m_22;
        m_21 = sh * m_11;
        break;
    case TxProject: {
        qreal tm13 = sv * m_23;
        qreal tm23 = sh * m_13;
        m_13 += tm13;
        m_23 += tm23;
    }
        [[fallthrough]];   // Q_FALLTHROUGH() 的 C++17 等价物
    case TxRotate:
    case TxShear: {
        qreal tm11 = sv * m_21;
        qreal tm22 = sh * m_12;
        qreal tm12 = sv * m_22;
        qreal tm21 = sh * m_11;
        m_11 += tm11; m_12 += tm12;
        m_21 += tm21; m_22 += tm22;
        break;
    }
    }
    if (m_dirty < TxShear)
        m_dirty = TxShear;
    return *this;
}

// qtransform.cpp:631-632
static const qreal pk_deg2rad = qreal(0.017453292519943295769);        // pi/180
static const qreal pk_inv_dist_to_plane = 1. / 1024.;

// qtransform.cpp:648-725。
// ⚠ **直角特判**：90/-270 → sina=1（cosa 留 0）；270/-90 → sina=-1；
// 180 → cosa=-1（sina 留 0）。**-180 不在特判里**，走 sin/cos。
// 实测 rotate(90) 之后 m11 恰为 0、m12 恰为 1（不是 6.1e-17）。
// ⚠ 非 Z 轴走的是完全另一套：造一个 result（m_type 直接钉成 TxProject）
// 再 `*this = result * *this` —— 注意乘法方向是 **result 在左**。
PkTransform &PkTransform::rotate(qreal a, Qt::Axis axis)
{
    if (a == 0)
        return *this;

    qreal sina = 0;
    qreal cosa = 0;
    if (a == 90. || a == -270.)
        sina = 1.;
    else if (a == 270. || a == -90.)
        sina = -1.;
    else if (a == 180.)
        cosa = -1.;
    else {
        qreal b = pk_deg2rad * a;     // convert to radians
        sina = std::sin(b);           // fast and convenient
        cosa = std::cos(b);
    }

    if (axis == Qt::ZAxis) {
        switch (inline_type()) {
        case TxNone:
        case TxTranslate:
            m_11 = cosa;
            m_12 = sina;
            m_21 = -sina;
            m_22 = cosa;
            break;
        case TxScale: {
            qreal tm11 = cosa * m_11;
            qreal tm12 = sina * m_22;
            qreal tm21 = -sina * m_11;
            qreal tm22 = cosa * m_22;
            m_11 = tm11; m_12 = tm12;
            m_21 = tm21; m_22 = tm22;
            break;
        }
        case TxProject: {
            qreal tm13 = cosa * m_13 + sina * m_23;
            qreal tm23 = -sina * m_13 + cosa * m_23;
            m_13 = tm13;
            m_23 = tm23;
            [[fallthrough]];   // Q_FALLTHROUGH() 的 C++17 等价物
        }
        case TxRotate:
        case TxShear: {
            qreal tm11 = cosa * m_11 + sina * m_21;
            qreal tm12 = cosa * m_12 + sina * m_22;
            qreal tm21 = -sina * m_11 + cosa * m_21;
            qreal tm22 = -sina * m_12 + cosa * m_22;
            m_11 = tm11; m_12 = tm12;
            m_21 = tm21; m_22 = tm22;
            break;
        }
        }
        if (m_dirty < TxRotate)
            m_dirty = TxRotate;
    } else {
        PkTransform result;
        if (axis == Qt::YAxis) {
            result.m_11 = cosa;
            result.m_13 = -sina * pk_inv_dist_to_plane;
        } else {
            result.m_22 = cosa;
            result.m_23 = -sina * pk_inv_dist_to_plane;
        }
        result.m_type = TxProject;
        *this = result * *this;
    }

    return *this;
}

// qtransform.cpp:741-803。与 rotate 的 Z 轴分支逐字相同，**只是没有直角特判**
// （弧度制没有"整 90 度"这个概念），所以 rotateRadians(M_PI/2) 的 m11 是
// 6.1e-17 而不是 0。两个函数不合并正是为了这一条。
PkTransform &PkTransform::rotateRadians(qreal a, Qt::Axis axis)
{
    qreal sina = std::sin(a);
    qreal cosa = std::cos(a);

    if (axis == Qt::ZAxis) {
        switch (inline_type()) {
        case TxNone:
        case TxTranslate:
            m_11 = cosa;
            m_12 = sina;
            m_21 = -sina;
            m_22 = cosa;
            break;
        case TxScale: {
            qreal tm11 = cosa * m_11;
            qreal tm12 = sina * m_22;
            qreal tm21 = -sina * m_11;
            qreal tm22 = cosa * m_22;
            m_11 = tm11; m_12 = tm12;
            m_21 = tm21; m_22 = tm22;
            break;
        }
        case TxProject: {
            qreal tm13 = cosa * m_13 + sina * m_23;
            qreal tm23 = -sina * m_13 + cosa * m_23;
            m_13 = tm13;
            m_23 = tm23;
            [[fallthrough]];   // Q_FALLTHROUGH() 的 C++17 等价物
        }
        case TxRotate:
        case TxShear: {
            qreal tm11 = cosa * m_11 + sina * m_21;
            qreal tm12 = cosa * m_12 + sina * m_22;
            qreal tm21 = -sina * m_11 + cosa * m_21;
            qreal tm22 = -sina * m_12 + cosa * m_22;
            m_11 = tm11; m_12 = tm12;
            m_21 = tm21; m_22 = tm22;
            break;
        }
        }
        if (m_dirty < TxRotate)
            m_dirty = TxRotate;
    } else {
        PkTransform result;
        if (axis == Qt::YAxis) {
            result.m_11 = cosa;
            result.m_13 = -sina * pk_inv_dist_to_plane;
        } else {
            result.m_22 = cosa;
            result.m_23 = -sina * pk_inv_dist_to_plane;
        }
        result.m_type = TxProject;
        *this = result * *this;
    }
    return *this;
}

// ── 比较与乘法 ─────────────────────────────────────────────────────────────

// qtransform.cpp:810-821。裸 `==`：NaN 矩阵不等于自己，+0.0 与 -0.0 判等。
bool PkTransform::operator==(const PkTransform &o) const
{
    return m_11 == o.m_11 &&
           m_12 == o.m_12 &&
           m_21 == o.m_21 &&
           m_22 == o.m_22 &&
           m_dx == o.m_dx &&
           m_dy == o.m_dy &&
           m_13 == o.m_13 &&
           m_23 == o.m_23 &&
           m_33 == o.m_33;
}

// qtransform.cpp:851-854
bool PkTransform::operator!=(const PkTransform &o) const
{
    return !operator==(o);
}

// qtransform.cpp:863-935。⚠ 两条提前返回都会**整个跳过** m_dirty/m_type 的写入：
// 对方是 TxNone 时原样返回、自己是 TxNone 时整体赋值成对方（连 m_type/m_dirty
// 一起拷）。末尾是**无条件** `m_dirty = t; m_type = t;`（不是条件抬升）。
PkTransform &PkTransform::operator*=(const PkTransform &o)
{
    const TransformationType otherType = o.inline_type();
    if (otherType == TxNone)
        return *this;

    const TransformationType thisType = inline_type();
    if (thisType == TxNone)
        return operator=(o);

    TransformationType t = qMax(thisType, otherType);
    switch (t) {
    case TxNone:
        break;
    case TxTranslate:
        m_dx += o.m_dx;
        m_dy += o.m_dy;
        break;
    case TxScale:
    {
        qreal m11 = m_11 * o.m_11;
        qreal m22 = m_22 * o.m_22;

        qreal m31 = m_dx * o.m_11 + o.m_dx;
        qreal m32 = m_dy * o.m_22 + o.m_dy;

        m_11 = m11;
        m_22 = m22;
        m_dx = m31; m_dy = m32;
        break;
    }
    case TxRotate:
    case TxShear:
    {
        qreal m11 = m_11 * o.m_11 + m_12 * o.m_21;
        qreal m12 = m_11 * o.m_12 + m_12 * o.m_22;

        qreal m21 = m_21 * o.m_11 + m_22 * o.m_21;
        qreal m22 = m_21 * o.m_12 + m_22 * o.m_22;

        qreal m31 = m_dx * o.m_11 + m_dy * o.m_21 + o.m_dx;
        qreal m32 = m_dx * o.m_12 + m_dy * o.m_22 + o.m_dy;

        m_11 = m11; m_12 = m12;
        m_21 = m21; m_22 = m22;
        m_dx = m31; m_dy = m32;
        break;
    }
    case TxProject:
    {
        qreal m11 = m_11 * o.m_11 + m_12 * o.m_21 + m_13 * o.m_dx;
        qreal m12 = m_11 * o.m_12 + m_12 * o.m_22 + m_13 * o.m_dy;
        qreal m13 = m_11 * o.m_13 + m_12 * o.m_23 + m_13 * o.m_33;

        qreal m21 = m_21 * o.m_11 + m_22 * o.m_21 + m_23 * o.m_dx;
        qreal m22 = m_21 * o.m_12 + m_22 * o.m_22 + m_23 * o.m_dy;
        qreal m23 = m_21 * o.m_13 + m_22 * o.m_23 + m_23 * o.m_33;

        qreal m31 = m_dx * o.m_11 + m_dy * o.m_21 + m_33 * o.m_dx;
        qreal m32 = m_dx * o.m_12 + m_dy * o.m_22 + m_33 * o.m_dy;
        qreal m33 = m_dx * o.m_13 + m_dy * o.m_23 + m_33 * o.m_33;

        m_11 = m11; m_12 = m12; m_13 = m13;
        m_21 = m21; m_22 = m22; m_23 = m23;
        m_dx = m31; m_dy = m32; m_33 = m33;
    }
    }

    m_dirty = t;
    m_type = t;

    return *this;
}

// qtransform.cpp:945-1018。**与 operator*= 不是同一份代码**，两处差别都照抄：
//   · TxTranslate 分支的 dx 是 `=`、dy 是 **`+=`**（qtransform.cpp:961-962）。
//     t 的 m_dy 初值是 +0.0，所以两侧 dy 都是 -0.0 时 `+=` 给 +0.0 而 `=` 会给
//     -0.0 —— 唯一看得见这个区别的输入形态。**这是 Qt 的写法，不是笔误修正对象。**
//   · 提前返回的是 `*this` / `m` 的**副本**（连 m_type/m_dirty 一起）。
PkTransform PkTransform::operator*(const PkTransform &m) const
{
    const TransformationType otherType = m.inline_type();
    if (otherType == TxNone)
        return *this;

    const TransformationType thisType = inline_type();
    if (thisType == TxNone)
        return m;

    PkTransform t(true);
    TransformationType type = qMax(thisType, otherType);
    switch (type) {
    case TxNone:
        break;
    case TxTranslate:
        t.m_dx = m_dx + m.m_dx;
        t.m_dy += m_dy + m.m_dy;
        break;
    case TxScale:
    {
        qreal m11 = m_11 * m.m_11;
        qreal m22 = m_22 * m.m_22;

        qreal m31 = m_dx * m.m_11 + m.m_dx;
        qreal m32 = m_dy * m.m_22 + m.m_dy;

        t.m_11 = m11;
        t.m_22 = m22;
        t.m_dx = m31; t.m_dy = m32;
        break;
    }
    case TxRotate:
    case TxShear:
    {
        qreal m11 = m_11 * m.m_11 + m_12 * m.m_21;
        qreal m12 = m_11 * m.m_12 + m_12 * m.m_22;

        qreal m21 = m_21 * m.m_11 + m_22 * m.m_21;
        qreal m22 = m_21 * m.m_12 + m_22 * m.m_22;

        qreal m31 = m_dx * m.m_11 + m_dy * m.m_21 + m.m_dx;
        qreal m32 = m_dx * m.m_12 + m_dy * m.m_22 + m.m_dy;

        t.m_11 = m11; t.m_12 = m12;
        t.m_21 = m21; t.m_22 = m22;
        t.m_dx = m31; t.m_dy = m32;
        break;
    }
    case TxProject:
    {
        qreal m11 = m_11 * m.m_11 + m_12 * m.m_21 + m_13 * m.m_dx;
        qreal m12 = m_11 * m.m_12 + m_12 * m.m_22 + m_13 * m.m_dy;
        qreal m13 = m_11 * m.m_13 + m_12 * m.m_23 + m_13 * m.m_33;

        qreal m21 = m_21 * m.m_11 + m_22 * m.m_21 + m_23 * m.m_dx;
        qreal m22 = m_21 * m.m_12 + m_22 * m.m_22 + m_23 * m.m_dy;
        qreal m23 = m_21 * m.m_13 + m_22 * m.m_23 + m_23 * m.m_33;

        qreal m31 = m_dx * m.m_11 + m_dy * m.m_21 + m_33 * m.m_dx;
        qreal m32 = m_dx * m.m_12 + m_dy * m.m_22 + m_33 * m.m_dy;
        qreal m33 = m_dx * m.m_13 + m_dy * m.m_23 + m_33 * m.m_33;

        t.m_11 = m11; t.m_12 = m12; t.m_13 = m13;
        t.m_21 = m21; t.m_22 = m22; t.m_23 = m23;
        t.m_dx = m31; t.m_dy = m32; t.m_33 = m33;
    }
    }

    t.m_dirty = type;
    t.m_type = type;

    return t;
}

// ── map ────────────────────────────────────────────────────────────────────

// qtransform.cpp:1189-1222。⚠ **不走 PK_MAP**：这里的 `w = 1./(...)` 没有
// PK_NEAR_CLIP 夹持，除以 0 会得到 inf、除以负数会得到负的 w。
// 实测 t(m13=-1) 在 (10,0) 上：map(QPoint) = (-1,0)，而 map(int*) = (1e7,0)。
// 把两族合并成一条实现会把这一整类差异抹掉。
PkPoint PkTransform::map(const PkPoint &p) const
{
    qreal fx = p.x();
    qreal fy = p.y();

    qreal x = 0, y = 0;

    TransformationType t = inline_type();
    switch (t) {
    case TxNone:
        x = fx;
        y = fy;
        break;
    case TxTranslate:
        x = fx + m_dx;
        y = fy + m_dy;
        break;
    case TxScale:
        x = m_11 * fx + m_dx;
        y = m_22 * fy + m_dy;
        break;
    case TxRotate:
    case TxShear:
    case TxProject:
        x = m_11 * fx + m_21 * fy + m_dx;
        y = m_12 * fx + m_22 * fy + m_dy;
        if (t == TxProject) {
            qreal w = 1. / (m_13 * fx + m_23 * fy + m_33);
            x *= w;
            y *= w;
        }
    }
    return PkPoint(qRound(x), qRound(y));
}

// qtransform.cpp:1240-1273。与上面逐字相同，只是不做 qRound。
PkPointF PkTransform::map(const PkPointF &p) const
{
    qreal fx = p.x();
    qreal fy = p.y();

    qreal x = 0, y = 0;

    TransformationType t = inline_type();
    switch (t) {
    case TxNone:
        x = fx;
        y = fy;
        break;
    case TxTranslate:
        x = fx + m_dx;
        y = fy + m_dy;
        break;
    case TxScale:
        x = m_11 * fx + m_dx;
        y = m_22 * fy + m_dy;
        break;
    case TxRotate:
    case TxShear:
    case TxProject:
        x = m_11 * fx + m_21 * fy + m_dx;
        y = m_12 * fx + m_22 * fy + m_dy;
        if (t == TxProject) {
            qreal w = 1. / (m_13 * fx + m_23 * fy + m_33);
            x *= w;
            y *= w;
        }
    }
    return PkPointF(x, y);
}

// qtransform.cpp —— 真 Qt 5.15.7 源码就是 `return QLineF(map(l.p1()),
// map(l.p2()));`：两个端点各自走已实现的 map(PkPointF)，不是新算法。
// R-21 T1 交付 PkLineF 后顺带解开（文件头「依赖当时范围外的类型」一节）。
PkLineF PkTransform::map(const PkLineF &l) const
{
    return PkLineF(map(l.p1()), map(l.p2()));
}

// ═══ R-21 T2：map(PkPolygonF) / squareToQuad / quadToSquare ════════════════
//
// R-21 T2 交付 PkPolygonF 之前这三个做不出来（文件头「依赖当时范围外的类型」
// 一节），与 T1 顺带解开 map(QLineF) 是同一个模式。

// qtransform.cpp:1465-1483。真 Qt 三分支：
//   t <= TxTranslate  → 整体走 translated()（不逐点算，纯平移）
//   t <  TxProject     → 逐点走仿射公式（MAP 宏在这个档位区间不触发 1/w 分支）
//   t >= TxProject     → **真 Qt 铺进 QPainterPath 做透视裁剪**（近裁剪面外的
//                        顶点被裁掉/替换，不是简单地对每个顶点各自做透视除法）
//
// ═══ 与 Qt 的一处真实行为偏离（同 mapRect 的 TxProject 分支同一个模式）══════
//
// `QPainterPath` 不在 R-21 交付范围（`Qt替代品选型.md` §1 几何那一行点名的
// 十个类型里没有它，归 R-22），所以 `t >= TxProject` 时本类**不裁剪**，逐点
// 走已实现的 `map(const PkPointF&)`——它自己的 TxProject 分支就是无夹持的
// `1/(m13*x+m23*y+m33)` 透视除法（见上方 map(const PkPointF&)），点落在近
// 裁剪面之外时得到的是 inf/翻转坐标而不是被裁掉的新顶点。
//
// `t < TxProject` 的一般分支**直接复用 map(const PkPointF&)，不是简化**：
// 该函数的 switch 对 TxNone/TxTranslate/TxScale/TxRotate/TxShear 五档给出的
// 公式，与真 Qt 这里内联展开的 MAP 宏在同一档位上逐字同构（两者共同的来源都
// 是 qtransform.cpp 的 MAP 宏），且 `t != TxProject` 时 map(PkPointF) 压根不
// 触碰 1/w 那个分支——于是对每个点调用它，取值与真 Qt 的一般分支逐位一致。
// 登记在 oracle/geometry.deviation，README「覆盖度缺口」同步点名。
PkPolygonF PkTransform::map(const PkPolygonF &a) const
{
    TransformationType t = inline_type();
    if (t <= TxTranslate)
        return a.translated(m_dx, m_dy);

    const int n = a.size();
    PkPolygonF p(n);
    for (int i = 0; i < n; ++i)
        p[i] = map(a.at(i));
    return p;
}

// qtransform.cpp:1810-1864。把单位正方形 (0,0)-(1,0)-(1,1)-(0,1) 映射成给定
// 四边形 quad 的变换矩阵。quad 恰好 4 个点时才有意义，否则返回 false（result
// 不动）。
//
// 两条路径：
//   ax==0 且 ay==0（quad 是平行四边形，仿射可解）→ 直接 setMatrix 六个仿射
//     分量，m13=m23=0（TxShear 档，不需要透视）。
//   否则（真透视四边形）→ 解一个 2x2 线性方程组拿 g/h（m13/m23 的候选值，
//     bottom==0 时方程组奇异，返回 false），再回代出 a..h 八个分量。
bool PkTransform::squareToQuad(const PkPolygonF &quad, PkTransform &result)
{
    if (quad.count() != 4)
        return false;

    qreal dx0 = quad[0].x();
    qreal dx1 = quad[1].x();
    qreal dx2 = quad[2].x();
    qreal dx3 = quad[3].x();

    qreal dy0 = quad[0].y();
    qreal dy1 = quad[1].y();
    qreal dy2 = quad[2].y();
    qreal dy3 = quad[3].y();

    double ax = dx0 - dx1 + dx2 - dx3;
    double ay = dy0 - dy1 + dy2 - dy3;

    if (!ax && !ay) { // affine transform
        result.setMatrix(dx1 - dx0, dy1 - dy0, 0,
                          dx2 - dx1, dy2 - dy1, 0,
                          dx0,       dy0,       1);
    } else {
        double ax1 = dx1 - dx2;
        double ax2 = dx3 - dx2;
        double ay1 = dy1 - dy2;
        double ay2 = dy3 - dy2;

        // determinants
        double gtop   =  ax  * ay2 - ax2 * ay;
        double htop   =  ax1 * ay  - ax  * ay1;
        double bottom =  ax1 * ay2 - ax2 * ay1;

        double a, b, c, d, e, f, g, h; // i is always 1

        if (!bottom)
            return false;

        g = gtop / bottom;
        h = htop / bottom;

        a = dx1 - dx0 + g * dx1;
        b = dx3 - dx0 + h * dx3;
        c = dx0;
        d = dy1 - dy0 + g * dy1;
        e = dy3 - dy0 + h * dy3;
        f = dy0;

        result.setMatrix(a, d, g,
                          b, e, h,
                          c, f, 1.0);
    }

    return true;
}

// qtransform.cpp:1875-1884。squareToQuad 反过来求逆——**直接复用已实现的
// inverted()**，不是新算法。invertible 从 inverted() 的 out 参数原样转发。
bool PkTransform::quadToSquare(const PkPolygonF &quad, PkTransform &result)
{
    if (!squareToQuad(quad, result))
        return false;

    bool invertible = false;
    result = result.inverted(&invertible);

    return invertible;
}

// qtransform.cpp:2086-2090。**走 PK_MAP，带夹持。**
void PkTransform::map(qreal x, qreal y, qreal *tx, qreal *ty) const
{
    TransformationType t = inline_type();
    PK_MAP(x, y, *tx, *ty);
}

// qtransform.cpp:2100-2107。同上带夹持，末尾 qRound。
void PkTransform::map(int x, int y, int *tx, int *ty) const
{
    TransformationType t = inline_type();
    qreal fx = 0, fy = 0;
    PK_MAP(x, y, fx, fy);
    *tx = qRound(fx);
    *ty = qRound(fy);
}

// ── mapRect ────────────────────────────────────────────────────────────────

// qtransform.cpp:1934-1940。参数是 PkRectF：整数版调用时靠 PkRect → PkRectF
// 的隐式提升，与 Qt 一致。
static inline bool pkNeedsPerspectiveClipping(const PkRectF &rect, const PkTransform &transform)
{
    const qreal wx = qMin(transform.m13() * rect.left(), transform.m13() * rect.right());
    const qreal wy = qMin(transform.m23() * rect.top(), transform.m23() * rect.bottom());

    return wx + wy + transform.m33() < PK_NEAR_CLIP;
}

// ═══ 与 Qt 的唯一一处真实行为偏离 ═══════════════════════════════════════════
//
// Qt 的两个 mapRect 在 **`t == TxProject` 且 pkNeedsPerspectiveClipping(rect) 为真**
// 时不走四角包围盒，而是
//     QPainterPath path; path.addRect(rect); return map(path).boundingRect();
// —— 把矩形当路径、在近裁剪面上真的**裁**一刀再取包围盒。
//
// `QPainterPath` 不在 R-03 交付范围（`Qt替代品选型.md` §1 几何那一行点名的四个
// 类型里没有它，归属未定；实测 766 次 / 168 文件，是一个独立子系统），
// 所以这一支**不实现**：本类在该输入形态上落回四角包围盒。
//
// 实测这确实是可观察的差异（探针 §C1，真 Qt 5.15.7）：
//     t(1,0,-1, 0,1,0, 0,0,1)（m13 = -1）对 QRectF(0,0,10,10)
//     needsClip = 1（wx+wy+m33 = -9 < 1e-6）
//     Qt:  mapRect(QRectF) = (0, 0, 999999, 1e+07)
//     四角包围盒（本类）  = (0, 0, 1e+07, 1e+07)
//
// 处置：**声明成偏离**，tag 谓词就是这一条判据本身
//（`type() == TxProject && pkNeedsPerspectiveClipping(rect, *this)`，
// 与理由里的限定词一一对应，见 oracle/geometry.deviation 的 mapRect/persp-clip 两行），
// 并写进 README 覆盖度缺口 + 回报给主会话的归属问题清单。
// **不是**把这片输入从对拍里拿掉 —— 那样谁都看不见这个洞有多大。
// ═══════════════════════════════════════════════════════════════════════════

// qtransform.cpp:1963-1985 的四角包围盒。抽成成员的理由见 PkTransform.h 的私有段：
// 让 mapRect 保住 Qt 原本的四分支结构，那条已声明的偏离才在代码里显形。
PkRect PkTransform::mapRectCorners(const PkRect &rect, TransformationType t) const
{
        // see mapToPolygon for explanations of the algorithm.
        qreal x = 0, y = 0;
        PK_MAP(rect.left(), rect.top(), x, y);
        qreal xmin = x;
        qreal ymin = y;
        qreal xmax = x;
        qreal ymax = y;
        PK_MAP(rect.right() + 1, rect.top(), x, y);
        xmin = qMin(xmin, x);
        ymin = qMin(ymin, y);
        xmax = qMax(xmax, x);
        ymax = qMax(ymax, y);
        PK_MAP(rect.right() + 1, rect.bottom() + 1, x, y);
        xmin = qMin(xmin, x);
        ymin = qMin(ymin, y);
        xmax = qMax(xmax, x);
        ymax = qMax(ymax, y);
        PK_MAP(rect.left(), rect.bottom() + 1, x, y);
        xmin = qMin(xmin, x);
        ymin = qMin(ymin, y);
        xmax = qMax(xmax, x);
        ymax = qMax(ymax, y);
        return PkRect(qRound(xmin), qRound(ymin), qRound(xmax) - qRound(xmin),
                      qRound(ymax) - qRound(ymin));
}

// qtransform.cpp:1942-1991
PkRect PkTransform::mapRect(const PkRect &rect) const
{
    TransformationType t = inline_type();
    if (t <= TxTranslate)
        return rect.translated(qRound(m_dx), qRound(m_dy));

    if (t <= TxScale) {
        int x = qRound(m_11 * rect.x() + m_dx);
        int y = qRound(m_22 * rect.y() + m_dy);
        int w = qRound(m_11 * rect.width());
        int h = qRound(m_22 * rect.height());
        if (w < 0) {
            w = -w;
            x -= w;
        }
        if (h < 0) {
            h = -h;
            y -= h;
        }
        return PkRect(x, y, w, h);
    } else if (t < TxProject || !pkNeedsPerspectiveClipping(rect, *this)) {
        return mapRectCorners(rect, t);
    } else {
        // ⚠⚠ **已声明的偏离就住在这一支** ⚠⚠
        // Qt 在这里是：QPainterPath path; path.addRect(rect);
        //              return map(path).boundingRect().toRect();
        // QPainterPath 不在 R-03 交付范围（归属未定），本类落回四角包围盒。
        // 分支保留成 Qt 的原样、而不是把两支合掉，就是为了让这一支在代码里
        // **看得见**：读到这里的人不必翻 .deviation 才知道有个洞。
        // 量化在 oracle/geometry.deviation 的 persp-clip 那 23 行（含分母）。
        return mapRectCorners(rect, t);
    }
}

// qtransform.cpp:2033-2054 的四角包围盒。⚠ 与整数版的两处不同：四角用的是
// x+w / y+h（**没有** +1 的差一补偿），且全程不 qRound。
PkRectF PkTransform::mapRectCorners(const PkRectF &rect, TransformationType t) const
{
        qreal x = 0, y = 0;
        PK_MAP(rect.x(), rect.y(), x, y);
        qreal xmin = x;
        qreal ymin = y;
        qreal xmax = x;
        qreal ymax = y;
        PK_MAP(rect.x() + rect.width(), rect.y(), x, y);
        xmin = qMin(xmin, x);
        ymin = qMin(ymin, y);
        xmax = qMax(xmax, x);
        ymax = qMax(ymax, y);
        PK_MAP(rect.x() + rect.width(), rect.y() + rect.height(), x, y);
        xmin = qMin(xmin, x);
        ymin = qMin(ymin, y);
        xmax = qMax(xmax, x);
        ymax = qMax(ymax, y);
        PK_MAP(rect.x(), rect.y() + rect.height(), x, y);
        xmin = qMin(xmin, x);
        ymin = qMin(ymin, y);
        xmax = qMax(xmax, x);
        ymax = qMax(ymax, y);
        return PkRectF(xmin, ymin, xmax - xmin, ymax - ymin);
}

// qtransform.cpp:2012-2060
PkRectF PkTransform::mapRect(const PkRectF &rect) const
{
    TransformationType t = inline_type();
    if (t <= TxTranslate)
        return rect.translated(m_dx, m_dy);

    if (t <= TxScale) {
        qreal x = m_11 * rect.x() + m_dx;
        qreal y = m_22 * rect.y() + m_dy;
        qreal w = m_11 * rect.width();
        qreal h = m_22 * rect.height();
        if (w < 0) {
            w = -w;
            x -= w;
        }
        if (h < 0) {
            h = -h;
            y -= h;
        }
        return PkRectF(x, y, w, h);
    } else if (t < TxProject || !pkNeedsPerspectiveClipping(rect, *this)) {
        return mapRectCorners(rect, t);
    } else {
        // ⚠⚠ **已声明的偏离就住在这一支** ⚠⚠ 与整数版逐字同理。
        return mapRectCorners(rect, t);
    }
}

// ⚠ **必须 #undef**：本 .cpp 会被 oracle/geometry_difftest.cpp `#include` 进
// `namespace pkoracle {}`，宏没有作用域，泄漏出去会污染后面的代码。
#undef PK_MAP
#undef PK_NEAR_CLIP

// ---------------------------------------------------------------------------
// 只有在一个翻译单元里才落得了地的 static_assert（与 PkPoint.cpp / PkSize.cpp /
// PkRect.cpp 同一条形态）。
// ---------------------------------------------------------------------------

// 枚举取值：**位标志**。写成 0..5 的话下面五条里有四条会红。
static_assert(PkTransform::TxNone == 0x00, "TxNone 必须是 0");
static_assert(PkTransform::TxTranslate == 0x01, "TxTranslate 必须是 1");
static_assert(PkTransform::TxScale == 0x02, "TxScale 必须是 2");
static_assert(PkTransform::TxRotate == 0x04, "TxRotate 必须是 4");
static_assert(PkTransform::TxShear == 0x08, "TxShear 必须是 8");
static_assert(PkTransform::TxProject == 0x10, "TxProject 必须是 16");

// 位标志的序关系是 type() 与 qMax(thisType, otherType) 的地基。
static_assert(PkTransform::TxNone < PkTransform::TxTranslate
              && PkTransform::TxTranslate < PkTransform::TxScale
              && PkTransform::TxScale < PkTransform::TxRotate
              && PkTransform::TxRotate < PkTransform::TxShear
              && PkTransform::TxShear < PkTransform::TxProject,
              "六档必须严格递增 —— isAffine()/isIdentity()/qMax 全靠这条");

// 布局：九个 double 连着两个 5 位的位域。**不与 QTransform 比 sizeof** ——
// Qt5 尾部多一个恒为 nullptr 的 Private *d，我们不留（说明在 PkTransform.h 头部）。
static_assert(std::is_trivially_copyable<PkTransform>::value,
              "必须平凡可复制 —— 编译器生成的拷贝要与 Qt 那份 memcpy 等价");
static_assert(std::is_trivially_destructible<PkTransform>::value,
              "必须平凡可析构");

// noexcept 面：QTransform 的这批公开成员**一个都没有 noexcept**（只有拷贝/移动
// 和 qHash 有）。noexcept 是可观察的，多加一个就是多一档能力。
// 形态与 PkRect.cpp:616-623 相同（拿一个未求值的表达式问 noexcept），
// 不用 std::declval —— 那个住在 <utility> 里，而本 .cpp 只许包
// oracle 顶部系统头区已有的头（<cmath> 与 <type_traits>）。
static_assert(!noexcept(PkTransform().type()), "type() 在 Qt 那边不是 noexcept");
static_assert(!noexcept(PkTransform().determinant()), "determinant() 在 Qt 那边不是 noexcept");
static_assert(!noexcept(PkTransform().m11()), "m11() 在 Qt 那边不是 noexcept");
static_assert(!noexcept(PkTransform().map(PkPointF(0, 0))), "map 在 Qt 那边不是 noexcept");
static_assert(!noexcept(PkTransform().inverted(nullptr)), "inverted 在 Qt 那边不是 noexcept");

// constexpr 面：QTransform **一个 constexpr 成员都没有**（qtransform.h 里连
// Q_DECL_CONSTEXPR 都没出现过），所以这里也一个都不加 —— 加了就是替代品比 Qt
// 多一档能力。"多一档"没法用 static_assert 反证（不能断言"某表达式不是常量
// 表达式"），钉在 tests/test_transform.cpp 的 transformConstexprSurfaceMatchesQt
// 里：那条测试如果哪天能编成 constexpr 变量，就说明有人顺手加了。
