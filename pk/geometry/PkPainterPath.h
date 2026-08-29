#pragma once

// ---------------------------------------------------------------------------
// PkPainterPath —— QPainterPath 的零 Qt 替代品（R-22 T1）。
//
// 逐字抄自真 Qt 5.15.7 的 qpainterpath.h（class 声明）、qpainterpath.cpp
// （out-of-line 成员）与 qpainterpath_p.h（close/maybeMoveTo 状态）。源码取自
// 上游 tag `v5.15.7-lts-lgpl`。
//
// COW 由 PkVector<Element> 自带（PkArrayContainer 的 PkMut()/PkConst() 机制），
// 不复刻 Qt 的 QScopedPointer<QPainterPathPrivate> + detach 调用模式。
// PkVector 的拷贝 O(1)，值语义与 Qt 一致。
//
// 缓存：mutable PkRectF cachedBounds + mutable PkRectF cachedControlPointRect +
// mutable bool dirtyBound/dirtyControl 的惰性更新模式（与 Qt 的 computeBoundingRect
// 同形态，但略去 type 的惰性缓存——QPainterPath 的 type() 也是 lazy，但本类型
// 保留范围内无真实调用点，按判据①不做）。
// ---------------------------------------------------------------------------

#include "PkGlobal.h"
#include "PkPoint.h"
#include "PkRect.h"
#include "PkPolygon.h"
#include "../container/PkVector.h"

class PkTransform;

class PkPainterPath
{
public:
    // ── ElementType ──────────────────────────────────────────────────────────
    // 取值顺序 0/1/2/3 照 Qt 头文件原样（与 PkTransform::TransformationType 的
    // 位标志陷阱同型——顺序错全错）。
    enum ElementType {
        MoveToElement     = 0,
        LineToElement     = 1,
        CurveToElement    = 2,
        CurveToDataElement = 3
    };

    // ── Element ──────────────────────────────────────────────────────────────
    // qpainterpath.h:49-62。真实调用点：kis_algebra_2d.cpp:1608,1806,2059
    // （`QPainterPath::Element e = path.elementAt(i)`）、
    // kis_polygonal_gradient_shape_strategy.cpp:36,60（`.isMoveTo()`、隐式转 QPointF）。
    class Element {
    public:
        qreal x = 0;
        qreal y = 0;
        ElementType type = MoveToElement;

        Element() = default;
        Element(qreal x_, qreal y_, ElementType t) : x(x_), y(y_), type(t) {}

        bool isMoveTo() const { return type == MoveToElement; }
        bool isLineTo() const { return type == LineToElement; }
        bool isCurveTo() const { return type == CurveToElement; }

        operator PkPointF() const { return PkPointF(x, y); }

        // qpainterpath.h:60 —— 用 pkQtFuzzyCompare 而不是 qFuzzyCompare
        // （理由同 PkPointF::operator== 与 PkPolygon.cpp 的 pkPolygonIsectLine：
        // 出现 qFuzzyCompare 的字符串会在「pk/test 那份垫片先进 TU」的路径上被
        // 预处理器悄悄改写成另一套公式）。
        bool operator==(const Element &e) const
        {
            return pkQtFuzzyCompare(x, e.x)
                && pkQtFuzzyCompare(y, e.y)
                && type == e.type;
        }
        bool operator!=(const Element &e) const { return !(*this == e); }
    };

    // ── 构造/析构/拷贝 ──────────────────────────────────────────────────────

    // qpainterpath.h:65
    PkPainterPath() noexcept;
    // qpainterpath.h:66 —— explicit：QPainterPath 的构造没有隐式转换。
    explicit PkPainterPath(const PkPointF &startPoint);

    // 拷贝/移动：PkVector 自带 O(1) COW，= default 即可。
    PkPainterPath(const PkPainterPath &) = default;
    PkPainterPath &operator=(const PkPainterPath &) = default;
    PkPainterPath(PkPainterPath &&) noexcept = default;
    PkPainterPath &operator=(PkPainterPath &&other) noexcept = default;
    ~PkPainterPath() = default;

    void swap(PkPainterPath &other) noexcept;

    // ── 清理与预留 ──────────────────────────────────────────────────────────

    void clear();
    void reserve(int size);

    // ── 子路径构建 ──────────────────────────────────────────────────────────

    void closeSubpath();

    void moveTo(const PkPointF &p);
    inline void moveTo(qreal x, qreal y) { moveTo(PkPointF(x, y)); }

    void lineTo(const PkPointF &p);
    inline void lineTo(qreal x, qreal y) { lineTo(PkPointF(x, y)); }

    void cubicTo(const PkPointF &ctrlPt1, const PkPointF &ctrlPt2, const PkPointF &endPt);
    inline void cubicTo(qreal ctrlPt1x, qreal ctrlPt1y,
                        qreal ctrlPt2x, qreal ctrlPt2y,
                        qreal endPtx, qreal endPty)
    {
        cubicTo(PkPointF(ctrlPt1x, ctrlPt1y),
                PkPointF(ctrlPt2x, ctrlPt2y),
                PkPointF(endPtx, endPty));
    }

    void quadTo(const PkPointF &ctrlPt, const PkPointF &endPt);
    inline void quadTo(qreal ctrlPtx, qreal ctrlPty,
                       qreal endPtx, qreal endPty)
    {
        quadTo(PkPointF(ctrlPtx, ctrlPty), PkPointF(endPtx, endPty));
    }

    PkPointF currentPosition() const;

    // ── 形状附加 ──────────────────────────────────────────────────────────

    void addRect(const PkRectF &rect);
    inline void addRect(qreal x, qreal y, qreal w, qreal h)
    { addRect(PkRectF(x, y, w, h)); }

    void addPolygon(const PkPolygonF &polygon);

    void addEllipse(const PkRectF &boundingRect);
    inline void addEllipse(qreal x, qreal y, qreal w, qreal h)
    { addEllipse(PkRectF(x, y, w, h)); }
    inline void addEllipse(const PkPointF &center, qreal rx, qreal ry)
    { addEllipse(PkRectF(center.x() - rx, center.y() - ry, 2 * rx, 2 * ry)); }

    void arcTo(const PkRectF &rect, qreal startAngle, qreal sweepLength);
    inline void arcTo(qreal x, qreal y, qreal w, qreal h,
                       qreal startAngle, qreal sweepLength)
    { arcTo(PkRectF(x, y, w, h), startAngle, sweepLength); }

    void addRoundedRect(const PkRectF &rect, qreal xRadius, qreal yRadius,
                        Qt::SizeMode mode = Qt::AbsoluteSize);
    inline void addRoundedRect(qreal x, qreal y, qreal w, qreal h,
                                qreal xRadius, qreal yRadius,
                                Qt::SizeMode mode = Qt::AbsoluteSize)
    { addRoundedRect(PkRectF(x, y, w, h), xRadius, yRadius, mode); }

    void addPath(const PkPainterPath &path);

    // ── 查询 ──────────────────────────────────────────────────────────

    // ── 查询（T1 + T3）──────────────────────────────────────────────────

    bool isEmpty() const;
    PkRectF boundingRect() const;
    PkRectF controlPointRect() const;
    bool isClosed() const;

    int elementCount() const { return m_elements.size(); }
    Element elementAt(int i) const { return m_elements.at(i); }
    void setElementPositionAt(int i, qreal x, qreal y);

    Qt::FillRule fillRule() const { return m_fillRule; }
    void setFillRule(Qt::FillRule fillRule)
    {
        if (m_fillRule == fillRule)
            return;
        detachForMutation();
        m_fillRule = fillRule;
    }

    // T3: 查询
    bool contains(const PkPointF &pt) const;
    bool contains(const PkRectF &rect) const;
    bool intersects(const PkRectF &rect) const;
    bool contains(const PkPainterPath &other) const;
    bool intersects(const PkPainterPath &other) const;

    PkPainterPath united(const PkPainterPath &other) const;
    PkPainterPath intersected(const PkPainterPath &other) const;
    PkPainterPath subtracted(const PkPainterPath &other) const;
    PkPainterPath simplified() const;

    PkPainterPath &operator&=(const PkPainterPath &other);
    PkPainterPath &operator|=(const PkPainterPath &other);
    PkPainterPath &operator+=(const PkPainterPath &other);
    PkPainterPath &operator-=(const PkPainterPath &other);

    // T3: 转换 + 摊平
    PkPolygonF toFillPolygon(const PkTransform &matrix) const;
    PkVector<PkPolygonF> toFillPolygons(const PkTransform &matrix) const;
    PkVector<PkPolygonF> toSubpathPolygons(const PkTransform &matrix) const;
    PkPainterPath toReversed() const;

    // T3: 测量
    qreal length() const;
    qreal percentAtLength(qreal t) const;
    PkPointF pointAtPercent(qreal t) const;
    qreal angleAtPercent(qreal t) const;

    // ── 变换（T1：translate/translated）────────────────────────────────────

    void translate(qreal dx, qreal dy);
    inline void translate(const PkPointF &offset) { translate(offset.x(), offset.y()); }

    PkPainterPath translated(qreal dx, qreal dy) const;
    inline PkPainterPath translated(const PkPointF &offset) const
    { return translated(offset.x(), offset.y()); }

    // ── 比较 ──────────────────────────────────────────────────────────

    bool operator==(const PkPainterPath &other) const;
    bool operator!=(const PkPainterPath &other) const { return !(*this == other); }

private:
    // 标记 boundingRect 与控制点矩形为脏（每次元素变更时调用）。
    void markDirty();

    // Qt 5.15 QPainterPathData 的 COW detach 会清 require_moveTo。PkVector
    // 单独承载元素 COW，因此每个写入口先在这里同步这项路径级状态。
    void detachForMutation();
    void maybeMoveTo();
    bool currentSubpathClosedExactly() const;

    // arcMoveTo 内部辅助：仅供 addRoundedRect 使用（qpainterpath.cpp:1033-1041）
    void arcMoveTo(const PkRectF &rect, qreal angle);

    // 存储：PkVector<Element> 自带 COW，拷贝 O(1)。
    PkVector<Element> m_elements;
    // 当前位置：一个子路径的末端（连续 moveTo 时更新，closeSubpath 时回到
    // 子路径起点）。
    PkPointF m_currentPos{0, 0};
    // closeSubpath()/closed shape 之后，下一条非 move 构建命令先物化一个
    // trailing MoveTo。对应 Qt 5.15 QPainterPathData::require_moveTo。
    bool m_requireMoveTo = false;
    // 缓存：Qt 的 computeBoundingRect/computeControlPointRect 模式。
    mutable PkRectF m_cachedBounds{0, 0, 0, 0};
    mutable PkRectF m_cachedControlRect{0, 0, 0, 0};
    mutable bool m_dirtyBounds = true;
    mutable bool m_dirtyControlRect = true;
    // 填充规则。
    Qt::FillRule m_fillRule = Qt::OddEvenFill;
};

// qpainterpath.h 末尾的 operator*（QPainterPath × QTransform）—— 自由函数。
// 定义在 T5（偏离 21 闭合）里实现，这里只做声明。
inline PkPainterPath operator*(const PkPainterPath &p, const PkTransform &t);

inline PkPainterPath operator&(const PkPainterPath &lhs, const PkPainterPath &rhs)
{ return lhs.intersected(rhs); }
inline PkPainterPath operator|(const PkPainterPath &lhs, const PkPainterPath &rhs)
{ return lhs.united(rhs); }
inline PkPainterPath operator+(const PkPainterPath &lhs, const PkPainterPath &rhs)
{ return lhs.united(rhs); }
inline PkPainterPath operator-(const PkPainterPath &lhs, const PkPainterPath &rhs)
{ return lhs.subtracted(rhs); }
