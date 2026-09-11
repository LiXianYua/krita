#pragma once
// ⚠ S-18：pk/geometry 的 Bezier 辅助，**照 Qt 的 `qbezier_p.h` 的形制**
//（qtbase 5.15 `src/gui/painting/qbezier_p.h`）—— 上游就是把 `QBezier` 放在
// 这个私有头里给 `qtransform.cpp` 与 `qpainterpath.cpp` **共用**。S-18 实现
// `mapProjective` 时对上了同一个需求：`cubicTo_clipped` 要把曲线按 flattening
// 阈值摊成折线（`QBezier::toPolygon`），而那套拆分数学本来只在本模块的
// `PkPainterPath.cpp` 里、是文件局部的。
//
// ⚠ **只搬不写**：下面每一个函数的算法都逐字来自上游 `qbezier_p.h` /
// `qbezier.cpp`（tag `v5.15.7-lts-lgpl`），包括 `pkBezierCoefficients` 的三行
// 系数式与 `split` 的中点式。原有的 license/出处登记在 `PkPainterPath.cpp`
// 文件头（含 `qbezier_p.h`、`qbezier.cpp` 两条），随本块一并适用。
//
// 分布与上游一致：平凡成员 inline 在头里；重活（pointAt / bounds / mapBy /
// getSubRange / tForY / stationaryYPoints / bezierOnInterval /
// parameterSplitLeft）**定义仍在 `PkPainterPath.cpp`**，这里只声明。

#include "PkPoint.h"
#include "PkRect.h"

// ⚠ 本头只在**声明**里用 `PkTransform`（`PkArcBezier::mapBy` 的引用形参），
// 引用形参要的是完整类型之外的"已声明"，所以这里前向声明就够、不必 include
// `PkTransform.h`（那会把 PkTransform 整包拉进每个 include 本头的 TU）。
// 两个使用方（`PkPainterPath.cpp`、`PkTransform.cpp`）都本来就 include 了
// `PkTransform.h`，定义那份（`mapBy`）在 `PkPainterPath.cpp` 里。
// 加这一行是为了**本头自足** —— 原先它靠 PkPoint.h/PkRect.h 的传递 include
// 才编得过，换个 include 顺序就会炸（评审 S-18 指出这处没核到底）。
class PkTransform;

struct PkArcBezier {
    qreal x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0, x4 = 0, y4 = 0;

    static PkArcBezier fromPoints(const PkPointF &p1, const PkPointF &p2,
                                   const PkPointF &p3, const PkPointF &p4)
    { return {p1.x(), p1.y(), p2.x(), p2.y(), p3.x(), p3.y(), p4.x(), p4.y()}; }

    PkPointF pt1() const { return PkPointF(x1, y1); }
    PkPointF pt2() const { return PkPointF(x2, y2); }
    PkPointF pt3() const { return PkPointF(x3, y3); }
    PkPointF pt4() const { return PkPointF(x4, y4); }

    PkPointF pointAt(qreal t) const;
    PkRectF bounds() const;
    PkArcBezier mapBy(const PkTransform &transform) const;
    PkArcBezier getSubRange(qreal t0, qreal t1) const;
    qreal tForY(qreal t0, qreal t1, qreal y) const;
    int stationaryYPoints(qreal &t0, qreal &t1) const;

    PkArcBezier bezierOnInterval(qreal t0, qreal t1) const;
    void parameterSplitLeft(qreal t, PkArcBezier *left);
};

struct PkArcBezierSplit { PkArcBezier first, second; };

static inline void pkBezierCoefficients(qreal t, qreal &a, qreal &b, qreal &c, qreal &d)
{
    qreal m_t = 1. - t;
    b = m_t * m_t; c = t * t; d = c * t;
    a = b * m_t; b *= 3. * t; c *= 3. * m_t;
}

// qtbase `QBezier::split()` 的等价物（上游在 qbezier.cpp；这里 inline 进头，
// 因为两个 TU 都要用）。
inline PkArcBezierSplit pkSplitBezier(const PkArcBezier &b)
{
    PkPointF mid12((b.x1 + b.x2) * 0.5, (b.y1 + b.y2) * 0.5);
    PkPointF mid23((b.x2 + b.x3) * 0.5, (b.y2 + b.y3) * 0.5);
    PkPointF mid34((b.x3 + b.x4) * 0.5, (b.y3 + b.y4) * 0.5);
    PkPointF mid1223((mid12.x() + mid23.x()) * 0.5, (mid12.y() + mid23.y()) * 0.5);
    PkPointF mid2334((mid23.x() + mid34.x()) * 0.5, (mid23.y() + mid34.y()) * 0.5);
    PkPointF mid((mid1223.x() + mid2334.x()) * 0.5, (mid1223.y() + mid2334.y()) * 0.5);
    return {PkArcBezier::fromPoints(PkPointF(b.x1,b.y1), mid12, mid1223, mid),
            PkArcBezier::fromPoints(mid, mid2334, mid34, PkPointF(b.x4,b.y4))};
}
