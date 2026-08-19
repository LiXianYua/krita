#ifndef PK_GEOMETRY_PKREGION_H
#define PK_GEOMETRY_PKREGION_H

#include "PkGlobal.h"
#include "PkRect.h"

#include <vector>

// ---------------------------------------------------------------------------
// PkRegion —— QRegion 的零 Qt 替代（R-21 T5）。
//
// ⚠ **不逐位对齐 Qt 的内部矩形划分**（见 R-21 plan.md「问 4」的裁决）：
// Qt 内部用扫描线/XRegion 算法把矩形集合合并成一个**实现定义**的最小非重叠
// 矩形划分，那个具体划分不是规范承诺的公开语义（Qt 文档只保证覆盖面积正确，
// 不保证划分方式）。移植 Qt 的实际算法成本极高，与 `Qt替代品选型.md` §2 判
// "COW 容器"不值得移植是同一个理由。Krita 自己已有先例：`libs/global/KisRegion.h`
// （"An more efficient (and more limited) replacement for QRegion"）用
// `mergeSparseRects` 两趟合并，且明确讲清它的输出**不保证**与 QRegion 的划分
// 一致、只保证覆盖面积一致。
//
// **处置**：本类内部用矩形表 + 合并（同构 KisRegion 的设计），对拍只比较
// 「覆盖谓词」（`isEmpty`、`boundingRect`、任意点的 `contains`、面积、
// `rects()` 汇总之后的覆盖面积），**不比较 `rects()` 的逐条内容/顺序/个数**。
// 这是一条登记在案的偏离（理由充分），不是遗漏。
//
// ── 内部表示 ──────────────────────────────────────────────────────────────
//
// `std::vector<PkRect> m_rects`，维持**非重叠**不变式（重叠会双计面积、破坏
// 覆盖谓词）。并运算先减后加（避免与新矩形重叠）、差运算把每个矩形劈成至多
// 4 片、交运算逐对取交、最后做一趟相邻合并（水平+垂直，同 KisRegion）把
// rectCount 压下去。合并不是覆盖正确性的前提（非重叠才是），只为让
// `kis_painting_tweaks.cpp:28` 的「rectCount > 1000」告警不误触发。
//
// ── 范围（判据①，按真实调用点）────────────────────────────────────────────
//
// 实现：默认构造、`PkRegion(const PkRect&)`（隐式，真实调用点
// `QRegion dirtyRegion = realNodeRect;`）、`isEmpty`/`isNull`、`begin`/`end`
// 迭代（`const PkRect*`）、`rects()`（返回合并后的矩形表）、`rectCount`、
// `boundingRect`、`contains(PkPoint)`/`contains(PkRect)`、`translate`/
// `translated`、`operator+=`/`operator+`/`operator|=`/`operator|`（并）、
// `operator-=`/`operator-`（差）、`operator&=`/`operator&`（交）、
// `united`/`intersected`/`subtracted`/`xored`、`intersects`、`operator==`/
// `operator!=`。
//
// 不实现：`QRegion(const QPolygon&)` 多边形填充构造（真实调用点 0，且要依赖
// QPainterPath 填充算法）、`QRegion(const QBitmap&)`（QBitmap 不在范围）、
// `operator QVariant()`（PkVariant 是 R-06）、`QDataStream` 流式、`QDebug`。
// ---------------------------------------------------------------------------

class PkRegion
{
public:
    typedef const PkRect *const_iterator;

    PkRegion();
    // qregion.h —— 非 explicit：真实调用点 `QRegion dirtyRegion = realNodeRect;`
    // 靠它（QRect 隐式转 QRegion）。
    PkRegion(const PkRect &r);

    bool isEmpty() const;
    bool isNull() const;

    const_iterator begin() const;
    const_iterator end() const;

    bool contains(const PkPoint &p) const;
    bool contains(const PkRect &r) const;

    void translate(int dx, int dy);
    void translate(const PkPoint &p);
    PkRegion translated(int dx, int dy) const;
    PkRegion translated(const PkPoint &p) const;

    PkRegion united(const PkRegion &r) const;
    PkRegion united(const PkRect &r) const;
    PkRegion intersected(const PkRegion &r) const;
    PkRegion intersected(const PkRect &r) const;
    PkRegion subtracted(const PkRegion &r) const;
    PkRegion xored(const PkRegion &r) const;

    bool intersects(const PkRegion &r) const;
    bool intersects(const PkRect &r) const;

    PkRect boundingRect() const;

    // qregion.h —— Qt 里返回 QVector<QRect>（deprecated 但真实调用点
    // `krita_utils.cpp:90`、`kis_paint_device_strategies.h:264` 还在用）。
    // 返回合并后的非重叠矩形表。
    std::vector<PkRect> rects() const;
    int rectCount() const;

    PkRegion &operator|=(const PkRegion &r);
    PkRegion &operator+=(const PkRegion &r);
    PkRegion &operator+=(const PkRect &r);
    PkRegion &operator&=(const PkRegion &r);
    PkRegion &operator&=(const PkRect &r);
    PkRegion &operator-=(const PkRegion &r);
    PkRegion &operator-=(const PkRect &r);
    PkRegion &operator^=(const PkRegion &r);

    PkRegion operator|(const PkRegion &r) const;
    PkRegion operator+(const PkRegion &r) const;
    PkRegion operator+(const PkRect &r) const;
    PkRegion operator&(const PkRegion &r) const;
    PkRegion operator&(const PkRect &r) const;
    PkRegion operator-(const PkRegion &r) const;
    PkRegion operator-(const PkRect &r) const;
    PkRegion operator^(const PkRegion &r) const;

    bool operator==(const PkRegion &r) const;
    bool operator!=(const PkRegion &r) const;

private:
    // 内部：把一个矩形并进集合（先减后加，保持非重叠），随后不立即合并
    //（合并统一在末尾做，见下面 normalize()）。
    void unionRect(const PkRect &r);
    // 内部：从集合减去一个矩形（每个被劈成至多 4 片）。
    void subtractRect(const PkRect &r);
    // 内部：逐对取交。
    void intersectRect(const PkRect &r);
    // 内部：水平+垂直两趟相邻合并（同 KisRegion::mergeSparseRects 的精神）。
    void merge();

    std::vector<PkRect> m_rects;
};

#endif // PK_GEOMETRY_PKREGION_H
