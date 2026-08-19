#include "PkPainterPath.h"

#include <cmath>
#include <cfloat>
#include <algorithm>
#include <math.h>

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
// T2 内部辅助函数 —— 弧→三次曲线转换（逐字抄自 Qt 5.15.7 qstroker.cpp 与
// qbezier_p.h）。这些函数不属于 PkPainterPath，是静态文件作用域。
// ============================================================================

// Qt 的 QT_PATH_KAPPA 常数（qstroker_p.h:113）：4*(sqrt(2)-1)/3
static constexpr qreal kPkPathKappa = 0.5522847498;

// `qDegreesToRadians` 的零 Qt 版本
static inline qreal pkDegToRad(qreal deg) { return deg * static_cast<qreal>(M_PI / 180.0); }

// ── 轻量 QBezier 辅助（只有 arc 转换需要的三个方法）─────────────────────────
//
// 不引入完整的 QBezier 类。只复刻 qbezier_p.h 的 coefficients() 内联 +
// qbezier.cpp 的 bezierOnInterval() + parameterSplitLeft() 内联。
struct PkArcBezier {
    qreal x1, y1, x2, y2, x3, y3, x4, y4;

    static PkArcBezier fromPoints(const PkPointF &p1, const PkPointF &p2,
                                   const PkPointF &p3, const PkPointF &p4)
    {
        return {p1.x(), p1.y(), p2.x(), p2.y(), p3.x(), p3.y(), p4.x(), p4.y()};
    }

    PkPointF pt2() const { return PkPointF(x2, y2); }
    PkPointF pt3() const { return PkPointF(x3, y3); }
    PkPointF pt4() const { return PkPointF(x4, y4); }

    PkArcBezier bezierOnInterval(qreal t0, qreal t1) const;

    // qbezier_p.h:243-265 的 parameterSplitLeft
    void parameterSplitLeft(qreal t, PkArcBezier *left);
};

// qbezier_p.h:154-163 的 coefficients
static inline void pkBezierCoefficients(qreal t, qreal &a, qreal &b, qreal &c, qreal &d)
{
    qreal m_t = 1. - t;
    b = m_t * m_t;
    c = t * t;
    d = c * t;
    a = b * m_t;
    b *= 3. * t;
    c *= 3. * m_t;
}

// qbezier.cpp:612-625 的 bezierOnInterval
PkArcBezier PkArcBezier::bezierOnInterval(qreal t0, qreal t1) const
{
    if (t0 == 0 && t1 == 1)
        return *this;

    PkArcBezier bezier = *this;

    PkArcBezier result;
    bezier.parameterSplitLeft(t0, &result);
    qreal trueT = (t1 - t0) / (1 - t0);
    bezier.parameterSplitLeft(trueT, &result);

    return result;
}

// qbezier_p.h:243-265 的 parameterSplitLeft
void PkArcBezier::parameterSplitLeft(qreal t, PkArcBezier *left)
{
    left->x1 = x1;
    left->y1 = y1;

    left->x2 = x1 + t * ( x2 - x1 );
    left->y2 = y1 + t * ( y2 - y1 );

    left->x3 = x2 + t * ( x3 - x2 );
    left->y3 = y2 + t * ( y3 - y2 );

    x3 = x3 + t * ( x4 - x3 );
    y3 = y3 + t * ( y4 - y3 );

    x2 = left->x3 + t * ( x3 - left->x3);
    y2 = left->y3 + t * ( y3 - left->y3);

    left->x3 = left->x2 + t * ( left->x3 - left->x2 );
    left->y3 = left->y2 + t * ( left->y3 - left->y2 );

    left->x4 = x1 = left->x3 + t * (x2 - left->x3);
    left->y4 = y1 = left->y3 + t * (y2 - left->y3);
}

// qstroker.cpp:792-850 的 qt_t_for_arc_angle
// 牛顿迭代法：给定角度，找到在 Bezier 曲线上对应 t 值。
static qreal pkTForArcAngle(qreal angle)
{
    if (pkQtFuzzyIsNull(angle))
        return 0;

    if (pkQtFuzzyCompare(angle, qreal(90)))
        return 1;

    qreal radians = pkDegToRad(angle);
    qreal cosAngle = std::cos(radians);
    qreal sinAngle = std::sin(radians);

    // initial guess
    qreal tc = angle / 90;
    // do some iterations of newton's method to approximate cosAngle
    tc -= ((((2-3*kPkPathKappa) * tc + 3*(kPkPathKappa-1)) * tc) * tc + 1 - cosAngle)
         / (((6-9*kPkPathKappa) * tc + 6*(kPkPathKappa-1)) * tc);
    tc -= ((((2-3*kPkPathKappa) * tc + 3*(kPkPathKappa-1)) * tc) * tc + 1 - cosAngle)
         / (((6-9*kPkPathKappa) * tc + 6*(kPkPathKappa-1)) * tc);

    // initial guess
    qreal ts = tc;
    ts -= ((((3*kPkPathKappa-2) * ts -  6*kPkPathKappa + 3) * ts + 3*kPkPathKappa) * ts - sinAngle)
         / (((9*kPkPathKappa-6) * ts + 12*kPkPathKappa - 6) * ts + 3*kPkPathKappa);
    ts -= ((((3*kPkPathKappa-2) * ts -  6*kPkPathKappa + 3) * ts + 3*kPkPathKappa) * ts - sinAngle)
         / (((9*kPkPathKappa-6) * ts + 12*kPkPathKappa - 6) * ts + 3*kPkPathKappa);

    // use the average of the t that best approximates cosAngle
    // and the t that best approximates sinAngle
    return 0.5 * (tc + ts);
}

// qstroker.cpp:852-950 的 qt_curves_for_arc 中的 qt_find_ellipse_coords 部分
// 与 qt_curves_for_arc 整体。
//
// 注意：qstroker.cpp 的 qt_find_ellipse_coords 声明在 qpainterpath.cpp 里也有
// 一份副本，我们这里合在一起实现。

// qpainterpath.cpp:113-160 的 qt_find_ellipse_coords
static void pkFindEllipseCoords(const PkRectF &r, qreal angle, qreal length,
                                 PkPointF *startPoint, PkPointF *endPoint)
{
    if (r.isNull()) {
        if (startPoint)
            *startPoint = PkPointF();
        if (endPoint)
            *endPoint = PkPointF();
        return;
    }

    qreal w2 = r.width() / 2;
    qreal h2 = r.height() / 2;

    qreal angles[2] = { angle, angle + length };
    PkPointF *points[2] = { startPoint, endPoint };

    for (int i = 0; i < 2; ++i) {
        if (!points[i])
            continue;

        qreal theta = angles[i] - 360 * std::floor(angles[i] / 360);
        qreal t = theta / 90;
        int quadrant = int(t);
        t -= quadrant;

        t = pkTForArcAngle(90 * t);

        // swap x and y?
        if (quadrant & 1)
            t = 1 - t;

        qreal a, b, c, d;
        pkBezierCoefficients(t, a, b, c, d);
        PkPointF p(a + b + c * kPkPathKappa, d + c + b * kPkPathKappa);

        // left quadrants
        if (quadrant == 1 || quadrant == 2)
            p.rx() = -p.x();

        // top quadrants
        if (quadrant == 0 || quadrant == 1)
            p.ry() = -p.y();

        *points[i] = r.center() + PkPointF(w2 * p.x(), h2 * p.y());
    }
}

// qstroker.cpp:852-950 的 qt_curves_for_arc
static PkPointF pkCurvesForArc(const PkRectF &rect, qreal startAngle, qreal sweepLength,
                                PkPointF *curves, int *point_count)
{
    // 假设 curves 有至少 15 个元素的容量
    *point_count = 0;

    if (std::isnan(rect.x()) || std::isnan(rect.y()) ||
        std::isnan(rect.width()) || std::isnan(rect.height()) ||
        std::isnan(startAngle) || std::isnan(sweepLength)) {
        return PkPointF();
    }

    if (rect.isNull()) {
        return PkPointF();
    }

    qreal x = rect.x();
    qreal y = rect.y();

    qreal w = rect.width();
    qreal w2 = rect.width() / 2;
    qreal w2k = w2 * kPkPathKappa;

    qreal h = rect.height();
    qreal h2 = rect.height() / 2;
    qreal h2k = h2 * kPkPathKappa;

    // 16 个预设点，用 0-15 索引
    // 注意：Qt 用 16 个 slot (0-15)，points[0..15]
    PkPointF points[16];

    // start point 在 points[0] (or points[12] depending on usage)
    // Qt 原文用 points[0] 到 points[15] 的 C 数组，第一次赋值在索引 0
    // 但 Qt 原文的数组初始化是：
    //   QPointF points[16] = { QPointF(x + w, y + h2), ... };
    // 所以 points[0] = 起点
    points[0]  = PkPointF(x + w, y + h2);

    // 0 -> 270 degrees
    points[1]  = PkPointF(x + w, y + h2 + h2k);
    points[2]  = PkPointF(x + w2 + w2k, y + h);
    points[3]  = PkPointF(x + w2, y + h);

    // 270 -> 180 degrees
    points[4]  = PkPointF(x + w2 - w2k, y + h);
    points[5]  = PkPointF(x, y + h2 + h2k);
    points[6]  = PkPointF(x, y + h2);

    // 180 -> 90 degrees
    points[7]  = PkPointF(x, y + h2 - h2k);
    points[8]  = PkPointF(x + w2 - w2k, y);
    points[9]  = PkPointF(x + w2, y);

    // 90 -> 0 degrees
    points[10] = PkPointF(x + w2 + w2k, y);
    points[11] = PkPointF(x + w, y + h2 - h2k);
    points[12] = PkPointF(x + w, y + h2);

    if (sweepLength > 360) sweepLength = 360;
    else if (sweepLength < -360) sweepLength = -360;

    // Special case fast paths
    if (startAngle == 0.0) {
        if (sweepLength == 360.0) {
            for (int i = 11; i >= 0; --i)
                curves[(*point_count)++] = points[i];
            return points[12];
        } else if (sweepLength == -360.0) {
            for (int i = 1; i <= 12; ++i)
                curves[(*point_count)++] = points[i];
            return points[0];
        }
    }

    int startSegment = int(std::floor(startAngle / 90));
    int endSegment = int(std::floor((startAngle + sweepLength) / 90));

    qreal startT = (startAngle - startSegment * 90) / 90;
    qreal endT = (startAngle + sweepLength - endSegment * 90) / 90;

    int delta = sweepLength > 0 ? 1 : -1;
    if (delta < 0) {
        startT = 1 - startT;
        endT = 1 - endT;
    }

    // avoid empty start segment
    if (pkQtFuzzyIsNull(startT - qreal(1))) {
        startT = 0;
        startSegment += delta;
    }

    // avoid empty end segment
    if (pkQtFuzzyIsNull(endT)) {
        endT = 1;
        endSegment -= delta;
    }

    startT = pkTForArcAngle(startT * 90);
    endT = pkTForArcAngle(endT * 90);

    const bool splitAtStart = !pkQtFuzzyIsNull(startT);
    const bool splitAtEnd = !pkQtFuzzyIsNull(endT - qreal(1));

    const int end = endSegment + delta;

    // empty arc?
    if (startSegment == end) {
        const int quadrant = 3 - ((startSegment % 4) + 4) % 4;
        const int j = 3 * quadrant;
        return delta > 0 ? points[j + 3] : points[j];
    }

    PkPointF startPoint, endPoint;
    pkFindEllipseCoords(rect, startAngle, sweepLength, &startPoint, &endPoint);

    for (int i = startSegment; i != end; i += delta) {
        const int quadrant = 3 - ((i % 4) + 4) % 4;
        const int j = 3 * quadrant;

        PkArcBezier b;
        if (delta > 0)
            b = PkArcBezier::fromPoints(points[j + 3], points[j + 2], points[j + 1], points[j]);
        else
            b = PkArcBezier::fromPoints(points[j], points[j + 1], points[j + 2], points[j + 3]);

        // empty arc?
        if (startSegment == endSegment && pkQtFuzzyCompare(startT, endT))
            return startPoint;

        if (i == startSegment) {
            if (i == endSegment && splitAtEnd)
                b = b.bezierOnInterval(startT, endT);
            else if (splitAtStart)
                b = b.bezierOnInterval(startT, 1);
        } else if (i == endSegment && splitAtEnd) {
            b = b.bezierOnInterval(0, endT);
        }

        // push control points
        curves[(*point_count)++] = b.pt2();
        curves[(*point_count)++] = b.pt3();
        curves[(*point_count)++] = b.pt4();
    }

    curves[*(point_count)-1] = endPoint;

    return startPoint;
}

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
// T2 形状辅助：addEllipse / arcTo / addRoundedRect
// 逐字抄自 Qt 5.15.7 qpainterpath.cpp（行号标在各项上方）。
// ============================================================================

// qpainterpath.cpp:1166-1195
void PkPainterPath::addEllipse(const PkRectF &boundingRect)
{
    if (boundingRect.isNull())
        return;

    PkPointF pts[12];
    int point_count;
    PkPointF start = pkCurvesForArc(boundingRect, 0, -360, pts, &point_count);

    moveTo(start);
    cubicTo(pts[0], pts[1], pts[2]);           // 0 -> 270
    cubicTo(pts[3], pts[4], pts[5]);           // 270 -> 180
    cubicTo(pts[6], pts[7], pts[8]);           // 180 -> 90
    cubicTo(pts[9], pts[10], pts[11]);         // 90 -> 0
}

// qpainterpath.cpp:976-1007
void PkPainterPath::arcTo(const PkRectF &rect, qreal startAngle, qreal sweepLength)
{
    if (rect.isNull())
        return;

    int point_count;
    PkPointF pts[15];
    PkPointF curve_start = pkCurvesForArc(rect, startAngle, sweepLength, pts, &point_count);

    lineTo(curve_start);
    for (int i = 0; i < point_count; i += 3) {
        cubicTo(pts[i].x(), pts[i].y(),
                pts[i + 1].x(), pts[i + 1].y(),
                pts[i + 2].x(), pts[i + 2].y());
    }
}

// qpainterpath.cpp:3216-3272
void PkPainterPath::addRoundedRect(const PkRectF &rect, qreal xRadius, qreal yRadius,
                                    Qt::SizeMode mode)
{
    PkRectF r = rect.normalized();

    if (r.isNull())
        return;

    if (mode == Qt::AbsoluteSize) {
        qreal w = r.width() / 2;
        qreal h = r.height() / 2;

        if (w == 0) {
            xRadius = 0;
        } else {
            xRadius = 100 * qMin(xRadius, w) / w;
        }
        if (h == 0) {
            yRadius = 0;
        } else {
            yRadius = 100 * qMin(yRadius, h) / h;
        }
    } else {
        if (xRadius > 100)
            xRadius = 100;

        if (yRadius > 100)
            yRadius = 100;
    }

    if (xRadius <= 0 || yRadius <= 0) {             // add normal rectangle
        addRect(r);
        return;
    }

    qreal x = r.x();
    qreal y = r.y();
    qreal w = r.width();
    qreal h = r.height();
    qreal rxx2 = w * xRadius / 100;
    qreal ryy2 = h * yRadius / 100;

    arcMoveTo(PkRectF(x, y, rxx2, ryy2), 180);
    arcTo(x, y, rxx2, ryy2, 180, -90);
    arcTo(x + w - rxx2, y, rxx2, ryy2, 90, -90);
    arcTo(x + w - rxx2, y + h - ryy2, rxx2, ryy2, 0, -90);
    arcTo(x, y + h - ryy2, rxx2, ryy2, 270, -90);
    closeSubpath();
}

// qpainterpath.cpp:1033-1041
void PkPainterPath::arcMoveTo(const PkRectF &rect, qreal angle)
{
    if (rect.isNull())
        return;

    PkPointF pt;
    pkFindEllipseCoords(rect, angle, 0, &pt, nullptr);
    moveTo(pt);
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