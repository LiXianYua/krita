#include "PkPolygon.h"

// ⚠ **这个系统头必须在 oracle/geometry_difftest.cpp 顶部的系统头区里也出现过**
// —— 那份对拍把本 .cpp `#include` 进 `namespace pkoracle {}` 里，理由与
// PkLine.cpp/PkRect.cpp 顶部同一条纪律。<type_traits> 是尾部 static_assert 要的。
#include <type_traits>

// ---------------------------------------------------------------------------
// PkPolygonF 的 out-of-line 成员：PkPolygonF(const PkRectF&) / boundingRect /
// containsPoint / toPolygon / translate(const PkPointF&) /
// translated(const PkPointF&)。**逐字抄自 qtbase 标签 v5.15.7-lts-lgpl 的
// src/gui/painting/qpolygon.cpp**（本机装的 Qt 只有 .so；源码取自上游同版本
// 标签，取值靠 tests/ 与 oracle/ 逐输入核对）。来源行号标在各项上方。
// ---------------------------------------------------------------------------

// qpolygon.cpp:55-83 —— `qt_polygon_isect_line` 的零 Qt 版本。containsPoint
// 的射线穿越/环绕数算法核心：给定一条边 (p1,p2) 与被测点 pos，累加这条边对
// pos 所在水平射线的**穿越方向**（不是穿越次数——同向穿越会相加、异向穿越
// 会抵消，环绕数规则要的正是这个带符号累加）。
//
// ⚠ **水平线直接跳过**（`pkQtFuzzyCompare(y1,y2)` 为真时 return），照抄注释
// 原文"ignore horizontal lines according to scan conversion rule"——不是
// 遗漏，是刻意的扫描线约定：水平边不产生穿越，处理它反而会在端点重合处重复
// 计数。
// ⚠ **走 pkQtFuzzyCompare 而不是 qFuzzyCompare**：与本仓库几何类型内部的既有
// 纪律相同（PkPointF::operator== 那条注释），函数体里出现 qFuzzyCompare 会在
// 「pk/test 那份垫片先进 TU」的路径上被预处理器悄悄改写成另一套公式。
// ⚠ **`y >= y1 && y < y2` 半开区间**：这正是「顶点重合时不重复计数」的关键
// ——一条边的终点恰好是下一条边的起点时，只有其中一条边会把 y==该点纳入
// 判定区间，逐字照抄这个不对称的开闭端。
static void pkPolygonIsectLine(const PkPointF &p1, const PkPointF &p2,
                                const PkPointF &pos, int *winding)
{
    qreal x1 = p1.x();
    qreal y1 = p1.y();
    qreal x2 = p2.x();
    qreal y2 = p2.y();
    qreal y = pos.y();

    int dir = 1;

    if (pkQtFuzzyCompare(y1, y2)) {
        // ignore horizontal lines according to scan conversion rule
        return;
    } else if (y2 < y1) {
        qreal x_tmp = x2; x2 = x1; x1 = x_tmp;
        qreal y_tmp = y2; y2 = y1; y1 = y_tmp;
        dir = -1;
    }

    if (y >= y1 && y < y2) {
        qreal x = x1 + ((x2 - x1) / (y2 - y1)) * (y - y1);

        // count up the winding number if we're
        if (x <= pos.x()) {
            (*winding) += dir;
        }
    }
}

// qpolygon.cpp:557-565。矩形四个顶点、顺时针（左上→右上→右下→左下），
// 首尾闭合回左上角，共 5 个点——不是 4 个。真实调用点见 PkPolygon.h 文件头。
PkPolygonF::PkPolygonF(const PkRectF &r)
{
    reserve(5);
    append(PkPointF(r.x(), r.y()));
    append(PkPointF(r.x() + r.width(), r.y()));
    append(PkPointF(r.x() + r.width(), r.y() + r.height()));
    append(PkPointF(r.x(), r.y() + r.height()));
    append(PkPointF(r.x(), r.y()));
}

// qpolygon.cpp:596-607。offset.isNull() 时提前返回（不做零位移的整趟遍历）。
void PkPolygonF::translate(const PkPointF &offset)
{
    if (offset.isNull())
        return;

    PkPointF *p = data();
    int i = size();
    while (i--) {
        *p += offset;
        ++p;
    }
}

// qpolygon.cpp:624-629。
PkPolygonF PkPolygonF::translated(const PkPointF &offset) const
{
    PkPolygonF copy(*this);
    copy.translate(offset);
    return copy;
}

// qpolygon.cpp:658-680。空多边形返回 (0,0,0,0)（不是默认构造的 PkRectF()，
// 两者在这个类型上恰好取值相同，但逐字照抄 Qt 的显式写法）。
PkRectF PkPolygonF::boundingRect() const
{
    const PkPointF *pd = constData();
    const PkPointF *pe = pd + size();
    if (pd == pe)
        return PkRectF(0, 0, 0, 0);
    qreal minx, maxx, miny, maxy;
    minx = maxx = pd->x();
    miny = maxy = pd->y();
    ++pd;
    while (pd != pe) {
        if (pd->x() < minx)
            minx = pd->x();
        else if (pd->x() > maxx)
            maxx = pd->x();
        if (pd->y() < miny)
            miny = pd->y();
        else if (pd->y() > maxy)
            maxy = pd->y();
        ++pd;
    }
    return PkRectF(minx, miny, maxx - minx, maxy - miny);
}

// qpolygon.cpp:689-696。
PkPolygon PkPolygonF::toPolygon() const
{
    PkPolygon a;
    a.reserve(size());
    for (int i = 0; i < size(); ++i)
        a.append(at(i).toPoint());
    return a;
}

// qpolygon.cpp:831-853。空多边形直接 false；从第二个点起逐边喂
// pkPolygonIsectLine，最后**隐式闭合**最后一段子路径（last_pt != last_start
// 时补一条 last_pt→last_start 的边）——调用点传入的多边形不要求自己闭合。
// WindingFill 看 winding_number != 0（环绕数非零即内部）；OddEvenFill 看
// winding_number 的奇偶（这里用的是同一个 winding_number 累加值取模，不是
// 分开维护一个"穿越次数"——qpolygon.cpp 就是这么写的，两种规则共用一次遍历）。
bool PkPolygonF::containsPoint(const PkPointF &pt, Qt::FillRule fillRule) const
{
    if (isEmpty())
        return false;

    int winding_number = 0;

    PkPointF last_pt = at(0);
    PkPointF last_start = at(0);
    for (int i = 1; i < size(); ++i) {
        const PkPointF &e = at(i);
        pkPolygonIsectLine(last_pt, e, pt, &winding_number);
        last_pt = e;
    }

    // implicitly close last subpath
    if (last_pt != last_start)
        pkPolygonIsectLine(last_pt, last_start, pt, &winding_number);

    return (fillRule == Qt::WindingFill
            ? (winding_number != 0)
            : ((winding_number % 2) != 0));
}

// ── 只在 TU 里落得了地的编译期断言 ──────────────────────────────────────

static_assert(std::is_standard_layout<PkPolygon>::value,
              "PkPolygon 必须是标准布局（继承 PkVector<PkPoint>，无新增字段）");
static_assert(std::is_standard_layout<PkPolygonF>::value,
              "PkPolygonF 必须是标准布局（继承 PkVector<PkPointF>，无新增字段）");
