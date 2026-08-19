#include "PkPainterPath.h"
#include "PkLine.h"
#include "PkTransform.h"

#include <cmath>
#include <cfloat>
#include <algorithm>
#include <math.h>
#include <cassert>
#include <type_traits>

// ---------------------------------------------------------------------------
// PkPainterPath 实现 —— 逐字抄自真 Qt 5.15.7 的 qpainterpath.cpp。
// 源码来自上游 tag `v5.15.7-lts-lgpl`。
// ---------------------------------------------------------------------------

// ============================================================================
// T2 内部辅助函数 —— 弧→三次曲线转换
// ============================================================================

static constexpr qreal kPkPathKappa = 0.5522847498;

static inline qreal pkDegToRad(qreal deg) { return deg * static_cast<qreal>(M_PI / 180.0); }

struct PkArcBezier {
    qreal x1 = 0, y1 = 0, x2 = 0, y2 = 0, x3 = 0, y3 = 0, x4 = 0, y4 = 0;

    static PkArcBezier fromPoints(const PkPointF &p1, const PkPointF &p2,
                                   const PkPointF &p3, const PkPointF &p4)
    { return {p1.x(), p1.y(), p2.x(), p2.y(), p3.x(), p3.y(), p4.x(), p4.y()}; }

    PkPointF pt1() const { return PkPointF(x1, y1); }
    PkPointF pt2() const { return PkPointF(x2, y2); }
    PkPointF pt3() const { return PkPointF(x3, y3); }
    PkPointF pt4() const { return PkPointF(x4, y4); }

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

void PkArcBezier::parameterSplitLeft(qreal t, PkArcBezier *left)
{
    left->x1 = x1; left->y1 = y1;
    left->x2 = x1 + t * (x2 - x1); left->y2 = y1 + t * (y2 - y1);
    left->x3 = x2 + t * (x3 - x2); left->y3 = y2 + t * (y3 - y2);
    x3 = x3 + t * (x4 - x3); y3 = y3 + t * (y4 - y3);
    x2 = left->x3 + t * (x3 - left->x3); y2 = left->y3 + t * (y3 - left->y3);
    left->x3 = left->x2 + t * (left->x3 - left->x2); left->y3 = left->y2 + t * (left->y3 - left->y2);
    left->x4 = x1 = left->x3 + t * (x2 - left->x3); left->y4 = y1 = left->y3 + t * (y2 - left->y3);
}

PkArcBezier PkArcBezier::bezierOnInterval(qreal t0, qreal t1) const
{
    if (t0 == 0 && t1 == 1) return *this;
    PkArcBezier bezier = *this, result;
    bezier.parameterSplitLeft(t0, &result);
    qreal trueT = (t1 - t0) / (1 - t0);
    bezier.parameterSplitLeft(trueT, &result);
    return result;
}

static PkArcBezierSplit pkSplitBezier(const PkArcBezier &b)
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

static PkRectF pkBezierBounds(const PkArcBezier &b)
{
    qreal xmin = b.x1, xmax = b.x1, ymin = b.y1, ymax = b.y1;
    auto upd = [&](qreal x, qreal y) { if (x < xmin) xmin = x; if (x > xmax) xmax = x; if (y < ymin) ymin = y; if (y > ymax) ymax = y; };
    upd(b.x2, b.y2); upd(b.x3, b.y3); upd(b.x4, b.y4);
    return PkRectF(xmin, ymin, xmax - xmin, ymax - ymin);
}

// ============================================================================
// 弧辅助函数
// ============================================================================

static qreal pkTForArcAngle(qreal angle)
{
    if (pkQtFuzzyIsNull(angle)) return 0;
    if (pkQtFuzzyCompare(angle, qreal(90))) return 1;
    qreal cosAngle = std::cos(pkDegToRad(angle));
    qreal sinAngle = std::sin(pkDegToRad(angle));
    qreal tc = angle / 90;
    tc -= ((((2-3*kPkPathKappa)*tc + 3*(kPkPathKappa-1))*tc)*tc + 1 - cosAngle)
         / (((6-9*kPkPathKappa)*tc + 6*(kPkPathKappa-1))*tc);
    tc -= ((((2-3*kPkPathKappa)*tc + 3*(kPkPathKappa-1))*tc)*tc + 1 - cosAngle)
         / (((6-9*kPkPathKappa)*tc + 6*(kPkPathKappa-1))*tc);
    qreal ts = tc;
    ts -= ((((3*kPkPathKappa-2)*ts - 6*kPkPathKappa + 3)*ts + 3*kPkPathKappa)*ts - sinAngle)
         / (((9*kPkPathKappa-6)*ts + 12*kPkPathKappa - 6)*ts + 3*kPkPathKappa);
    ts -= ((((3*kPkPathKappa-2)*ts - 6*kPkPathKappa + 3)*ts + 3*kPkPathKappa)*ts - sinAngle)
         / (((9*kPkPathKappa-6)*ts + 12*kPkPathKappa - 6)*ts + 3*kPkPathKappa);
    return 0.5 * (tc + ts);
}

static void pkFindEllipseCoords(const PkRectF &r, qreal angle, qreal length,
                                 PkPointF *startPoint, PkPointF *endPoint)
{
    if (r.isNull()) {
        if (startPoint) *startPoint = PkPointF();
        if (endPoint) *endPoint = PkPointF();
        return;
    }
    qreal w2 = r.width() / 2, h2 = r.height() / 2;
    qreal angles[2] = {angle, angle + length};
    PkPointF *points[2] = {startPoint, endPoint};
    for (int i = 0; i < 2; ++i) {
        if (!points[i]) continue;
        qreal theta = angles[i] - 360 * std::floor(angles[i] / 360);
        qreal t = theta / 90;
        int quadrant = int(t); t -= quadrant;
        t = pkTForArcAngle(90 * t);
        if (quadrant & 1) t = 1 - t;
        qreal a, b, c, d; pkBezierCoefficients(t, a, b, c, d);
        PkPointF p(a + b + c * kPkPathKappa, d + c + b * kPkPathKappa);
        if (quadrant == 1 || quadrant == 2) p.rx() = -p.x();
        if (quadrant == 0 || quadrant == 1) p.ry() = -p.y();
        *points[i] = r.center() + PkPointF(w2 * p.x(), h2 * p.y());
    }
}

static PkPointF pkCurvesForArc(const PkRectF &rect, qreal startAngle, qreal sweepLength,
                                PkPointF *curves, int *point_count)
{
    *point_count = 0;
    if (std::isnan(rect.x()) || std::isnan(rect.y()) || std::isnan(rect.width()) || std::isnan(rect.height())
        || std::isnan(startAngle) || std::isnan(sweepLength)) return PkPointF();
    if (rect.isNull()) return PkPointF();
    qreal x = rect.x(), y = rect.y(), w = rect.width(), h = rect.height();
    qreal w2 = w/2, w2k = w2 * kPkPathKappa, h2 = h/2, h2k = h2 * kPkPathKappa;
    PkPointF pts[16];
    pts[0] = PkPointF(x+w, y+h2); pts[1] = PkPointF(x+w, y+h2+h2k);
    pts[2] = PkPointF(x+w2+w2k, y+h); pts[3] = PkPointF(x+w2, y+h);
    pts[4] = PkPointF(x+w2-w2k, y+h); pts[5] = PkPointF(x, y+h2+h2k);
    pts[6] = PkPointF(x, y+h2); pts[7] = PkPointF(x, y+h2-h2k);
    pts[8] = PkPointF(x+w2-w2k, y); pts[9] = PkPointF(x+w2, y);
    pts[10] = PkPointF(x+w2+w2k, y); pts[11] = PkPointF(x+w, y+h2-h2k);
    pts[12] = PkPointF(x+w, y+h2);
    if (sweepLength > 360) sweepLength = 360; else if (sweepLength < -360) sweepLength = -360;
    if (startAngle == 0.0) {
        if (sweepLength == 360.0) { for (int i = 11; i >= 0; --i) curves[(*point_count)++] = pts[i]; return pts[12]; }
        if (sweepLength == -360.0) { for (int i = 1; i <= 12; ++i) curves[(*point_count)++] = pts[i]; return pts[0]; }
    }
    int startSegment = int(std::floor(startAngle / 90));
    int endSegment = int(std::floor((startAngle + sweepLength) / 90));
    qreal startT = (startAngle - startSegment * 90) / 90;
    qreal endT = (startAngle + sweepLength - endSegment * 90) / 90;
    int delta = sweepLength > 0 ? 1 : -1;
    if (delta < 0) { startT = 1 - startT; endT = 1 - endT; }
    if (pkQtFuzzyIsNull(startT - 1)) { startT = 0; startSegment += delta; }
    if (pkQtFuzzyIsNull(endT)) { endT = 1; endSegment -= delta; }
    startT = pkTForArcAngle(startT * 90); endT = pkTForArcAngle(endT * 90);
    bool splitAtStart = !pkQtFuzzyIsNull(startT), splitAtEnd = !pkQtFuzzyIsNull(endT - 1);
    int end = endSegment + delta;
    if (startSegment == end) { int q = 3 - ((startSegment % 4) + 4) % 4; int j = 3 * q; return delta > 0 ? pts[j + 3] : pts[j]; }
    PkPointF startPoint, endPoint;
    pkFindEllipseCoords(rect, startAngle, sweepLength, &startPoint, &endPoint);
    for (int i = startSegment; i != end; i += delta) {
        int q = 3 - ((i % 4) + 4) % 4, j = 3 * q;
        PkArcBezier b = (delta > 0) ? PkArcBezier::fromPoints(pts[j+3], pts[j+2], pts[j+1], pts[j])
                                     : PkArcBezier::fromPoints(pts[j], pts[j+1], pts[j+2], pts[j+3]);
        if (startSegment == endSegment && pkQtFuzzyCompare(startT, endT)) return startPoint;
        if (i == startSegment) {
            if (i == endSegment && splitAtEnd) b = b.bezierOnInterval(startT, endT);
            else if (splitAtStart) b = b.bezierOnInterval(startT, 1);
        } else if (i == endSegment && splitAtEnd) { b = b.bezierOnInterval(0, endT); }
        curves[(*point_count)++] = b.pt2(); curves[(*point_count)++] = b.pt3(); curves[(*point_count)++] = b.pt4();
    }
    curves[*(point_count)-1] = endPoint;
    return startPoint;
}

// ============================================================================
// T3/T4 内部辅助函数（前向声明放这里）
// ============================================================================

static void pkPainterPathIsectLine(const PkPointF &p1, const PkPointF &p2, const PkPointF &pos, int *winding);
static void pkPainterPathIsectCurve(const PkArcBezier &bezier, const PkPointF &pt, int *winding, int depth = 0);
static bool pkPainterPathIsectLineRect(qreal x1, qreal y1, qreal x2, qreal y2, const PkRectF &rect);
static bool pkPointOnEdge(const PkRectF &r, const PkPointF &pt);
static bool pkCheckCrossing(const PkPainterPath *path, const PkRectF &rect);
static bool pkIsectCurveHorizontal(const PkArcBezier &bezier, qreal y, qreal x1, qreal x2, int depth = 0);
static bool pkIsectCurveVertical(const PkArcBezier &bezier, qreal x, qreal y1, qreal y2, int depth = 0);

static void pkPainterPathIsectLine(const PkPointF &p1, const PkPointF &p2, const PkPointF &pos, int *winding)
{
    qreal x1=p1.x(), y1=p1.y(), x2=p2.x(), y2=p2.y(), y=pos.y(); int dir=1;
    if (pkQtFuzzyCompare(y1, y2)) return;
    if (y2 < y1) { std::swap(x1, x2); std::swap(y1, y2); dir = -1; }
    if (y >= y1 && y < y2) { qreal x = x1 + ((x2-x1)/(y2-y1))*(y-y1); if (x <= pos.x()) (*winding) += dir; }
}

static void pkPainterPathIsectCurve(const PkArcBezier &bezier, const PkPointF &pt, int *winding, int depth)
{
    qreal y=pt.y(), x=pt.x(); PkRectF bounds = pkBezierBounds(bezier);
    if (y >= bounds.y() && y < bounds.y() + bounds.height()) {
        const qreal lower_bound = qreal(.001);
        if (depth == 32 || (bounds.width() < lower_bound && bounds.height() < lower_bound)) {
            if (bezier.pt1().x() <= x) (*winding) += (bezier.pt4().y() > bezier.pt1().y() ? 1 : -1);
            return;
        }
        auto halves = pkSplitBezier(bezier);
        pkPainterPathIsectCurve(halves.first, pt, winding, depth+1);
        pkPainterPathIsectCurve(halves.second, pt, winding, depth+1);
    }
}

enum { kPkLeftDir = 0, kPkRightDir = 1, kPkTopDir = 2, kPkBottomDir = 3 };

static bool pkPainterPathIsectLineRect(qreal x1, qreal y1, qreal x2, qreal y2, const PkRectF &rect)
{
    qreal left=rect.left(), right=rect.right(), top=rect.top(), bottom=rect.bottom();
    auto oc = [&](qreal x, qreal y) { return ((x<left)<<kPkLeftDir)|((x>right)<<kPkRightDir)|((y<top)<<kPkTopDir)|((y>bottom)<<kPkBottomDir); };
    int p1=oc(x1,y1), p2=oc(x2,y2); if (p1&p2) return false;
    if (p1|p2) {
        qreal dx=x2-x1, dy=y2-y1;
        auto clipX = [&](qreal &x, qreal &y) { if (x<left) { y+=dy/dx*(left-x); x=left; } else if (x>right) { y-=dy/dx*(x-right); x=right; } };
        auto clipY = [&](qreal &x, qreal &y) { if (y<top) { x+=dx/dy*(top-y); y=top; } else if (y>bottom) { x-=dx/dy*(y-bottom); y=bottom; } };
        clipX(x1,y1); clipX(x2,y2);
        p1=oc(x1,y1); p2=oc(x2,y2); if (p1&p2) return false;
        clipY(x1,y1); clipY(x2,y2);
        p1=oc(x1,y1); p2=oc(x2,y2); if (p1&p2) return false;
        return true;
    }
    return false;
}

static bool pkPointOnEdge(const PkRectF &r, const PkPointF &pt)
{
    return ((pkQtFuzzyCompare(pt.x(), r.left()) || pkQtFuzzyCompare(pt.x(), r.right())) && pt.y() >= r.top() && pt.y() <= r.bottom())
        || ((pkQtFuzzyCompare(pt.y(), r.top()) || pkQtFuzzyCompare(pt.y(), r.bottom())) && pt.x() >= r.left() && pt.x() <= r.right());
}

static bool pkIsectCurveHorizontal(const PkArcBezier &bezier, qreal y, qreal x1, qreal x2, int depth)
{
    PkRectF bounds = pkBezierBounds(bezier);
    if (y >= bounds.top() && y < bounds.bottom() && bounds.right() >= x1 && bounds.left() < x2) {
        const qreal lb = qreal(.01);
        if (depth == 32 || (bounds.width() < lb && bounds.height() < lb)) return true;
        auto halves = pkSplitBezier(bezier);
        return pkIsectCurveHorizontal(halves.first, y, x1, x2, depth+1) || pkIsectCurveHorizontal(halves.second, y, x1, x2, depth+1);
    }
    return false;
}

static bool pkIsectCurveVertical(const PkArcBezier &bezier, qreal x, qreal y1, qreal y2, int depth)
{
    PkRectF bounds = pkBezierBounds(bezier);
    if (x >= bounds.left() && x < bounds.right() && bounds.bottom() >= y1 && bounds.top() < y2) {
        const qreal lb = qreal(.01);
        if (depth == 32 || (bounds.width() < lb && bounds.height() < lb)) return true;
        auto halves = pkSplitBezier(bezier);
        return pkIsectCurveVertical(halves.first, x, y1, y2, depth+1) || pkIsectCurveVertical(halves.second, x, y1, y2, depth+1);
    }
    return false;
}

static bool pkCheckCrossing(const PkPainterPath *path, const PkRectF &rect)
{
    PkPointF last_pt, last_start;
    enum { kOnRect, kInsideRect, kOutsideRect } edgeStatus = kOnRect;
    for (int i = 0; i < path->elementCount(); ++i) {
        const PkPainterPath::Element &e = path->elementAt(i);
        switch (e.type) {
        case PkPainterPath::MoveToElement:
            if (i>0 && pkQtFuzzyCompare(last_pt.x(), last_start.x()) && pkQtFuzzyCompare(last_pt.y(), last_start.y())
                && pkPainterPathIsectLineRect(last_pt.x(), last_pt.y(), last_start.x(), last_start.y(), rect))
                return true;
            last_start = last_pt = PkPointF(e.x, e.y); break;
        case PkPainterPath::LineToElement:
            if (pkPainterPathIsectLineRect(last_pt.x(), last_pt.y(), e.x, e.y, rect)) return true;
            last_pt = PkPointF(e.x, e.y); break;
        case PkPainterPath::CurveToElement: {
            PkPointF cp2 = path->elementAt(++i), ep = path->elementAt(++i);
            PkArcBezier bezier = PkArcBezier::fromPoints(last_pt, PkPointF(e.x,e.y), cp2, ep);
            if (pkIsectCurveHorizontal(bezier, rect.top(), rect.left(), rect.right())
                || pkIsectCurveHorizontal(bezier, rect.bottom(), rect.left(), rect.right())
                || pkIsectCurveVertical(bezier, rect.left(), rect.top(), rect.bottom())
                || pkIsectCurveVertical(bezier, rect.right(), rect.top(), rect.bottom()))
                return true;
            last_pt = ep; break;
        }
        default: break;
        }
        if (!pkPointOnEdge(rect, last_pt)) {
            bool contained = rect.contains(last_pt);
            switch (edgeStatus) {
            case kOutsideRect: if (contained) return true; break;
            case kInsideRect: if (!contained) return true; break;
            case kOnRect: edgeStatus = contained ? kInsideRect : kOutsideRect; break;
            }
        } else { if (last_pt == last_start) edgeStatus = kOnRect; }
    }
    if (last_pt != last_start && pkPainterPathIsectLineRect(last_pt.x(), last_pt.y(), last_start.x(), last_start.y(), rect))
        return true;
    return false;
}

// 曲线细分摊平
static void pkAddBezierToPolygon(const PkArcBezier &bezier, PkPolygonF *polygon, qreal threshold = 0.5)
{
    PkArcBezier beziers[10]; int levels[10];
    beziers[0] = bezier; levels[0] = 9; int top = 0;
    while (top >= 0) {
        PkArcBezier *b = &beziers[top];
        qreal y4y1 = b->y4 - b->y1, x4x1 = b->x4 - b->x1, l = qAbs(x4x1) + qAbs(y4y1);
        qreal d;
        if (l > 1.) d = qAbs((x4x1)*(b->y1 - b->y2) - (y4y1)*(b->x1 - b->x2)) + qAbs((x4x1)*(b->y1 - b->y3) - (y4y1)*(b->x1 - b->x3));
        else { d = qAbs(b->x1-b->x2)+qAbs(b->y1-b->y2)+qAbs(b->x1-b->x3)+qAbs(b->y1-b->y3); l = 1.; }
        if (d < threshold * l || levels[top] == 0) { polygon->append(PkPointF(b->x4, b->y4)); --top; }
        else {
            auto halves = pkSplitBezier(*b);
            beziers[top] = halves.second; beziers[top + 1] = halves.first;
            levels[top + 1] = --levels[top]; ++top;
        }
    }
}

// 三次 Bezier 曲线长度（近似积分）
static qreal pkBezierLength(const PkArcBezier &b, qreal error = 0.01)
{
    qreal dx = b.x4 - b.x1, dy = b.y4 - b.y1, chord = std::sqrt(dx*dx + dy*dy);
    qreal cx = (b.x2 + b.x3) * 0.5, cy = (b.y2 + b.y3) * 0.5;
    qreal mx = (b.x1 + b.x4 + 2*cx) * 0.25, my = (b.y1 + b.y4 + 2*cy) * 0.25;
    qreal flat = std::sqrt((mx - (b.x1 + b.x4)*0.5)*(mx - (b.x1 + b.x4)*0.5) + (my - (b.y1 + b.y4)*0.5)*(my - (b.y1 + b.y4)*0.5));
    if (flat < error || chord < 1e-6) return chord;
    auto halves = pkSplitBezier(b);
    return pkBezierLength(halves.first, error) + pkBezierLength(halves.second, error);
}

// ============================================================================
// PkPainterPath 成员实现
// ============================================================================

PkPainterPath::PkPainterPath() noexcept : m_fillRule(Qt::OddEvenFill) {}
PkPainterPath::PkPainterPath(const PkPointF &startPoint) : m_fillRule(Qt::OddEvenFill) { moveTo(startPoint); }

void PkPainterPath::swap(PkPainterPath &other) noexcept
{
    m_elements.swap(other.m_elements); std::swap(m_currentPos, other.m_currentPos);
    std::swap(m_cachedBounds, other.m_cachedBounds); std::swap(m_cachedControlRect, other.m_cachedControlRect);
    std::swap(m_dirtyBounds, other.m_dirtyBounds); std::swap(m_dirtyControlRect, other.m_dirtyControlRect);
    std::swap(m_fillRule, other.m_fillRule);
}

void PkPainterPath::markDirty() { m_dirtyBounds = true; m_dirtyControlRect = true; }
void PkPainterPath::clear() { m_elements.clear(); m_currentPos = PkPointF(0,0); markDirty(); }
void PkPainterPath::reserve(int size) { m_elements.reserve(size); }

void PkPainterPath::moveTo(const PkPointF &p) { m_elements.append(Element(p.x(),p.y(),MoveToElement)); m_currentPos = p; markDirty(); }
void PkPainterPath::lineTo(const PkPointF &p) { m_elements.append(Element(p.x(),p.y(),LineToElement)); m_currentPos = p; markDirty(); }
void PkPainterPath::cubicTo(const PkPointF &c1, const PkPointF &c2, const PkPointF &ep)
{ m_elements.append(Element(c1.x(),c1.y(),CurveToElement)); m_elements.append(Element(c2.x(),c2.y(),CurveToDataElement)); m_elements.append(Element(ep.x(),ep.y(),CurveToDataElement)); m_currentPos = ep; markDirty(); }
void PkPainterPath::quadTo(const PkPointF &cp, const PkPointF &ep)
{ const PkPointF sp=m_currentPos; cubicTo(PkPointF(sp.x()+2./3.*(cp.x()-sp.x()),sp.y()+2./3.*(cp.y()-sp.y())),PkPointF(ep.x()+2./3.*(cp.x()-ep.x()),ep.y()+2./3.*(cp.y()-ep.y())),ep); }
void PkPainterPath::closeSubpath()
{ for (int i=m_elements.size()-1;i>=0;--i) { const auto &e=m_elements.at(i); if (e.type==MoveToElement) { const PkPointF sp(e.x,e.y); if (m_currentPos!=sp) lineTo(sp); return; } } }
PkPointF PkPainterPath::currentPosition() const { return m_currentPos; }

void PkPainterPath::addRect(const PkRectF &rect)
{ moveTo(rect.x(),rect.y()); lineTo(rect.x()+rect.width(),rect.y()); lineTo(rect.x()+rect.width(),rect.y()+rect.height()); lineTo(rect.x(),rect.y()+rect.height()); closeSubpath(); }
void PkPainterPath::addPolygon(const PkPolygonF &polygon)
{ if (polygon.isEmpty()) return; moveTo(polygon.first()); for (int i=1;i<polygon.size();++i) lineTo(polygon.at(i)); closeSubpath(); }
void PkPainterPath::addPath(const PkPainterPath &path)
{ m_elements.reserve(m_elements.size()+path.m_elements.size()); for (int i=0;i<path.m_elements.size();++i) m_elements.append(path.m_elements.at(i)); m_currentPos=path.m_currentPos; markDirty(); }

void PkPainterPath::addEllipse(const PkRectF &r)
{ if (r.isNull()) return; PkPointF pts[12]; int pc; PkPointF s=pkCurvesForArc(r,0,-360,pts,&pc); moveTo(s); cubicTo(pts[0],pts[1],pts[2]); cubicTo(pts[3],pts[4],pts[5]); cubicTo(pts[6],pts[7],pts[8]); cubicTo(pts[9],pts[10],pts[11]); }
void PkPainterPath::arcTo(const PkRectF &rect, qreal sa, qreal sl)
{ if (rect.isNull()) return; int pc; PkPointF pts[15]; PkPointF cs=pkCurvesForArc(rect,sa,sl,pts,&pc); lineTo(cs); for (int i=0;i<pc;i+=3) cubicTo(pts[i],pts[i+1],pts[i+2]); }
void PkPainterPath::addRoundedRect(const PkRectF &rect, qreal xr, qreal yr, Qt::SizeMode mode)
{ PkRectF r=rect.normalized(); if (r.isNull()) return;
  if (mode==Qt::AbsoluteSize) { qreal w=r.width()/2,h=r.height()/2; xr=w?100*qMin(xr,w)/w:0; yr=h?100*qMin(yr,h)/h:0; }
  else { if (xr>100) xr=100; if (yr>100) yr=100; }
  if (xr<=0||yr<=0) { addRect(r); return; }
  qreal x=r.x(),y=r.y(),w=r.width(),h=r.height(),rxx2=w*xr/100,ryy2=h*yr/100;
  arcMoveTo(PkRectF(x,y,rxx2,ryy2),180); arcTo(x,y,rxx2,ryy2,180,-90); arcTo(x+w-rxx2,y,rxx2,ryy2,90,-90);
  arcTo(x+w-rxx2,y+h-ryy2,rxx2,ryy2,0,-90); arcTo(x,y+h-ryy2,rxx2,ryy2,270,-90); closeSubpath(); }
void PkPainterPath::arcMoveTo(const PkRectF &rect, qreal angle)
{ if (rect.isNull()) return; PkPointF pt; pkFindEllipseCoords(rect,angle,0,&pt,nullptr); moveTo(pt); }

// 查询
bool PkPainterPath::isEmpty() const { return m_elements.isEmpty(); }
PkRectF PkPainterPath::boundingRect() const
{ if (!m_dirtyBounds) return m_cachedBounds; if (m_elements.isEmpty()) { m_cachedBounds=PkRectF(0,0,0,0); m_dirtyBounds=false; return m_cachedBounds; }
  qreal minx=m_elements.at(0).x,maxx=minx,miny=m_elements.at(0).y,maxy=miny;
  for (int i=1;i<m_elements.size();++i) { const auto &e=m_elements.at(i); if (e.x<minx) minx=e.x; if (e.x>maxx) maxx=e.x; if (e.y<miny) miny=e.y; if (e.y>maxy) maxy=e.y; }
  m_cachedBounds=PkRectF(minx,miny,maxx-minx,maxy-miny); m_dirtyBounds=false; return m_cachedBounds; }
PkRectF PkPainterPath::controlPointRect() const
{ if (!m_dirtyControlRect) return m_cachedControlRect; if (m_elements.isEmpty()) { m_cachedControlRect=PkRectF(0,0,0,0); m_dirtyControlRect=false; return m_cachedControlRect; }
  qreal minx=m_elements.at(0).x,maxx=minx,miny=m_elements.at(0).y,maxy=miny;
  for (int i=1;i<m_elements.size();++i) { const auto &e=m_elements.at(i); if (e.x<minx) minx=e.x; if (e.x>maxx) maxx=e.x; if (e.y<miny) miny=e.y; if (e.y>maxy) maxy=e.y; }
  m_cachedControlRect=PkRectF(minx,miny,maxx-minx,maxy-miny); m_dirtyControlRect=false; return m_cachedControlRect; }
bool PkPainterPath::isClosed() const
{ if (m_elements.isEmpty()) return false; PkPointF sp(0,0),lp(0,0); bool fs=false,fl=false;
  for (int i=0;i<m_elements.size();++i) { const auto &e=m_elements.at(i); if (e.type==MoveToElement) { sp=PkPointF(e.x,e.y); fs=true; } lp=PkPointF(e.x,e.y); fl=true; }
  return fs&&fl&&pkQtFuzzyCompare(lp.x(),sp.x())&&pkQtFuzzyCompare(lp.y(),sp.y()); }
void PkPainterPath::setElementPositionAt(int i, qreal x, qreal y) { m_elements[i].x=x; m_elements[i].y=y; markDirty(); }

bool PkPainterPath::contains(const PkPointF &pt) const
{
    if (isEmpty()||!controlPointRect().contains(pt)) return false;
    int wn=0; PkPointF lp,ls;
    for (int i=0;i<m_elements.size();++i) {
        const auto &e=m_elements.at(i);
        switch (e.type) {
        case MoveToElement: if (i>0) pkPainterPathIsectLine(lp,ls,pt,&wn); ls=lp=PkPointF(e.x,e.y); break;
        case LineToElement: pkPainterPathIsectLine(lp,PkPointF(e.x,e.y),pt,&wn); lp=PkPointF(e.x,e.y); break;
        case CurveToElement: { auto cp2=m_elements.at(++i),ep=m_elements.at(++i); pkPainterPathIsectCurve(PkArcBezier::fromPoints(lp,PkPointF(e.x,e.y),PkPointF(cp2.x,cp2.y),PkPointF(ep.x,ep.y)),pt,&wn); lp=PkPointF(ep.x,ep.y); break; }
        default: break;
        }
    }
    if (lp!=ls) pkPainterPathIsectLine(lp,ls,pt,&wn);
    return (m_fillRule==Qt::WindingFill)?(wn!=0):((wn%2)!=0);
}

bool PkPainterPath::contains(const PkRectF &rect) const
{
    if (isEmpty()||!controlPointRect().contains(rect)) return false;
    if (pkCheckCrossing(this,rect)) { if (m_fillRule==Qt::OddEvenFill) return false; if (!contains(rect.topLeft())||!contains(rect.topRight())||!contains(rect.bottomRight())||!contains(rect.bottomLeft())) return false; }
    return contains(rect.center());
}

bool PkPainterPath::intersects(const PkRectF &rect) const
{
    if (elementCount()==1&&rect.contains(elementAt(0))) return true;
    if (isEmpty()) return false;
    PkRectF cp=controlPointRect(),rn=rect.normalized();
    if (qMax(rn.left(),cp.left())>qMin(rn.right(),cp.right())||qMax(rn.top(),cp.top())>qMin(rn.bottom(),cp.bottom())) return false;
    if (pkCheckCrossing(this,rect)) return true;
    if (contains(rect.center())) return true;
    for (int i=0;i<m_elements.size();++i) { const auto &e=m_elements.at(i); if (e.type==MoveToElement&&rect.contains(PkPointF(e.x,e.y))) return true; }
    return false;
}

void PkPainterPath::translate(qreal dx, qreal dy)
{ if (pkQtFuzzyCompare(dx,0)&&pkQtFuzzyCompare(dy,0)) return; for (int i=0;i<m_elements.size();++i) { m_elements[i].x+=dx; m_elements[i].y+=dy; } m_currentPos+=PkPointF(dx,dy); markDirty(); }
PkPainterPath PkPainterPath::translated(qreal dx, qreal dy) const { PkPainterPath c(*this); c.translate(dx,dy); return c; }

bool PkPainterPath::operator==(const PkPainterPath &other) const
{ if (m_elements.size()!=other.m_elements.size()) return false; if (m_fillRule!=other.m_fillRule) return false; for (int i=0;i<m_elements.size();++i) if (m_elements.at(i)!=other.m_elements.at(i)) return false; return true; }

// 摊平 + 转换
PkVector<PkPolygonF> PkPainterPath::toSubpathPolygons(const PkTransform &matrix) const
{
    PkVector<PkPolygonF> flatCurves; if (isEmpty()) return flatCurves;
    PkPolygonF current;
    for (int i=0;i<elementCount();++i) {
        const auto &e=elementAt(i);
        switch (e.type) {
        case MoveToElement: if (current.size()>1) flatCurves.append(current); current.clear(); current.reserve(16); current.append(PkPointF(e.x,e.y)*matrix); break;
        case LineToElement: current.append(PkPointF(e.x,e.y)*matrix); break;
        case CurveToElement: { PkPointF cp2=elementAt(i+1),ep=elementAt(i+2); PkArcBezier b=PkArcBezier::fromPoints(PkPointF(elementAt(i-1).x,elementAt(i-1).y)*matrix,PkPointF(e.x,e.y)*matrix,cp2*matrix,ep*matrix); pkAddBezierToPolygon(b,&current); i+=2; break; }
        default: break;
        }
    }
    if (current.size()>1) flatCurves.append(current);
    return flatCurves;
}

PkVector<PkPolygonF> PkPainterPath::toFillPolygons(const PkTransform &matrix) const
{
    PkVector<PkPolygonF> subpaths=toSubpathPolygons(matrix),polys;
    int count=subpaths.size(); if (count==0) return polys;
    PkVector<PkRectF> bounds; bounds.reserve(count);
    for (int i=0;i<count;++i) bounds.append(subpaths.at(i).boundingRect());
    PkVector<PkVector<int>> isects; isects.resize(count);
    for (int j=0;j<count;++j) { if (subpaths.at(j).size()<=2) continue; PkRectF cb=bounds.at(j); for (int i=0;i<count;++i) if (cb.intersects(bounds.at(i))) isects[j].append(i); }
    for (int i=0;i<count;++i) { const auto &ci=isects.at(i); for (int j=0;j<ci.size();++j) { int ij=ci.at(j); if (ij==i) continue; const auto &ij2=isects.at(ij); for (int k=0;k<ij2.size();++k) { int ik=ij2.at(k); if (ik!=i&&!isects.at(i).contains(ik)) isects[i].append(ik); } isects[ij].clear(); } }
    for (int i=0;i<count;++i) { const auto &sl=isects.at(i); if (!sl.isEmpty()) { PkPolygonF bu; for (int j=0;j<sl.size();++j) { const auto &sp=subpaths.at(sl.at(j)); bu.append(sp); if (!sp.isClosed()) bu.append(sp.first()); if (!bu.isClosed()) bu.append(bu.first()); } polys.append(bu); } }
    return polys;
}

PkPolygonF PkPainterPath::toFillPolygon(const PkTransform &matrix) const
{
    PkVector<PkPolygonF> flats=toSubpathPolygons(matrix); PkPolygonF poly;
    if (flats.isEmpty()) return poly;
    PkPointF first=flats.first().first();
    for (int i=0;i<flats.size();++i) { poly.append(flats.at(i)); if (!flats.at(i).isClosed()) poly.append(flats.at(i).first()); if (i>0) poly.append(first); }
    return poly;
}

PkPainterPath PkPainterPath::toReversed() const
{
    if (isEmpty()) return *this;
    PkPainterPath rev; rev.moveTo(PkPointF(m_elements.at(m_elements.size()-1).x,m_elements.at(m_elements.size()-1).y));
    for (int i=m_elements.size()-1;i>=1;--i) {
        const auto &elm=m_elements.at(i),&prev=m_elements.at(i-1);
        switch (elm.type) {
        case LineToElement: rev.lineTo(PkPointF(prev.x,prev.y)); break;
        case MoveToElement: rev.moveTo(PkPointF(prev.x,prev.y)); break;
        case CurveToDataElement: { const auto &cp1=m_elements.at(i-2),&sp=m_elements.at(i-3); rev.cubicTo(PkPointF(prev.x,prev.y),PkPointF(cp1.x,cp1.y),PkPointF(sp.x,sp.y)); i-=3; break; }
        default: break;
        }
    }
    return rev;
}

// 测量
qreal PkPainterPath::length() const
{
    if (isEmpty()) return 0; qreal len=0;
    for (int i=1;i<m_elements.size();++i) {
        const auto &e=m_elements.at(i);
        switch (e.type) {
        case MoveToElement: break;
        case LineToElement: { PkPointF a(m_elements.at(i-1)),b(e); qreal dx=b.x()-a.x(),dy=b.y()-a.y(); len+=std::sqrt(dx*dx+dy*dy); break; }
        case CurveToElement: { PkArcBezier b=PkArcBezier::fromPoints(m_elements.at(i-1),e,m_elements.at(i+1),m_elements.at(i+2)); len+=pkBezierLength(b); i+=2; break; }
        default: break;
        }
    }
    return len;
}

qreal PkPainterPath::percentAtLength(qreal len) const
{
    if (isEmpty()||len<=0) return 0; qreal tl=length(); if (len>tl) return 1; qreal cl=0;
    for (int i=1;i<m_elements.size();++i) {
        const auto &e=m_elements.at(i);
        switch (e.type) {
        case MoveToElement: break;
        case LineToElement: { PkPointF a(m_elements.at(i-1)),b(e); qreal ll=std::sqrt((b.x()-a.x())*(b.x()-a.x())+(b.y()-a.y())*(b.y()-a.y())); cl+=ll; if (cl>=len) return len/tl; break; }
        case CurveToElement: { PkArcBezier b=PkArcBezier::fromPoints(m_elements.at(i-1),e,m_elements.at(i+1),m_elements.at(i+2)); qreal bl=pkBezierLength(b),pl=cl; cl+=bl; if (cl>=len) { qreal res=(len-pl)/bl; return (res*bl+pl)/tl; } i+=2; break; }
        default: break;
        }
    }
    return 0;
}

PkPointF PkPainterPath::pointAtPercent(qreal t) const
{
    if (t<0||t>1) return PkPointF(); if (m_elements.isEmpty()) return PkPointF(); if (m_elements.size()==1) return PkPointF(m_elements.at(0).x,m_elements.at(0).y);
    qreal tl=length(); if (tl<=0) return PkPointF(); qreal target=tl*t,cl=0;
    for (int i=1;i<m_elements.size();++i) {
        const auto &e=m_elements.at(i);
        switch (e.type) {
        case MoveToElement: break;
        case LineToElement: { PkPointF a(m_elements.at(i-1)),b(e); qreal dx=b.x()-a.x(),dy=b.y()-a.y(),ll=std::sqrt(dx*dx+dy*dy); if (cl+ll>=target||i==m_elements.size()-1) { qreal f=ll>0?(target-cl)/ll:0; f=qBound(qreal(0),f,qreal(1)); return PkPointF(a.x()+f*dx,a.y()+f*dy); } cl+=ll; break; }
        case CurveToElement: { PkArcBezier b=PkArcBezier::fromPoints(m_elements.at(i-1),e,m_elements.at(i+1),m_elements.at(i+2)); qreal bl=pkBezierLength(b); if (cl+bl>=target||i+2>=m_elements.size()-1) { qreal f=bl>0?(target-cl)/bl:0; f=qBound(qreal(0),f,qreal(1)); qreal mt=1-f; return PkPointF(b.x1*mt*mt*mt+3*b.x2*mt*mt*f+3*b.x3*mt*f*f+b.x4*f*f*f,b.y1*mt*mt*mt+3*b.y2*mt*mt*f+3*b.y3*mt*f*f+b.y4*f*f*f); } cl+=bl; i+=2; break; }
        default: break;
        }
    }
    return PkPointF(m_elements.at(m_elements.size()-1).x,m_elements.at(m_elements.size()-1).y);
}

qreal PkPainterPath::angleAtPercent(qreal t) const
{
    if (t<0||t>1) return 0; qreal tl=length(); if (tl<=0) return 0; qreal target=tl*t,cl=0;
    for (int i=1;i<m_elements.size();++i) {
        const auto &e=m_elements.at(i);
        switch (e.type) {
        case MoveToElement: break;
        case LineToElement: { PkPointF a(m_elements.at(i-1)),b(e); qreal dx=b.x()-a.x(),dy=b.y()-a.y(),ll=std::sqrt(dx*dx+dy*dy); cl+=ll; if (cl>=target||i==m_elements.size()-1) { qreal a2=std::atan2(dy,dx)*180.0/M_PI; return a2<0?a2+360:a2; } break; }
        case CurveToElement: { PkArcBezier b=PkArcBezier::fromPoints(m_elements.at(i-1),e,m_elements.at(i+1),m_elements.at(i+2)); qreal bl=pkBezierLength(b); cl+=bl; if (cl>=target||i+2>=m_elements.size()-1) { qreal f=bl>0?(target-(cl-bl))/bl:0; f=qBound(qreal(0),f,qreal(1)); qreal mt=1-f; qreal dx=3*mt*mt*(b.x2-b.x1)+6*mt*f*(b.x3-b.x2)+3*f*f*(b.x4-b.x3),dy=3*mt*mt*(b.y2-b.y1)+6*mt*f*(b.y3-b.y2)+3*f*f*(b.y4-b.y3); qreal a2=std::atan2(dy,dx)*180.0/M_PI; return a2<0?a2+360:a2; } i+=2; break; }
        default: break;
        }
    }
    return 0;
}

static_assert(std::is_standard_layout<PkPainterPath::Element>::value, "PkPainterPath::Element 必须是标准布局");