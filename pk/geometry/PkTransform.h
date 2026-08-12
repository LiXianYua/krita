#ifndef PK_GEOMETRY_PKTRANSFORM_H
#define PK_GEOMETRY_PKTRANSFORM_H

#include "PkGlobal.h"
#include "PkPoint.h"
#include "PkRect.h"

// ---------------------------------------------------------------------------
// PkTransform —— QTransform 的零 Qt 替代（3x3 齐次矩阵）。
//
// **逐字抄自真 Qt 5.15.7** 的 include/QtGui/qtransform.h 与 qtbase 标签
// `v5.15.7-lts-lgpl` 的 src/gui/painting/qtransform.cpp（本机装的 Qt 只有 .so
// 没有 .cpp，源码取自上游同版本标签；核对方式是逐输入对拍，见 oracle/）。
// 来源行号标在各项上方，格式 `qtransform.h:NNN` / `qtransform.cpp:NNN`。
//
// 对齐口径：与 Qt 的任何行为差异默认都是缺陷 —— 所以 Qt 那些反直觉的地方也
// 照抄，并在 tests/test_transform.cpp 与 oracle/ 里逐条钉住：
//
//   · **行向量约定**：`p' = p * M`，即 `x' = m11*x + m21*y + m31`。
//     实测（探针 §I）：`QTransform(2,3,5,7,11,13).map(QPointF(1,0)) == (13,16)`；
//     列向量约定会得到 (14,18)。**搞反了每一个 map 都错**，而对称矩阵看不出来。
//   · **TransformationType 是位标志**：TxNone=0 TxTranslate=1 TxScale=2
//     TxRotate=4 TxShear=8 TxProject=16。照 `enum { TxNone, TxTranslate, ... }`
//     的顺序写会得到 0,1,2,3,4,5，**全错**（而且 `qMax(thisType, otherType)`
//     与 `t <= TxTranslate` 这类比较会跟着全错）。
//   · **`type()` 是有状态的，不是矩阵九个分量的纯函数。** 见下面「惰性缓存」。
//   · **直角特判**：`rotate(90/-90/180/270/-270)` 直接填 ±1/0，不走 sin/cos，
//     所以结果是精确的（m11 恰为 0 而不是 6.1e-17）。去掉特判对拍立刻分家。
//   · **`inverted` 的三条路径门槛互不相同**：TxScale 用 `qFuzzyIsNull(m11)` 与
//     `qFuzzyIsNull(m22)`；TxRotate/TxShear 走 QMatrix::inverted，判据是
//     **`det == 0.0` 精确零**；TxProject 用 `qFuzzyIsNull(det)`。
//     失败时返回的是**单位阵**，且此时 m_type/m_dirty **不**从源矩阵拷贝。
//   · **`map(PkPoint)` / `map(PkPointF)` 不夹持 w**，而
//     `map(int,int,int*,int*)` / `map(qreal,qreal,qreal*,qreal*)` 走 MAP 宏、
//     **夹持** `w >= Q_NEAR_CLIP(1e-6)`。同一个矩阵同一个点，两族给不同答案
//     （实测：t(m13=-1) 在 (10,0) 上 map(QPointF)=(-1.11111,-0)、
//     map(qreal*)=(1e7,0)）。合并成一条实现会把这一整类差异抹掉。
//   · **`operator*` 的 TxTranslate 分支里 dy 用的是 `+=` 而不是 `=`**
//     （`t.affine._dy += affine._dy + m.affine._dy`，而 t 的 _dy 初值是 +0.0）。
//     只有两侧 dy 都是 -0.0 时看得见：`=` 给 -0.0、`+=` 给 +0.0。照抄 `+=`。
//   · **`mapRect(PkRect)` 与 `mapRect(PkRectF)` 语义不同**：整数版四角取 qRound
//     且用 `right()+1` / `bottom()+1`（差一补偿），浮点版直接用 x+w / y+h。
//
// ── 惰性缓存（m_type / m_dirty）：**必须复刻，它是可观测语义** ──────────────
//
// Qt 把「这个矩阵是什么档位」缓存在 `m_type` 里，把「上次算完之后发生过多大
// 级别的改动」记在 `m_dirty` 里，`type()` 的第一行是
//     if (m_dirty == TxNone || m_dirty < m_type) return m_type;
// 也就是说**改动级别低于当前档位时直接返回旧值，不重算**。这不是纯粹的性能
// 手段 —— 旧值可能已经过期，而过期值是从公开 API 看得见的。
//
// 实测（探针 probe_transform.cpp §A，真 Qt 5.15.7，输出贴在 Task 6 报告 §1）：
//     QTransform t(2,0,0, 0,2,0, 0,0,2);
//     t.type();                     // → 16 (TxProject)，顺带把 m_dirty 清零
//     t *= 0.5;                     // 矩阵变成**单位阵**；operator*=(qreal) 只把
//                                   //   m_dirty 抬到 TxScale(2)
//     t.type();                     // → **16 (TxProject)**，isIdentity() == false
// 而**中间不问那一次 type()** 的同一条序列：
//     QTransform t(2,0,0, 0,2,0, 0,0,2); t *= 0.5; t.type();   // → 0 (TxNone)
//
// 同一个矩阵、同一串操作，只因为中间问过一次 type()，答案就从 TxNone 变成
// TxProject，`isIdentity()` 从 true 变成 false，`map()` 走的分支也跟着变
// （inline_type() 转 type()）。所以「照直算 type()」会在这一整类序列上与 Qt
// 分家 —— 计划里"缓存不是可观测语义"那句是**错的**，本文件按实测走：
// **m_type / m_dirty 连同每个成员对它们的写法逐字照抄**。
//
// ── 与 Qt 的三处登记在案的形态差异（**都不构成行为差异**）─────────────────
//
//   ① **不用 QMatrix。** Qt 的 QTransform 内部存的是 `QMatrix affine` 加
//      m_13/m_23/m_33；QMatrix 是 Qt5 已废弃的类型，`toAffine()` 实测 0 用量、
//      不实现，所以这里把 affine 的六个分量摊成 m_11/m_12/m_21/m_22/m_dx/m_dy。
//      `affine.inverted(&inv)` 那一跳照 qmatrix.cpp:947-965 就地展开成
//      invertedAffine()（**判据仍是 `dtr == 0.0` 精确零**，不是 qFuzzyIsNull）。
//   ② **不留 `Private *d`。** Qt5 的 QTransform 尾部有一个永远是 nullptr 的
//      `Private *d`（Qt6 已删）。它不经任何 API 露出来，这里不留 ——
//      代价是 `sizeof(PkTransform) != sizeof(QTransform)`，所以对拍里
//      **没有** sizeof 相等的 static_assert（Point/Size/Rect 三族都有）。
//   ③ **不复刻 `#ifndef QT_NO_DEBUG` 的 NaN 早退分支。** qtransform.cpp 里
//      translate/scale/shear/rotate/rotateRadians/fromTranslate/fromScale 七个
//      都有 `if (qIsNaN(...)) { nanWarning(); return; }`，**只在非 QT_NO_DEBUG
//      构建里存在**。实测本机 libQt5Gui.so 是带 QT_NO_DEBUG 编的（探针 §B：
//      `translate(NaN,1)` 之后 dx == nan，说明早退分支不在），Krita 的发布构建
//      同样带 QT_NO_DEBUG（与 PkSize 那边 Q_ASSERT 的处置同一条口径）。
//      于是这里不写这七个分支，取值与实测的 .so 逐字一致。
//
// Qt 宏到 C++17 的映射（无行为差异，README 偏离清单里登记）：
//   Q_GUI_EXPORT / Q_DECLARE_TYPEINFO / Q_REQUIRED_RESULT / Q_FALLTHROUGH → 去掉
//   uint → unsigned（位域宽度 5 照抄）
//
// ── 明确不实现（逐条都有依据，完整归属表在 README）────────────────────────
//
// 【实测用量 0，判据①「一项不多」】（R-03.md §「用量为 0、明确不实现」）：
//   · `adjoint` —— 但 `inverted` 的 TxProject 路径要用它，所以它作为**私有**
//     helper 存在，不进公开面（Qt 那边是公开的）
//   · `isInvertible` `isRotating` `isScaling` `isTranslating` `mapToPolygon`
//     `quadToQuad` `toAffine` `det`
// 【依赖 R-03 范围外的类型】：
//   · `map(QLine)` `map(QLineF)` `map(QPolygon)` `map(QPolygonF)`
//     `map(QRegion)` `map(QPainterPath)` 六个重载，以及
//     `operator*(const QLine&/QLineF&/QPolygon&/QPolygonF&/QRegion&, const QTransform&)`
//     —— QLine/QLineF/QPolygon/QPolygonF/QRegion/QPainterPath 都不在
//     `Qt替代品选型.md` §1 几何那一行点名的四个类型里，**归属未定**
//   · `squareToQuad` / `quadToSquare`（实测 2 次 / 3 次，用量 > 0）——
//     它们的签名吃 `QPolygonF`，同上不可得。**这是一个已知缺口，不是遗漏**，
//     报回主会话，README 覆盖度缺口点名
//   · `mapRect` 在「TxProject 且需要透视裁剪」时 Qt 走 QPainterPath ——
//     见 PkTransform.cpp 里 mapRect 上方那段与 oracle/geometry.deviation
// 【归别的线】：
//   · `qHash`（R-02 容器）、`QDataStream operator<<>>`（R-12 端口）、
//     `QDebug operator<<`（R-08 日志）、`operator QVariant()`（QVariant 不在范围）
//   · `explicit QTransform(Qt::Initialization)` —— Qt::Uninitialized 归 R-02，
//     且实测 0 用量
//   · `QTransform(const QMatrix&)` —— QMatrix 是 Qt5 已废弃类型，0 用量
// 【交给编译器生成】：
//   · 拷贝/移动构造与拷贝/移动赋值。Qt 为 Qt5 手写了 memcpy 版并在注释里写着
//     "### Qt 6: remove; the compiler-generated ones are fine!"。本类的九个
//     double 加两个位域都是平凡可复制的，编译器生成的逐成员拷贝与 memcpy
//     取值一致（**m_type / m_dirty 一并拷贝**，与 qtransform.cpp:1056 的
//     手写 operator= 逐字段一致）。
// ---------------------------------------------------------------------------

class PkTransform
{
public:
    // qtransform.h:59-66 —— **位标志，不是 0..5**。
    enum TransformationType {
        TxNone      = 0x00,
        TxTranslate = 0x01,
        TxScale     = 0x02,
        TxRotate    = 0x04,
        TxShear     = 0x08,
        TxProject   = 0x10
    };

    // qtransform.cpp:284-293 / 303-314 / 323-333。三个公开构造的
    // m_type / m_dirty 初值**各不相同**，而它决定了此后第一次 type() 从哪一档
    // 开始重算：默认构造 (TxNone,TxNone)、九参 (TxNone,TxProject)、
    // 六参 (TxNone,TxShear)。抄错这两个数就等于抄错了 type()。
    PkTransform();
    PkTransform(qreal h11, qreal h12, qreal h13,
                qreal h21, qreal h22, qreal h23,
                qreal h31, qreal h32, qreal h33 = 1.0);
    PkTransform(qreal h11, qreal h12, qreal h21,
                qreal h22, qreal dx, qreal dy);

    // qtransform.h:222-229。两条都走 inline_type()（会触发惰性重算）。
    bool isAffine() const;
    bool isIdentity() const;

    // qtransform.cpp:2135-2177。**有状态**，见文件头「惰性缓存」。
    TransformationType type() const;

    // qtransform.h:250-254。展开顺序照抄 —— 浮点加减不结合，换个写法就换个取值。
    qreal determinant() const;

    // qtransform.h:261-304。十一个一行取值器。
    qreal m11() const;
    qreal m12() const;
    qreal m13() const;
    qreal m21() const;
    qreal m22() const;
    qreal m23() const;
    qreal m31() const;
    qreal m32() const;
    qreal m33() const;
    qreal dx() const;
    qreal dy() const;

    // qtransform.cpp:1923-1932。m_type=TxNone、m_dirty=TxProject（全量重算）。
    void setMatrix(qreal m11, qreal m12, qreal m13,
                   qreal m21, qreal m22, qreal m23,
                   qreal m31, qreal m32, qreal m33);

    // qtransform.cpp:400-445 / 382-388。
    PkTransform inverted(bool *invertible = nullptr) const;
    PkTransform transposed() const;

    // qtransform.cpp:453-489 / 521-555 / 587-629 / 648-725 / 741-803。
    // 五个都按 inline_type() 分档走不同公式，且末尾都是
    // `if (m_dirty < TxN) m_dirty = TxN;`（**不是**无条件赋值）。
    PkTransform &translate(qreal dx, qreal dy);
    PkTransform &scale(qreal sx, qreal sy);
    PkTransform &shear(qreal sh, qreal sv);
    PkTransform &rotate(qreal a, Qt::Axis axis = Qt::ZAxis);
    PkTransform &rotateRadians(qreal a, Qt::Axis axis = Qt::ZAxis);

    // qtransform.cpp:810-821 / 851-854。**只比九个分量，不比 m_type/m_dirty**，
    // 且用的是裸 `==`（不是 qFuzzyCompare）—— 于是 NaN 矩阵永远不等于自己，
    // ±0.0 判等。自由函数 qFuzzyCompare(PkTransform,PkTransform) 才是模糊版。
    bool operator==(const PkTransform &) const;
    bool operator!=(const PkTransform &) const;

    // qtransform.cpp:863-935 / 945-1018。**两者不是同一份代码**：`operator*`
    // 在 TxTranslate 分支上用 `+=` 写 dy（见文件头），`operator*=` 用 `+=` 写
    // 两个分量。乘法顺序是 `this` 在左：实测 fromScale(2,3)*fromTranslate(4,5)
    // 的 dx=4，而 fromTranslate(4,5)*fromScale(2,3) 的 dx=8。
    PkTransform &operator*=(const PkTransform &);
    PkTransform operator*(const PkTransform &o) const;

    // qtransform.cpp:1082-1088。
    void reset();

    // qtransform.cpp:1189-1222 / 1240-1273 / 2086-2107。
    // **四个重载分两族**：map(PkPoint)/map(PkPointF) 自己写死的分档里
    // `w = 1./(m13*x+m23*y+m33)` **不夹持**；map(int*)/map(qreal*) 走 MAP 宏、
    // 夹持 `w < 1e-6 -> w = 1e-6`。见文件头。
    PkPoint map(const PkPoint &p) const;
    PkPointF map(const PkPointF &p) const;
    void map(int x, int y, int *tx, int *ty) const;
    void map(qreal x, qreal y, qreal *tx, qreal *ty) const;

    // qtransform.cpp:1942-1991 / 2012-2060。
    PkRect mapRect(const PkRect &) const;
    PkRectF mapRect(const PkRectF &) const;

    // qtransform.h:311-366。四个标量运算符**对 m_dirty 的写法互不相同**：
    // `*=` 抬到 TxScale（`if (m_dirty < TxScale)`）、`/=` 转发给 `*=`、
    // `+=` 与 `-=` **无条件**钉成 TxProject。而且三个都有提前返回
    // （`*=` 的 num==1、`/=` 的 div==0、`+=`/`-=` 的 num==0），
    // 提前返回时 m_dirty **一个字都不动**。
    PkTransform &operator*=(qreal div);
    PkTransform &operator/=(qreal div);
    PkTransform &operator+=(qreal div);
    PkTransform &operator-=(qreal div);

    // qtransform.cpp:498-513 / 564-579。**它们不走构造函数那条重算路径**：
    // 直接把 m_type 钉成 TxNone/TxTranslate（或 TxNone/TxScale）、m_dirty 钉成
    // TxNone。于是 `fromScale(1,1).type()` 是 TxNone 而不需要重算（实测 §A6）。
    static PkTransform fromTranslate(qreal dx, qreal dy);
    static PkTransform fromScale(qreal dx, qreal dy);

private:
    // qtransform.h:175-196 —— 两个私有构造。带 bool 的十参构造把 m_dirty 钉成
    // TxProject，无参数的 PkTransform(bool) 造单位阵且 m_dirty=TxNone。
    // ⚠ **PkTransform(bool) 造出来的是单位阵，不是未初始化**（QMatrix(bool) 的
    // 六个字段是 1,0,0,1,0,0）。operator* 里 `t.m_dy += ...` 那个 `+=` 的左值
    // 初值就来自这里。
    PkTransform(qreal h11, qreal h12, qreal h13,
                qreal h21, qreal h22, qreal h23,
                qreal h31, qreal h32, qreal h33, bool);
    explicit PkTransform(bool);

    // qtransform.h:215-220。
    TransformationType inline_type() const;

    // qtransform.cpp:359-377。Qt 那边是公开 API，实测 0 用量，这里降成私有
    // helper —— inverted() 的 TxProject 路径要用它。
    PkTransform adjoint() const;

    // qmatrix.cpp:947-965 就地展开。**判据是 `dtr == 0.0` 精确零**，
    // 与 inverted() 另外两条路径的 qFuzzyIsNull 门槛不同，别统一。
    void invertedAffine(PkTransform &out, bool *invertible) const;

    // 四角包围盒 —— qtransform.cpp:1963-1985 与 2033-2054 那一段。抽成成员是为了
    // 让 mapRect 保住 Qt 原本的**三分支**结构（`t <= TxTranslate` / `t <= TxScale` /
    // `t < TxProject || !needsPerspectiveClipping(...)` / else），
    // 而那第四支（Qt 走 QPainterPath 的那一支）在本类里落回同一个四角包围盒。
    // 不抽的话只能把两段一模一样的代码写两遍、或者把分支合掉 ——
    // 合掉之后**那条已声明的偏离在代码里就不显形了**。
    PkRect mapRectCorners(const PkRect &rect, TransformationType t) const;
    PkRectF mapRectCorners(const PkRectF &rect, TransformationType t) const;

    qreal m_11, m_12;
    qreal m_21, m_22;
    qreal m_dx, m_dy;
    qreal m_13;
    qreal m_23;
    qreal m_33;

    mutable unsigned m_type : 5;
    mutable unsigned m_dirty : 5;
};

// ---------------------------------------------------------------------------
// 内联部分 —— 与 qtransform.h 的 `/******* inlines *****/` 一节一一对应。
// 放在类体外不是风格：oracle/run_oracle.sh 的规则三闸门按「类体里的每条声明
// 都要在 transform_api.map 里有一行」对账，它靠花括号计数剥函数体，类体里少放
// 一个花括号就少一分解析风险。Qt 自己也是这么排的。
// ---------------------------------------------------------------------------

// qtransform.h:215-220
inline PkTransform::TransformationType PkTransform::inline_type() const
{
    if (m_dirty == TxNone)
        return static_cast<TransformationType>(m_type);
    return type();
}

// qtransform.h:222-229
inline bool PkTransform::isAffine() const
{
    return inline_type() < TxProject;
}
inline bool PkTransform::isIdentity() const
{
    return inline_type() == TxNone;
}

// qtransform.h:250-254 —— 展开顺序逐字照抄（浮点加减不结合）。
inline qreal PkTransform::determinant() const
{
    return m_11 * (m_33 * m_22 - m_dy * m_23) -
        m_21 * (m_33 * m_12 - m_dy * m_13) + m_dx * (m_23 * m_12 - m_22 * m_13);
}

// qtransform.h:261-304
inline qreal PkTransform::m11() const { return m_11; }
inline qreal PkTransform::m12() const { return m_12; }
inline qreal PkTransform::m13() const { return m_13; }
inline qreal PkTransform::m21() const { return m_21; }
inline qreal PkTransform::m22() const { return m_22; }
inline qreal PkTransform::m23() const { return m_23; }
inline qreal PkTransform::m31() const { return m_dx; }
inline qreal PkTransform::m32() const { return m_dy; }
inline qreal PkTransform::m33() const { return m_33; }
inline qreal PkTransform::dx() const { return m_dx; }
inline qreal PkTransform::dy() const { return m_dy; }

// qtransform.h:311-327 —— ⚠ 提前返回时 m_dirty 不动；`if (m_dirty < TxScale)`
// 是条件抬升而不是赋值。两条都参与「过期缓存」那类可观测行为。
inline PkTransform &PkTransform::operator*=(qreal num)
{
    if (num == 1.)
        return *this;
    m_11 *= num;
    m_12 *= num;
    m_13 *= num;
    m_21 *= num;
    m_22 *= num;
    m_23 *= num;
    m_dx *= num;
    m_dy *= num;
    m_33 *= num;
    if (m_dirty < TxScale)
        m_dirty = TxScale;
    return *this;
}
// qtransform.h:328-334 —— ⚠ `div == 0` 时**原样返回**（不是造 inf），
// 且转成乘以倒数（`1/div` 再乘，不是逐个除）—— 取值与逐个除不同。
inline PkTransform &PkTransform::operator/=(qreal div)
{
    if (div == 0)
        return *this;
    div = 1 / div;
    return operator*=(div);
}
// qtransform.h:335-350 —— m_dirty **无条件**钉成 TxProject。
inline PkTransform &PkTransform::operator+=(qreal num)
{
    if (num == 0)
        return *this;
    m_11 += num;
    m_12 += num;
    m_13 += num;
    m_21 += num;
    m_22 += num;
    m_23 += num;
    m_dx += num;
    m_dy += num;
    m_33 += num;
    m_dirty = TxProject;
    return *this;
}
// qtransform.h:351-366
inline PkTransform &PkTransform::operator-=(qreal num)
{
    if (num == 0)
        return *this;
    m_11 -= num;
    m_12 -= num;
    m_13 -= num;
    m_21 -= num;
    m_22 -= num;
    m_23 -= num;
    m_dx -= num;
    m_dy -= num;
    m_33 -= num;
    m_dirty = TxProject;
    return *this;
}

// qtransform.h:370-381 —— 自由函数，九个分量逐个 qFuzzyCompare。
// ⚠ 用 pkQtFuzzyCompare 而不是 qFuzzyCompare：后者在「pk/test 的垫片先进 TU」
// 那条路径上是个 #define，会把这里的公式静默换成 pk/test 那套（理由与
// PkPointF::operator== 相同，见 PkGlobal.h 的 pkQt* 一节）。
inline bool qFuzzyCompare(const PkTransform &t1, const PkTransform &t2)
{
    return pkQtFuzzyCompare(t1.m11(), t2.m11())
        && pkQtFuzzyCompare(t1.m12(), t2.m12())
        && pkQtFuzzyCompare(t1.m13(), t2.m13())
        && pkQtFuzzyCompare(t1.m21(), t2.m21())
        && pkQtFuzzyCompare(t1.m22(), t2.m22())
        && pkQtFuzzyCompare(t1.m23(), t2.m23())
        && pkQtFuzzyCompare(t1.m31(), t2.m31())
        && pkQtFuzzyCompare(t1.m32(), t2.m32())
        && pkQtFuzzyCompare(t1.m33(), t2.m33());
}

// qtransform.h:395-418 —— "mathematical semantics" 一节里**本 Task 范围内**的
// 六个。QLine / QLineF / QPolygon / QPolygonF / QRegion 五个重载不做（类型不在
// R-03 范围，见文件头）。
inline PkPoint operator*(const PkPoint &p, const PkTransform &m)
{ return m.map(p); }
inline PkPointF operator*(const PkPointF &p, const PkTransform &m)
{ return m.map(p); }

inline PkTransform operator*(const PkTransform &a, qreal n)
{ PkTransform t(a); t *= n; return t; }
inline PkTransform operator/(const PkTransform &a, qreal n)
{ PkTransform t(a); t /= n; return t; }
inline PkTransform operator+(const PkTransform &a, qreal n)
{ PkTransform t(a); t += n; return t; }
inline PkTransform operator-(const PkTransform &a, qreal n)
{ PkTransform t(a); t -= n; return t; }

#endif // PK_GEOMETRY_PKTRANSFORM_H
