#include "PkPainterPath.h"

#include <cmath>
#include <cfloat>
#include <algorithm>

// ⚠ 这个系统头必须在 oracle/geometry_difftest.cpp 顶部的系统头区里也出现过
// —— 那份对拍把本 .cpp `#include` 进 `namespace pkoracle {}` 里，理由同
// PkLine.cpp/PkRect.cpp 顶部同一条纪律。<type_traits> 是尾部 static_assert 要的。
#include <type_traits>

// ---------------------------------------------------------------------------
// PkPainterPath 实现 —— 逐字抄自真 Qt 5.15.7 的 qpainterpath.cpp。
//
// T1 交付：元素存储 + 构建核心 + 基础查询 + 变换 + 简单形状附加。
// 源码来自上游 tag `v5.15.7-lts-lgpl`，行号在各项上方。
// ---------------------------------------------------------------------------

// ============================================================================
// 构造/析构/拷贝
// ============================================================================

// qpainterpath.cpp:267-273
PkPainterPath::PkPainterPath() noexcept
    : m_fillRule(Qt::OddEvenFill)
{
}

// qpainterpath.cpp:275-281
PkPainterPath::PkPainterPath(const PkPointF &startPoint)
    : m_fillRule(Qt::OddEvenFill)
{
    moveTo(startPoint);
}

// ============================================================================
// swap
// ============================================================================

// qpainterpath.cpp:303-305
void PkPainterPath::swap(PkPainterPath &other) noexcept
{
    m_elements.swap(other.m_elements);
    std::swap(m_currentPos, other.m_currentPos);
    std::swap(m_cachedBounds, other.m_cachedBounds);
    std::swap(m_cachedControlRect, other.m_cachedControlRect);
    std::swap(m_dirtyBounds, other.m_dirtyBounds);
    std::swap(m_dirtyControlRect, other.m_dirtyControlRect);
    std::swap(m_fillRule, other.m_fillRule);
}

// ============================================================================
// 标记缓存为脏
// ============================================================================

void PkPainterPath::markDirty()
{
    m_dirtyBounds = true;
    m_dirtyControlRect = true;
}

// ============================================================================
// 清理与预留
// ============================================================================

// qpainterpath.cpp:307-310
void PkPainterPath::clear()
{
    m_elements.clear();
    m_currentPos = PkPointF(0, 0);
    markDirty();
}

// qpainterpath.cpp:2030-2032
void PkPainterPath::reserve(int size)
{
    m_elements.reserve(size);
}

// ============================================================================
// 子路径构建：moveTo / lineTo / cubicTo / quadTo / closeSubpath
// ============================================================================

// qpainterpath.cpp:312-317
void PkPainterPath::moveTo(const PkPointF &p)
{
    m_elements.append(Element(p.x(), p.y(), MoveToElement));
    m_currentPos = p;
    markDirty();
}

// qpainterpath.cpp:319-323
void PkPainterPath::lineTo(const PkPointF &p)
{
    m_elements.append(Element(p.x(), p.y(), LineToElement));
    m_currentPos = p;
    markDirty();
}

// qpainterpath.cpp:325-332
void PkPainterPath::cubicTo(const PkPointF &ctrlPt1, const PkPointF &ctrlPt2,
                             const PkPointF &endPt)
{
    m_elements.append(Element(ctrlPt1.x(), ctrlPt1.y(), CurveToElement));
    m_elements.append(Element(ctrlPt2.x(), ctrlPt2.y(), CurveToDataElement));
    m_elements.append(Element(endPt.x(), endPt.y(), CurveToDataElement));
    m_currentPos = endPt;
    markDirty();
}

// qpainterpath.cpp:334-339
void PkPainterPath::quadTo(const PkPointF &ctrlPt, const PkPointF &endPt)
{
    // 二次 Bezier → 三次 Bezier 的精确转换（Qt 的 QPainterPath 内部只用三次曲线）。
    // 给定控制点 (cx,cy) 与终点 (ex,ey)，起点 sp = currentPosition()：
    //   c1 = sp + 2/3*(cx-sp)
    //   c2 = ex + 2/3*(cx-ex)
    const PkPointF sp = m_currentPos;
    cubicTo(PkPointF(sp.x() + 2.0 / 3.0 * (ctrlPt.x() - sp.x()),
                     sp.y() + 2.0 / 3.0 * (ctrlPt.y() - sp.y())),
            PkPointF(endPt.x() + 2.0 / 3.0 * (ctrlPt.x() - endPt.x()),
                     endPt.y() + 2.0 / 3.0 * (ctrlPt.y() - endPt.y())),
            endPt);
    // cubicTo 已设置 m_currentPos 并 markDirty()，这里不用重复。
}

// qpainterpath.cpp:341-349
void PkPainterPath::closeSubpath()
{
    // 找到当前子路径的起点：从末尾向前找最后一个 MoveToElement。
    // 若找不到或当前位置已等于起点，不做任何事。
    for (int i = m_elements.size() - 1; i >= 0; --i) {
        if (m_elements.at(i).type == MoveToElement) {
            const PkPointF startPos(m_elements.at(i).x, m_elements.at(i).y);
            if (m_currentPos != startPos) {
                lineTo(startPos);
            }
            return;
        }
    }
}

// ============================================================================
// currentPosition
// ============================================================================

// qpainterpath.cpp:361-367
PkPointF PkPainterPath::currentPosition() const
{
    return m_currentPos;
}

// ============================================================================
// 形状附加：addRect / addPolygon / addPath
// ============================================================================

// qpainterpath.cpp:371-381
// 矩形：四条边，从左上角顺时针。
void PkPainterPath::addRect(const PkRectF &rect)
{
    moveTo(rect.x(), rect.y());
    lineTo(rect.x() + rect.width(), rect.y());
    lineTo(rect.x() + rect.width(), rect.y() + rect.height());
    lineTo(rect.x(), rect.y() + rect.height());
    closeSubpath();
}

// qpainterpath.cpp:499-506
void PkPainterPath::addPolygon(const PkPolygonF &polygon)
{
    if (polygon.isEmpty())
        return;

    moveTo(polygon.first());
    for (int i = 1; i < polygon.size(); ++i)
        lineTo(polygon.at(i));
    closeSubpath();
}

// qpainterpath.cpp:526-535
void PkPainterPath::addPath(const PkPainterPath &path)
{
    m_elements.reserve(m_elements.size() + path.m_elements.size());
    for (int i = 0; i < path.m_elements.size(); ++i) {
        m_elements.append(path.m_elements.at(i));
    }
    m_currentPos = path.m_currentPos;
    markDirty();
}

// ============================================================================
// 查询
// ============================================================================

// qpainterpath.cpp:356-359
bool PkPainterPath::isEmpty() const
{
    return m_elements.isEmpty();
}

// qpainterpath.cpp:361-367 的 computeBoundingRect 逻辑
// Qt 的 QPainterPath::boundingRect() 先检查脏标志，然后 computeBoundingRect()。
// 这里直接在 boundRect 中惰性计算。
PkRectF PkPainterPath::boundingRect() const
{
    if (!m_dirtyBounds)
        return m_cachedBounds;

    // computeBoundingRect: 遍历所有元素，取所有点（含控制点）的包围盒。
    if (m_elements.isEmpty()) {
        m_cachedBounds = PkRectF(0, 0, 0, 0);
        m_dirtyBounds = false;
        return m_cachedBounds;
    }

    qreal minx = m_elements.at(0).x;
    qreal maxx = minx;
    qreal miny = m_elements.at(0).y;
    qreal maxy = miny;

    for (int i = 1; i < m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);
        if (e.x < minx) minx = e.x;
        if (e.x > maxx) maxx = e.x;
        if (e.y < miny) miny = e.y;
        if (e.y > maxy) maxy = e.y;
    }

    m_cachedBounds = PkRectF(minx, miny, maxx - minx, maxy - miny);
    m_dirtyBounds = false;
    return m_cachedBounds;
}

// qpainterpath.cpp: 与 boundingRect 同形态的 controlPointRect。
PkRectF PkPainterPath::controlPointRect() const
{
    if (!m_dirtyControlRect)
        return m_cachedControlRect;

    // computeControlPointRect: 与 boundingRect 相同（boundingRect 取的
    // 就是所有元素点，包含控制点）。Qt 的 controlPointRect 专门取所有
    // 控制点的包围盒，与 boundingRect 一样——因为所有元素点都包含控制点。
    if (m_elements.isEmpty()) {
        m_cachedControlRect = PkRectF(0, 0, 0, 0);
        m_dirtyControlRect = false;
        return m_cachedControlRect;
    }

    qreal minx = m_elements.at(0).x;
    qreal maxx = minx;
    qreal miny = m_elements.at(0).y;
    qreal maxy = miny;

    for (int i = 1; i < m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);
        if (e.x < minx) minx = e.x;
        if (e.x > maxx) maxx = e.x;
        if (e.y < miny) miny = e.y;
        if (e.y > maxy) maxy = e.y;
    }

    m_cachedControlRect = PkRectF(minx, miny, maxx - minx, maxy - miny);
    m_dirtyControlRect = false;
    return m_cachedControlRect;
}

// qpainterpath.cpp: 判断路径是否闭合：检查最后一个元素是否与子路径起点重合。
bool PkPainterPath::isClosed() const
{
    if (m_elements.isEmpty())
        return false;

    // 找最后一个 MoveToElement 的位置
    PkPointF startPos(0, 0);
    PkPointF lastPos(0, 0);
    bool foundStart = false;
    bool foundLast = false;

    // 追踪最后一个元素的位置
    for (int i = 0; i < m_elements.size(); ++i) {
        const Element &e = m_elements.at(i);
        if (e.type == MoveToElement) {
            startPos = PkPointF(e.x, e.y);
            foundStart = true;
        }
        // 每个元素都更新位置（最后一个元素的位置就是路径末端）
        lastPos = PkPointF(e.x, e.y);
        foundLast = true;
    }

    if (!foundStart || !foundLast)
        return false;

    return pkQtFuzzyCompare(lastPos.x(), startPos.x())
        && pkQtFuzzyCompare(lastPos.y(), startPos.y());
}

// qpainterpath.cpp:2030-2032
void PkPainterPath::setElementPositionAt(int i, qreal x, qreal y)
{
    m_elements[i].x = x;
    m_elements[i].y = y;
    markDirty();
}

// ============================================================================
// 变换
// ============================================================================

// qpainterpath.cpp:1260-1265
void PkPainterPath::translate(qreal dx, qreal dy)
{
    if (pkQtFuzzyCompare(dx, 0.0) && pkQtFuzzyCompare(dy, 0.0))
        return;

    for (int i = 0; i < m_elements.size(); ++i) {
        m_elements[i].x += dx;
        m_elements[i].y += dy;
    }
    m_currentPos += PkPointF(dx, dy);
    markDirty();
}

PkPainterPath PkPainterPath::translated(qreal dx, qreal dy) const
{
    PkPainterPath copy(*this);
    copy.translate(dx, dy);
    return copy;
}

// ============================================================================
// 比较
// ============================================================================

// qpainterpath.cpp:2028-2038
bool PkPainterPath::operator==(const PkPainterPath &other) const
{
    if (m_elements.size() != other.m_elements.size())
        return false;
    if (m_fillRule != other.m_fillRule)
        return false;
    for (int i = 0; i < m_elements.size(); ++i) {
        if (m_elements.at(i) != other.m_elements.at(i))
            return false;
    }
    return true;
}

// ============================================================================
// 编译期断言
// ============================================================================

static_assert(std::is_standard_layout<PkPainterPath::Element>::value,
              "PkPainterPath::Element 必须是标准布局");