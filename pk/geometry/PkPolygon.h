#ifndef PK_GEOMETRY_PKPOLYGON_H
#define PK_GEOMETRY_PKPOLYGON_H

#include "PkGlobal.h"
#include "PkPoint.h"
#include "PkRect.h"
#include "../container/PkVector.h"

// ---------------------------------------------------------------------------
// PkPolygon / PkPolygonF —— QPolygon / QPolygonF 的零 Qt 替代（R-21 T2）。
//
// **逐字抄自真 Qt 5.15.7** 的 include/QtGui/qpolygon.h（class 声明）与 qtbase
// 标签 `v5.15.7-lts-lgpl` 的 src/gui/painting/qpolygon.cpp（out-of-line 成员：
// 本机只有 .so 没有 qpolygon.cpp 源码，取自上游同版本标签，取值经 tests/ 与
// oracle/ 逐输入核对）。来源行号标在各项上方。
//
// ── `class QPolygon : public QVector<QPoint>`——继承，不是包一层 ──────────
//
// **这不是风格选择，是真实调用点逼出来的硬约束**（研究阶段已实测确认，见
// R-21 plan.md T2 小节「补充实测」）：
//   · `kis_convex_hull.cpp:60-74` 用 boost::geometry 的
//     `range_iterator<QPolygon>`/`range_const_iterator<QPolygon>` 特化，
//     直接写 `QPolygon::iterator`/`QPolygon::const_iterator`——这两个名字要
//     存在，唯一自然的来源是**公开继承** `PkVector<PkPoint>`（`PkArrayContainer`
//     里现成的 `iterator`/`const_iterator`/`begin()`/`end()` 标准迭代器接口）。
//   · `KisTofuGlyph.cpp` 里大量 `QPolygon a{QVector<QPoint>{{x,y},...}}`
//     初始化列表构造；`kis_convex_hull.cpp:82`
//     `boost::geometry::convex_hull(QPolygon(points), hull)`——`points` 是
//     `const QVector<QPoint>&`，要求非 explicit 的 `QPolygon(const QVector<QPoint>&)`。
//   · `libs/image/kis_outline_generator.h`、`kis_pixel_selection.h/.cpp`、
//     `KoPolygonUtils.h/.cpp` 把 `QPolygon`/`QPolygonF` 当 `QVector<QPolygon>`/
//     `QList<QPolygon>` 的元素类型——需要拷贝/移动语义与 `PkVector<T>` 一致
//     （编译器生成的、O(1) 的那套）。
// 包一层（组合）拿不到「继承的标准迭代器类型名」与「boost 的 range 概念」，
// 这两类真实调用点会直接编不过。
//
// ── PkPolygon（int）的范围按真实调用点砍到最小 ──────────────────────────
//
// 保留范围内 `QPolygon` 的真实用量集中在**构造 + 迭代**（`kis_convex_hull.cpp`、
// `inplace_transform_stroke_strategy.cpp:370` 同形态的
// `QPolygon(points)`/`QPolygon()`）。`.containsPoint(`/`.boundingRect(`/
// `.translate(`/`.translated(`/`.isClosed(`/`.toPolygon(`/`.united(`/
// `.intersected(`/`.subtracted(`/`.intersects(` 这十个成员**实测调用点全部
// 落在 `QPolygonF` 类型的接收者上**（逐个跟过声明行确认，如
// `psd_layer_section.cpp:792` 的 `poly` 是 `QPolygonF`、
// `KisToolKnife.cpp:110-115` 的 `polygon` 是 `QPolygonF`），`QPolygon`（int）
// 侧一个都没有。按判据①「一项不多」，`PkPolygon` 只做构造与继承来的迭代器/
// 容器操作，不重复声明这十个成员——它们只在 `PkPolygonF` 里实现。
// `QPolygon(const QRect&, bool closed=false)`、`point()`/`setPoint()`/
// `setPoints()`/`putPoints()` 四个下标式老接口实测调用点同样是 0，不实现。
//
// ── PkPolygonF 的范围：任务清单 + 实测证伪补齐（同 T1 `center()`/`fromPolar()`
//    的处置模式）──────────────────────────────────────────────────────────
//
// 任务文件与 R-21 plan.md 明确点名的四项：
//   · `containsPoint(const QPointF&, Qt::FillRule)` —— 真实调用点 ≥15 处，
//     全部经 `Qt::FillRule` 落地（枚举本身见 PkGlobal.h）。
//   · `QPolygonF(const QRectF&)` 构造 —— 真实调用点不止任务文件点名的那一处
//     （`kis_perspective_transform_strategy.cpp:165`），实测另有
//     `KisHandlePainterHelper.cpp:62,82,343`、
//     `kis_free_transform_strategy.cpp:206`、
//     `DefaultTool.cpp:1439`、`SelectionDecorator.cpp:109`、
//     `RemoveGutterStrategy.cpp:81`、`VanishingPointAssistant.cc:100`、
//     `ParallelRulerAssistant.cc:90`、`TwoPointAssistant.cc:238` 等 ≥10 处。
//   · `PkTransform::map(QPolygonF)`／`squareToQuad`／`quadToSquare`
//     —— 见 PkTransform.h。
//
// 任务清单没点名、**实测证伪按判据①补上**的四项（都不依赖 QPainterPath）：
//   · `boundingRect() const` —— 真实调用点 `kis_free_transform_strategy.cpp:172`
//     `boundsTransform.inverted().map(convexHull).boundingRect()`（`map()`
//     的返回值就是 `PkPolygonF`，这里立刻在其上调 `boundingRect()`——是
//     `map(QPolygonF)` 这条真实调用点链路的直接延伸，不补就是把同一条调用
//     链砍掉一半）、`kis_liquify_transform_worker.cpp:570`、
//     `kis_grid_interpolation_tools.h` 多处、`KisBezierGradientMesh.cpp:30`、
//     `kis_cage_transform_worker.cpp:150`、`kis_polygonal_gradient_shape_strategy.cpp`、
//     `psd_layer_section.cpp:792`、`KoSelection.cpp:68`、
//     `libs/image/tests/kis_algebra_2d_test.cpp:339` 等。
//   · `translate(const QPointF&)`/`translate(qreal,qreal)` —— 真实调用点
//     `KisHandlePainterHelper.cpp:83,85,174,218,344`。
//   · `translated(const QPointF&)`/`translated(qreal,qreal)` —— 真实调用点
//     `kis_cage_transform_worker.cpp:412`、
//     `KoSvgTextShapeLayoutFunc_inShape.cpp` 多处、
//     `KisHandlePainterHelper.cpp:154`。
//   · `isClosed() const` —— 真实调用点 `krita_utils.cpp:466,467`、
//     `kis_algebra_2d.cpp:310`。
//   · `toPolygon() const` —— 真实调用点
//     `kis_transform_mask_test.cpp:69,78,181,190`、
//     `KoSvgTextShapeLayoutFunc_inShape.cpp:50,76`。
//
// ── 明确不实现：`united`/`intersected`/`subtracted`/`intersects`（登记在案的
//    偏离，不是遗漏）───────────────────────────────────────────────────────
//
// 真实调用点确实存在（`kis_safe_transform.cpp:169,170,183,184`、
// `KoSvgTextShapeLayoutFunc_inShape.cpp:83,84`、
// `SelectionDecorator.cpp:109,110`、`VanishingPointAssistant.cc:100`、
// `ParallelRulerAssistant.cc:90`、`TwoPointAssistant.cc:238`），但真 Qt
// 5.15.7 的这四个成员**内部把多边形铺进 `QPainterPath`**、借它的布尔集合
// 运算实现（`QPainterPath subject; subject.addPolygon(*this); ...`，
// qpolygon.cpp:897 起）。`QPainterPath` 不在 R-21 交付范围（`Qt替代品选型.md`
// §1 几何那一行点名的十个类型里没有它，归 R-22），处置与 `PkTransform::mapRect`
// 在 `TxProject` 且需要透视裁剪时落回四角包围盒是**同一个模式**：这四个成员
// 与 `PkTransform::map(const PkPolygonF&)` 的 `TxProject` 分支一起，是本
// Task 与 Qt 的登记在案的行为偏离，详见 oracle/geometry.deviation 与
// README「覆盖度缺口」。依赖它们的调用点在本 Task 之后仍然编不过（那些文件
// 不在本 Task 的 graft/ 试接目标里）——这是诚实登记的缺口，不是静默遗漏。
// ---------------------------------------------------------------------------

class PkPolygon : public PkVector<PkPoint>
{
public:
    // ⚠ **三个构造全部只留声明，定义挪到类体外**（与 PkLineF(const PkLine&)
    // 同一处置，见该处注释）：run_oracle.sh 规则三闸门的声明解析器按花括号
    // 剥函数体，构造函数的初始化列表 `: PkVector<PkPoint>(v)` 写在类体内的
    // 花括号**之前**、含自己的圆括号，正则 `\(([^()]*)\)...$` 不支持嵌套
    // 括号，类内写内联体会让这条声明直接解析失败、掉进 miss、把整个闸门判
    // FAIL（实测踩过，三条构造一起失败——见 R-21 T2 报告）。
    PkPolygon();
    // qpolygon.h:62 —— 非 explicit：真实调用点
    // `kis_convex_hull.cpp:82`（`QPolygon(points)`，`points` 是
    // `const QVector<QPoint>&`）、`inplace_transform_stroke_strategy.cpp:370`
    // 同形态、`KisTofuGlyph.cpp` 的初始化列表构造（`QPolygon a{QVector<QPoint>{...}}`，
    // 外层花括号直接落到这个构造函数上）。
    PkPolygon(const PkVector<PkPoint> &v);
    // qpolygon.h:63 —— Qt 同时提供右值重载，本类对齐（拷贝/移动两条路都要通）。
    PkPolygon(PkVector<PkPoint> &&v) noexcept;

private:
    // PkPolygon 自己没有新增字段（存储全部继承自 PkVector<PkPoint>）。这个
    // private: 只是给 oracle/run_oracle.sh 规则三闸门的类体解析器占位——
    // 它按 `class X {(.*?)\n\s*private:` 定位 public 段结束，PkMargins.h:66-74
    // 已经有这个先例。
};

// ── PkPolygon 构造：类体外定义（理由见类体内声明上方的注释）─────────────

// qpolygon.h:59 —— Qt 原文 `inline QPolygon() {}`，空函数体不做任何初始化
// （基类 PkVector<PkPoint>() = default 已经给出空容器语义）。
inline PkPolygon::PkPolygon() {}

inline PkPolygon::PkPolygon(const PkVector<PkPoint> &v) : PkVector<PkPoint>(v) {}

inline PkPolygon::PkPolygon(PkVector<PkPoint> &&v) noexcept : PkVector<PkPoint>(std::move(v)) {}

// ---------------------------------------------------------------------------
// PkPolygonF —— 逐字抄自 qpolygon.h 的 QPolygonF 那一半 + qpolygon.cpp 的
// out-of-line 成员（`PkPolygonF(const PkRectF&)`/`boundingRect`/
// `containsPoint`/`toPolygon`/`translate(const PkPointF&)`/
// `translated(const PkPointF&)`，定义见 PkPolygon.cpp）。
// ---------------------------------------------------------------------------

class PkPolygonF : public PkVector<PkPointF>
{
public:
    // ⚠ **前三个构造只留声明，定义挪到类体外**——同 PkPolygon 的三个构造
    // 同一个理由（初始化列表的圆括号会让规则三闸门的声明解析器 miss）。
    PkPolygonF();

    // qpolygon.h:142 —— explicit int 版：真实调用点 0，但**本类自己的
    // `PkTransform::map(const PkPolygonF&)` 实现要用它**按目标 size 预分配
    // （真 Qt 的 map(QPolygonF) 就是这么写的，qtransform.cpp:1476）。Qt 这个
    // 签名本身是 public 的，保留 public 面与 Qt 一致，不降级成 helper。
    explicit PkPolygonF(int size);

    // qpolygon.h:143 —— 非 explicit：真实调用点 ≥10 处（见文件头）。
    PkPolygonF(const PkVector<PkPointF> &v);
    PkPolygonF(PkVector<PkPointF> &&v) noexcept;

    // qpolygon.h:145 —— out-of-line（qpolygon.cpp:557-565）：矩形四个顶点
    // 顺时针、首尾闭合成 5 个点。真实调用点 ≥10 处，见文件头。
    PkPolygonF(const PkRectF &r);

    // qpolygon.h:155-159 —— (dx,dy) 两个重载是 inline 转发（qpolygon.h 本身
    // 就是内联的），offset 版是 out-of-line（qpolygon.cpp）。真实调用点见
    // 文件头（translate: KisHandlePainterHelper.cpp；translated:
    // kis_cage_transform_worker.cpp 等）。
    inline void translate(qreal dx, qreal dy);
    void translate(const PkPointF &offset);

    inline PkPolygonF translated(qreal dx, qreal dy) const;
    PkPolygonF translated(const PkPointF &offset) const;

    // qpolygon.h:163 —— inline，Qt 原文就是类体里的一行表达式。
    // 真实调用点 krita_utils.cpp:466,467、kis_algebra_2d.cpp:310。
    inline bool isClosed() const { return !isEmpty() && first() == last(); }

    // qpolygon.h:165 —— out-of-line（qpolygon.cpp:658-680）。真实调用点见
    // 文件头，含 `PkTransform::map(const PkPolygonF&)` 返回值上的直接延伸。
    PkRectF boundingRect() const;

    // qpolygon.h:167 —— out-of-line（qpolygon.cpp:831-853，射线穿越/环绕数
    // 算法，见 PkPolygon.cpp 顶部）。真实调用点 ≥15 处，见文件头。
    bool containsPoint(const PkPointF &pt, Qt::FillRule fillRule) const;

    // qpolygon.h:161 —— out-of-line（qpolygon.cpp:689-696）。真实调用点见
    // 文件头。
    PkPolygon toPolygon() const;

    // 明确不实现（见文件头「登记在案的偏离」）：
    //   united(const PkPolygonF&) / intersected(const PkPolygonF&) /
    //   subtracted(const PkPolygonF&) / intersects(const PkPolygonF&) const
    //   —— 真 Qt 内部铺进 QPainterPath 做布尔集合运算，QPainterPath 不在
    //   R-21 范围（归 R-22），处置同 PkTransform::mapRect 的 TxProject 分支。

private:
    // 同 PkPolygon：没有新增字段，private: 只为规则三闸门的类体解析器占位。
};

// ── PkPolygonF 构造：类体外定义（理由见类体内声明上方的注释）─────────────

// qpolygon.h:140 —— Qt 原文 `inline QPolygonF() {}`。
inline PkPolygonF::PkPolygonF() {}

inline PkPolygonF::PkPolygonF(int size) : PkVector<PkPointF>(size) {}

inline PkPolygonF::PkPolygonF(const PkVector<PkPointF> &v) : PkVector<PkPointF>(v) {}

inline PkPolygonF::PkPolygonF(PkVector<PkPointF> &&v) noexcept : PkVector<PkPointF>(std::move(v)) {}

// ── PkPolygonF inline（qpolygon.h:191-195）─────────────────────────────────

inline void PkPolygonF::translate(qreal dx, qreal dy)
{ translate(PkPointF(dx, dy)); }

inline PkPolygonF PkPolygonF::translated(qreal dx, qreal dy) const
{ return translated(PkPointF(dx, dy)); }

#endif // PK_GEOMETRY_PKPOLYGON_H
