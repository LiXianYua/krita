#include "PkTransform.h"
#include "PkPainterPath.h"
// S-18：Bezier 辅助（照 Qt 的 `qbezier_p.h` 形制）。`pkCubicToClipped` 要用
// `pkSplitBezier` 把曲线摊成折线 —— 上游 `cubicTo_clipped` 走的就是
// `QBezier::toPolygon`，同一个头在 qtransform.cpp / qpainterpath.cpp 之间共用。
#include "PkBezier_p.h"

// ⚠ **这两个系统头必须在 oracle/geometry_difftest.cpp 顶部的系统头区里也出现过**
// —— 那份对拍把本 .cpp `#include` 进 `namespace pkoracle {}` 里，头文件守卫已经
// 点掉的 include 才会空转；没出现过的话会造出 pkoracle::std（与 PkSize.cpp /
// PkRect.cpp 顶部同一条纪律）。两个都在（<cmath> 与 <type_traits>）。
// <cmath> 是 rotate / rotateRadians 的 std::sin / std::cos 要的。
#include <cmath>
#include <type_traits>

// ⚠ S-18：`mapProjective` 的两个重载定义在本文件靠后（与上游 qtransform.cpp 里
// 「先定义 mapProjective、后用到」的排布不同 —— 本文件 `map(const PkPolygonF&)`
// 在前）。前置声明即可，**不要把定义挪位置**：挪到前面会打乱本文件既有的
// 「按 Qt 原行号顺序排布」的编排，那是给对照上游看的人留的。
static PkPainterPath pkMapProjective(const PkTransform &transform, const PkPainterPath &path);
static PkPolygonF pkMapProjective(const PkTransform &transform, const PkPolygonF &poly);

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
// **S-18（2026-09-12 人拍板）起本文件不再有真实行为偏离**：原先唯一那一处 ——
// `mapRect` / `map(PkPainterPath)` / `map(PkPolygonF)` 在 `TxProject` 下 Qt 走
// `mapProjective`（近/远裁剪面上的真裁剪）、本类落回四角包围盒 —— **已经实现**，
// 见本文件 `pkMapProjective` 那一整块。原成文理由「`QPainterPath` 归属未定不在
// R-03 范围」已过期（`PkPainterPath` 由 R-22 交付，且消费方是活的）。
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
PkTransform &PkTransform::rotate(qreal a, Pk::Axis axis)
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

    if (axis == Pk::ZAxis) {
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
        if (axis == Pk::YAxis) {
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
PkTransform &PkTransform::rotateRadians(qreal a, Pk::Axis axis)
{
    qreal sina = std::sin(a);
    qreal cosa = std::cos(a);

    if (axis == Pk::ZAxis) {
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
        if (axis == Pk::YAxis) {
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

    TransformationType t = pkMax(thisType, otherType);
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
    TransformationType type = pkMax(thisType, otherType);
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
    return PkPoint(pkRound(x), pkRound(y));
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
PkPolygonF PkTransform::map(const PkPolygonF &a) const
{
    TransformationType t = inline_type();
    if (t <= TxTranslate)
        return a.translated(m_dx, m_dy);

    // 上游：`if (t >= QTransform::TxProject) return mapProjective(*this, a);`
    //（qtransform.cpp:1472）。S-18 起这一支实现好了，不再声明成偏离。
    if (t >= TxProject)
        return pkMapProjective(*this, a);

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
    *tx = pkRound(fx);
    *ty = pkRound(fy);
}

// ── mapRect ────────────────────────────────────────────────────────────────

// qtransform.cpp:1934-1940。参数是 PkRectF：整数版调用时靠 PkRect → PkRectF
// 的隐式提升，与 Qt 一致。
static inline bool pkNeedsPerspectiveClipping(const PkRectF &rect, const PkTransform &transform)
{
    const qreal wx = pkMin(transform.m13() * rect.left(), transform.m13() * rect.right());
    const qreal wy = pkMin(transform.m23() * rect.top(), transform.m23() * rect.bottom());

    return wx + wy + transform.m33() < PK_NEAR_CLIP;
}

// ═══ 与 Qt 的唯一一处真实行为偏离 ═══════════════════════════════════════════
//
// Qt 的两个 mapRect 在 **`t == TxProject` 且 pkNeedsPerspectiveClipping(rect) 为真**
// 时不走四角包围盒，而是
//     QPainterPath path; path.addRect(PkRectF(rect)); return map(path).boundingRect();
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

// ═══════════════════════════════════════════════════════════════════════════
// 投影路径映射（`mapProjective`）—— S-18 按 `S线-spec.md`
// 「S-18 撞到的投影偏离：不声明，实现 mapProjective」（2026-09-12 人拍板）。
//
// 上游 qtbase 5.15 `src/gui/painting/qtransform.cpp` 的一整块，**逐字照抄**，
// 只把 `Q*` 换成 `Pk*`（`qAbs`→`pkAbs`、`qMax`→`pkMax`、`qSqrt`→`std::sqrt`、
// `qFuzzyCompare`→`pkQtFuzzyCompare`）。**别自创** —— 这条线刚立的规矩是
// 「判据是 Qt 怎么做，不是本 fork 当年怎么拍的」：
//   qt_scaleForTransform / QHomogeneousCoordinate / mapHomogeneous /
//   lineTo_clipped / cubicTo_clipped / mapProjective(QPainterPath) /
//   mapProjective(QPolygonF)
//
// 这一块关掉的是**同一条根因**的四处（原先都声明成偏离）：
//   `T::map(PainterPath) txproject` 2652 · `T::mapRect(PkRectF)`
//   `persp-clip/proj-signed-zero/signed-zero` 1 · `T::mapRect(PkRect)` 的 8 行
//   `persp-clip/*` · `T::map(PolygonF) txproject-deviation`。
// 原成文理由「`QPainterPath` 归属未定不在 R-03 范围」**已过期**：`PkPainterPath`
// 由 R-22 交付，且消费方是活的
//（`plugins/tools/tool_transform2/kis_perspective_transform_strategy.cpp:274`
//  的 `handlesTransform.map(handles)` 正是这条路径；`libs/global/kis_dom_utils.cpp`
//  还会从 XML 反序列化含 m13/m23/m33 的九分量矩阵）。
// ═══════════════════════════════════════════════════════════════════════════

// ⚠ 近裁剪面用本文件顶部那个 `PK_NEAR_CLIP`（`qtransform.cpp:64`，本机
// qreal==double 所以是 1e-6）—— **不要在这里再 define 一个**：`PK_MAP` 与
// `pkNeedsPerspectiveClipping` 用的是同一个常量，两份定义迟早会漂。
// （S-18 第一版重复 define 过，编译期就撞 `macro redefined` 警告，已删。）

// qtransform.cpp:2373-2410（`qt_scaleForTransform`）。返回值在
// `cubicTo_clipped` 里没被用，但照抄保留 —— 它是「旋转+等比缩放」的判据。
static bool pkScaleForTransform(const PkTransform &transform, qreal *scale)
{
    const PkTransform::TransformationType type = transform.type();
    if (type <= PkTransform::TxTranslate) {
        if (scale)
            *scale = 1;
        return true;
    } else if (type == PkTransform::TxScale) {
        const qreal xScale = pkAbs(transform.m11());
        const qreal yScale = pkAbs(transform.m22());
        if (scale)
            *scale = pkMax(xScale, yScale);
        return pkQtFuzzyCompare(xScale, yScale);
    }

    // rotate then scale: compare columns
    const qreal xScale1 = transform.m11() * transform.m11()
                        + transform.m21() * transform.m21();
    const qreal yScale1 = transform.m12() * transform.m12()
                        + transform.m22() * transform.m22();

    // scale then rotate: compare rows
    const qreal xScale2 = transform.m11() * transform.m11()
                        + transform.m12() * transform.m12();
    const qreal yScale2 = transform.m21() * transform.m21()
                        + transform.m22() * transform.m22();

    // decide the order of rotate and scale operations
    if (pkAbs(xScale1 - yScale1) > pkAbs(xScale2 - yScale2)) {
        if (scale)
            *scale = std::sqrt(pkMax(xScale1, yScale1));

        return type == PkTransform::TxRotate && pkQtFuzzyCompare(xScale1, yScale1);
    } else {
        if (scale)
            *scale = std::sqrt(pkMax(xScale2, yScale2));

        return type == PkTransform::TxRotate && pkQtFuzzyCompare(xScale2, yScale2);
    }
}

// qtransform.cpp:1576-1589（`QHomogeneousCoordinate`）。
struct PkHomogeneousCoordinate {
    qreal x = 0;
    qreal y = 0;
    qreal w = 0;

    PkHomogeneousCoordinate() {}
    PkHomogeneousCoordinate(qreal x_, qreal y_, qreal w_) : x(x_), y(y_), w(w_) {}

    const PkPointF toPoint() const {
        qreal iw = 1. / w;
        return PkPointF(x * iw, y * iw);
    }
};

// qtransform.cpp:1591-1599（`mapHomogeneous`）。
static inline PkHomogeneousCoordinate pkMapHomogeneous(const PkTransform &transform, const PkPointF &p)
{
    PkHomogeneousCoordinate c;
    c.x = transform.m11() * p.x() + transform.m21() * p.y() + transform.m31();
    c.y = transform.m12() * p.x() + transform.m22() * p.y() + transform.m32();
    c.w = transform.m13() * p.x() + transform.m23() * p.y() + transform.m33();
    return c;
}

// qtransform.cpp:1598-1635（`lineTo_clipped`）。近裁剪面上的真裁剪：
// 两端都在近裁剪面之前就整条丢掉；一端在前就在交点处切开。
static inline bool pkLineToClipped(PkPainterPath &path, const PkTransform &transform,
                                   const PkPointF &a, const PkPointF &b,
                                   bool needsMoveTo, bool needsLineTo = true)
{
    PkHomogeneousCoordinate ha = pkMapHomogeneous(transform, a);
    PkHomogeneousCoordinate hb = pkMapHomogeneous(transform, b);

    if (ha.w < PK_NEAR_CLIP && hb.w < PK_NEAR_CLIP)
        return false;

    if (hb.w < PK_NEAR_CLIP) {
        const qreal t = (PK_NEAR_CLIP - hb.w) / (ha.w - hb.w);

        hb.x += (ha.x - hb.x) * t;
        hb.y += (ha.y - hb.y) * t;
        hb.w = qreal(PK_NEAR_CLIP);
    } else if (ha.w < PK_NEAR_CLIP) {
        const qreal t = (PK_NEAR_CLIP - ha.w) / (hb.w - ha.w);

        ha.x += (hb.x - ha.x) * t;
        ha.y += (hb.y - ha.y) * t;
        ha.w = qreal(PK_NEAR_CLIP);

        const PkPointF p = ha.toPoint();
        if (needsMoveTo) {
            path.moveTo(p);
            needsMoveTo = false;
        } else {
            path.lineTo(p);
        }
    }

    if (needsMoveTo)
        path.moveTo(ha.toPoint());

    if (needsLineTo)
        path.lineTo(hb.toPoint());

    return true;
}

// qbezier.cpp:59-75（`QBezier::toPolygon`）+ :103-137（`addToPolygon`）。
// 按 flattening 阈值把三次曲线摊成折线：控制点离首尾连线足够近就停，否则二分。
// 用**显式栈**而不是递归（上游就是显式栈，深度上限 9），所以这里照抄那个循环。
static void pkBezierAddToPolygon(PkPolygonF *polygon, const PkArcBezier &bezier,
                                 qreal bezier_flattening_threshold)
{
    PkArcBezier beziers[10];
    int levels[10];
    beziers[0] = bezier;
    levels[0] = 9;
    int top = 0;

    while (top >= 0) {
        PkArcBezier *b = &beziers[top];
        // check if we can pop the top bezier curve from the stack
        qreal y4y1 = b->y4 - b->y1;
        qreal x4x1 = b->x4 - b->x1;
        qreal l = pkAbs(x4x1) + pkAbs(y4y1);
        qreal d;
        if (l > 1.) {
            d = pkAbs((x4x1) * (b->y1 - b->y2) - (y4y1) * (b->x1 - b->x2))
                + pkAbs((x4x1) * (b->y1 - b->y3) - (y4y1) * (b->x1 - b->x3));
        } else {
            d = pkAbs(b->x1 - b->x2) + pkAbs(b->y1 - b->y2)
                + pkAbs(b->x1 - b->x3) + pkAbs(b->y1 - b->y3);
            l = 1.;
        }
        if (d < bezier_flattening_threshold * l || levels[top] == 0) {
            // good enough, we pop it off and add the endpoint
            polygon->append(PkPointF(b->x4, b->y4));
            --top;
        } else {
            // split, second half of the polygon goes lower into the stack
            const PkArcBezierSplit sp = pkSplitBezier(*b);
            b[1] = sp.first;      // 上游 `std::tie(b[1], b[0]) = b->split();`
            b[0] = sp.second;
            levels[top + 1] = --levels[top];
            ++top;
        }
    }
}

static PkPolygonF pkBezierToPolygon(const PkArcBezier &bezier, qreal bezier_flattening_threshold)
{
    PkPolygonF polygon;
    polygon.append(PkPointF(bezier.x1, bezier.y1));
    pkBezierAddToPolygon(&polygon, bezier, bezier_flattening_threshold);
    return polygon;
}

// qtransform.cpp:1639-1657（`cubicTo_clipped`）。曲线先按 flattening 阈值摊成
// 折线，再逐段走 `lineTo_clipped` —— 上游注释写着理由：
// "Convert projective xformed curves to line segments so they can be
//  transformed more accurately"。
static inline bool pkCubicToClipped(PkPainterPath &path, const PkTransform &transform,
                                    const PkPointF &a, const PkPointF &b, const PkPointF &c,
                                    const PkPointF &d, bool needsMoveTo)
{
    qreal scale;
    pkScaleForTransform(transform, &scale);

    qreal curveThreshold = scale == 0 ? qreal(0.25) : (qreal(0.25) / scale);

    PkPolygonF segment = pkBezierToPolygon(PkArcBezier::fromPoints(a, b, c, d), curveThreshold);

    for (int i = 0; i < segment.size() - 1; ++i)
        if (pkLineToClipped(path, transform, segment.at(i), segment.at(i + 1), needsMoveTo))
            needsMoveTo = false;

    return !needsMoveTo;
}

// qtransform.cpp:1658-1700（`mapProjective(const QTransform&, const QPainterPath&)`）。
static PkPainterPath pkMapProjective(const PkTransform &transform, const PkPainterPath &path)
{
    PkPainterPath result;

    // ⚠ **先撒一个占位 `moveTo(0,0)`，镜像 Qt 的 `ensureData()`。**
    // 上游 `qpainterpath.cpp:598-606`：
    //     void QPainterPath::ensureData_helper() {
    //         QPainterPathPrivate *data = new QPainterPathData;
    //         data->elements.reserve(16);
    //         QPainterPath::Element e = { 0, 0, QPainterPath::MoveToElement };
    //         data->elements << e;          // ← 就是这一句
    //         d_ptr.reset(data);
    //     }
    // Qt 的 QPainterPath 一旦被 "ensure" 过就**自带一个 (0,0) 的 MoveTo 占位元素**，
    // 后续第一个 `moveTo` 会把它**覆盖**掉（qpainterpath.cpp:747 那条
    // `if (last.type == MoveToElement) 覆写`）。而 `mapProjective` 结尾会调
    // `result.setFillRule(...)`，`setFillRule` 的第一句就是 `ensureData()`
    // （qpainterpath.cpp:1395-1397）—— 于是**整条路径被裁光时，Qt 返回的不是空
    // 路径，而是一条只有一个 (0,0) MoveTo 的路径**（实测：`QPainterPath r;
    // r.setFillRule(Qt::WindingFill);` → `elementCount()==1`，而 `isEmpty()` 仍是真）。
    //
    // 本模块的 `PkPainterPath` **没有**这个占位元素（`m_elements` 空就是空），
    // 于是同一批输入下 pk 给 0 个元素、Qt 给 1 个 —— 实测这就是 `T::map(PainterPath)
    // txproject` 残留那 161 条的全部根因（都是"整个路径落在近裁剪面之后"的输入）。
    // 在**这里**撒种子是忠实的：`result` 全程只被 `moveTo`/`lineTo` 碰，语义与
    // Qt 那条 ensured 路径逐一对应；**不去改 `PkPainterPath` 的表示** —— 那是
    // 全模块的结构改动（`elementCount`/`isEmpty`/`currentPosition`/路径布尔运算
    // 都受影响），远超本任务。
    result.moveTo(PkPointF());

    PkPointF last;
    PkPointF lastMoveTo;
    bool needsMoveTo = true;
    for (int i = 0; i < path.elementCount(); ++i) {
        switch (path.elementAt(i).type) {
        case PkPainterPath::MoveToElement:
            if (i > 0 && lastMoveTo != last)
                pkLineToClipped(result, transform, last, lastMoveTo, needsMoveTo);

            lastMoveTo = path.elementAt(i);
            last = path.elementAt(i);
            needsMoveTo = true;
            break;
        case PkPainterPath::LineToElement:
            if (pkLineToClipped(result, transform, last, path.elementAt(i), needsMoveTo))
                needsMoveTo = false;
            last = path.elementAt(i);
            break;
        case PkPainterPath::CurveToElement:
            if (pkCubicToClipped(result, transform, last, path.elementAt(i),
                                 path.elementAt(i + 1), path.elementAt(i + 2), needsMoveTo))
                needsMoveTo = false;
            i += 2;
            last = path.elementAt(i);
            break;
        default:
            // 上游这里是 Q_ASSERT(false)；pk/geometry 没有断言设施（归 R-08），
            // 与 PkTransform.cpp 里其它 Q_ASSERT 位点同一处置：落到这里就跳过。
            break;
        }
    }

    if (path.elementCount() > 0 && lastMoveTo != last)
        pkLineToClipped(result, transform, last, lastMoveTo, needsMoveTo, false);

    result.setFillRule(path.fillRule());
    return result;
}

// qtransform.cpp:1416-1435（`mapProjective(const QTransform&, const QPolygonF&)`）。
// 多边形先铺成路径、走同一条路，再把元素倒回多边形。
static PkPolygonF pkMapProjective(const PkTransform &transform, const PkPolygonF &poly)
{
    if (poly.size() == 0)
        return poly;

    if (poly.size() == 1) {
        PkPolygonF one;
        one.append(transform.map(poly.at(0)));
        return one;
    }

    PkPainterPath path;
    path.addPolygon(poly);

    path = pkMapProjective(transform, path);

    PkPolygonF result;
    const int elementCount = path.elementCount();
    result.reserve(elementCount);
    for (int i = 0; i < elementCount; ++i)
        result.append(path.elementAt(i));     // Element 隐式转 PkPointF（qpainterpath.h:57）
    return result;
}

// R-22 T5: map(PkPainterPath)。关闭偏离 21。
//
// ⚠ **逐元素原地改坐标，不走 moveTo/lineTo/cubicTo 重建** —— 这是 S-18 按
// `S线-spec.md`「已裁决的岔路」的裁决 B 修的（原实现走构建器重建）。
//
// 上游 `QTransform::map(const QPainterPath &path)`（qtbase 5.15
// `src/gui/painting/qtransform.cpp`）是这么写的（**就元素结构而言本实现与它同形；
// 副作用层面不是**：上游裸写 `elements[i]` 不动别的，本实现走的
// `setElementPositionAt` 会连带更新 `m_currentPos`（且只对最后一个元素）与
// `markDirty()`。那个差在**改前就在**——旧实现走构建器重建，同样把 `m_currentPos`
// 设成了映射后的最后一点——所以不是本轮引入的；且 `currentPosition()` 不在对拍的
// `same_path` 比对里、树内也没有消费者。记在这里免得下一个人把它当成新差异去追）：
//
//     TransformationType t = inline_type();
//     if (t == TxNone || path.elementCount() == 0) return path;
//     if (t >= TxProject) return mapProjective(*this, path);
//     QPainterPath copy = path;
//     if (t == TxTranslate) { copy.translate(affine._dx, affine._dy); }
//     else {
//         copy.detach();
//         for (int i = 0; i < path.elementCount(); ++i) {
//             QPainterPath::Element &e = copy.d_ptr->elements[i];   // ← 原地改
//             MAP(e.x, e.y, e.x, e.y);
//         }
//     }
//     return copy;
//
// **它直接改 `elements[i]`，元素个数与类型结构原样保留。** 走构建器重建时，
// 构建器自带的「退化就跳过」判据会在**退化矩阵**上把路径塌掉：
//   · `PkPainterPath::lineTo` 的 `if (p == m_currentPos) return;`
//   · `PkPainterPath::cubicTo` 的 `if (last == c1 && c1 == c2 && c2 == ep) return;`
// 实测（S-18）：构造出的 11 元素路径，零矩阵下 Qt 得 **11** 个元素、原实现只得
// **1** 个；±inf 矩阵 11 vs 9；1e308 矩阵 11 vs 10。普通仿射五族（identity /
// translate / scale / rotate / shear）与次正规数两侧**逐位相同** —— 偏离只在
// 退化输入上出现，机制清楚、够得着，所以按裁决 B 修成对齐，不声明成偏离。
//
// ⚠ **投影档（t >= TxProject）仍不对齐**：Qt 走 `mapProjective(*this, path)`，
// 含透视除法与**裁剪**。本实现不做裁剪 —— 与 `mapRect` / `map(const PkPolygonF&)`
// 已声明的那条偏离**同根因**（见本文件上方 mapRect 那段长注释与 README 的偏离
// 清单），这里保持与那两处一致，不假装对齐。
PkPainterPath PkTransform::map(const PkPainterPath &path) const
{
    TransformationType t = inline_type();
    if (t == TxNone || path.elementCount() == 0) {
        return path;
    }

    // ⚠ **这一支现在是实现好的，不再是偏离**（S-18，见本函数上方那段长注释）。
    // 上游：`if (t >= TxProject) return mapProjective(*this, path);`
    if (t >= TxProject) {
        return pkMapProjective(*this, path);
    }

    PkPainterPath copy = path;

    if (t == TxTranslate) {
        copy.translate(m_dx, m_dy);
    } else {
        // 从 `path` 读、往 `copy` 写：第一次 setElementPositionAt 就把 copy 从
        // path 上 detach 掉（COW），所以读源是稳的。
        //
        // ⚠ **逐点用 `map(PkPointF)`，不要换成 `map(x, y, &tx, &ty)`。**
        // Qt 自己那两个 TxProject 点映射**不是同一个算术**：
        //   · `QTransform::map(const QPointF&)`：`w = 1./(m13*fx + m23*fy + m33)`
        //     —— **没有**近裁剪面夹持；
        //   · `MAP` 宏（`map(x,y,tx,ty)` 用的那个）：`if (w < Q_NEAR_CLIP)
        //     w = Q_NEAR_CLIP;` —— **有**夹持。
        // 本文件两个都照抄了（`map(const PkPointF&)` 与 `PK_MAP`）。原实现走的是
        // 前者，本函数**只修结构、不动算术** —— 换成后者会连带改掉投影档的取值，
        // 实测会把 `transformMapRectPerspectiveClipIsADeclaredGap` 打红
        //（mapRect 的投影支就是 `map(path).boundingRect()`）。
        for (int i = 0; i < path.elementCount(); ++i) {
            const PkPainterPath::Element e = path.elementAt(i);
            const PkPointF pt = map(PkPointF(e.x, e.y));
            copy.setElementPositionAt(i, pt.x(), pt.y());
        }
    }

    return copy;
}

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
        xmin = pkMin(xmin, x);
        ymin = pkMin(ymin, y);
        xmax = pkMax(xmax, x);
        ymax = pkMax(ymax, y);
        PK_MAP(rect.right() + 1, rect.bottom() + 1, x, y);
        xmin = pkMin(xmin, x);
        ymin = pkMin(ymin, y);
        xmax = pkMax(xmax, x);
        ymax = pkMax(ymax, y);
        PK_MAP(rect.left(), rect.bottom() + 1, x, y);
        xmin = pkMin(xmin, x);
        ymin = pkMin(ymin, y);
        xmax = pkMax(xmax, x);
        ymax = pkMax(ymax, y);
        return PkRect(pkRound(xmin), pkRound(ymin), pkRound(xmax) - pkRound(xmin),
                      pkRound(ymax) - pkRound(ymin));
}

// qtransform.cpp:1942-1991
PkRect PkTransform::mapRect(const PkRect &rect) const
{
    TransformationType t = inline_type();
    if (t <= TxTranslate)
        return rect.translated(pkRound(m_dx), pkRound(m_dy));

    if (t <= TxScale) {
        int x = pkRound(m_11 * rect.x() + m_dx);
        int y = pkRound(m_22 * rect.y() + m_dy);
        int w = pkRound(m_11 * rect.width());
        int h = pkRound(m_22 * rect.height());
        if (w < 0) {
            w = -w;
            x -= w;
        }
        if (h < 0) {
            h = -h;
            y -= h;
        }
        return PkRect(x, y, w, h);
    } else if (t < TxProject || !pkNeedsPerspectiveClipping(PkRectF(rect), *this)) {
        return mapRectCorners(rect, t);
    } else {
        // R-22 T5: 用 PkPainterPath 裁剪路径关闭偏离 21
        PkPainterPath path;
        path.addRect(PkRectF(rect));
        return map(path).boundingRect().toRect();
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
        xmin = pkMin(xmin, x);
        ymin = pkMin(ymin, y);
        xmax = pkMax(xmax, x);
        ymax = pkMax(ymax, y);
        PK_MAP(rect.x() + rect.width(), rect.y() + rect.height(), x, y);
        xmin = pkMin(xmin, x);
        ymin = pkMin(ymin, y);
        xmax = pkMax(xmax, x);
        ymax = pkMax(ymax, y);
        PK_MAP(rect.x(), rect.y() + rect.height(), x, y);
        xmin = pkMin(xmin, x);
        ymin = pkMin(ymin, y);
        xmax = pkMax(xmax, x);
        ymax = pkMax(ymax, y);
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
    } else if (t < TxProject || !pkNeedsPerspectiveClipping(PkRectF(rect), *this)) {
        return mapRectCorners(rect, t);
    } else {
        // R-22 T5: 用 PkPainterPath 裁剪路径关闭偏离 21
        PkPainterPath path;
        path.addRect(PkRectF(rect));
        return map(path).boundingRect();
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

// 位标志的序关系是 type() 与 pkMax(thisType, otherType) 的地基。
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
