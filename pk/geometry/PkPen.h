#ifndef PK_GEOMETRY_PKPEN_H
#define PK_GEOMETRY_PKPEN_H

#include "PkBrush.h"
#include <PkGlobal.h>
#include <PkVector.h>

#include <vector>

// ---------------------------------------------------------------------------
// PkPen —— QPen 的零 Qt 替代（S-07-b 越锁修复补进 pk/geometry）。
//
// 形态照 `.exec/shell/kritaimage/compat/QPen`（S-06 壳 compat 的同型先例），
// 从壳垫片升格为 committed pk/geometry 头。理由：libs/image 公开头
// `kis_painter.h` 已前置声明 `class PkPen;` 并把 `const PkPen&` 用在
// `drawPainterPath` 签名里，libpaintop 的 freehand_stroke（QPAINTER_PATH 透传）
// 需要完整定义。S-05-b PkNodeId（pk/uuid）同型先例。
//
// **只做 data-carrier**：保存实测绘制命令与私有路径栅格器需要的 brush/color、
// width、style、cap/join、miter、dash/offset 与 cosmetic 状态。类本身不提供
// 任何绘制或栅格化方法。
class PkPen {
public:
    PkPen() = default;
    explicit PkPen(Qt::PenStyle style)
        : m_style(style) {}
    explicit PkPen(Qt::GlobalColor color, qreal widthF = 1.0)
        : m_brush(color), m_widthF(widthF) {}
    PkPen(const PkColor &color, qreal widthF = 1.0)
        : m_brush(color), m_widthF(widthF) {}
    PkPen(const PkBrush &brush, qreal widthF = 1.0)
        : m_brush(brush), m_widthF(widthF) {}
    PkPen(const PkPen &) = default;
    PkPen &operator=(const PkPen &) = default;

    void setColor(Qt::GlobalColor color) { m_brush.setColor(color); }
    void setColor(const PkColor &color) { m_brush.setColor(color); }
    PkBrush brush() const { return m_brush; }
    void setBrush(const PkBrush &brush) { m_brush = brush; }
    PkColor color() const { return m_brush.color(); }

    void setWidthF(qreal width)
    {
        if (width < 0.0 || width >= (1 << 15)) {
            return;
        }
        if (qAbs(m_widthF - width) < 0.00000001) {
            return;
        }
        m_widthF = width;
    }
    qreal widthF() const { return m_widthF; }
    int width() const { return qRound(m_widthF); }
    void setWidth(int width) { setWidthF(width); }

    void setStyle(Qt::PenStyle style)
    {
        if (m_style == style) {
            return;
        }
        m_style = style;
        m_dashPattern.clear();
        m_dashOffset = 0.0;
    }
    Qt::PenStyle style() const { return m_style; }

    void setCapStyle(Qt::PenCapStyle style) { m_capStyle = style; }
    Qt::PenCapStyle capStyle() const { return m_capStyle; }

    void setJoinStyle(Qt::PenJoinStyle style) { m_joinStyle = style; }
    Qt::PenJoinStyle joinStyle() const { return m_joinStyle; }

    void setMiterLimit(qreal limit) { m_miterLimit = limit; }
    qreal miterLimit() const { return m_miterLimit; }

    void setDashOffset(qreal offset)
    {
        if (qFuzzyCompare(offset, m_dashOffset)) {
            return;
        }
        if (m_style != Qt::CustomDashLine) {
            m_dashPattern = dashPattern();
            m_style = Qt::CustomDashLine;
        }
        m_dashOffset = offset;
    }
    qreal dashOffset() const { return m_dashOffset; }

    void setDashPattern(const std::vector<qreal> &pattern)
    {
        if (pattern.empty()) {
            return;
        }
        m_dashPattern = pattern;
        m_style = Qt::CustomDashLine;
        if ((m_dashPattern.size() % 2) == 1) {
            m_dashPattern.push_back(1.0);
        }
    }

    void setDashPattern(const PkVector<qreal> &pattern)
    {
        std::vector<qreal> values;
        values.reserve(static_cast<std::size_t>(pattern.size()));
        for (qreal value : pattern) {
            values.push_back(value);
        }
        setDashPattern(values);
    }

    std::vector<qreal> dashPattern() const
    {
        if (m_style == Qt::SolidLine || m_style == Qt::NoPen) {
            return {};
        }
        if (!m_dashPattern.empty()) {
            return m_dashPattern;
        }

        switch (m_style) {
        case Qt::DashLine:
            return {4.0, 2.0};
        case Qt::DotLine:
            return {1.0, 2.0};
        case Qt::DashDotLine:
            return {4.0, 2.0, 1.0, 2.0};
        case Qt::DashDotDotLine:
            return {4.0, 2.0, 1.0, 2.0, 1.0, 2.0};
        default:
            return {};
        }
    }

    bool isCosmetic() const { return m_cosmetic; }
    void setCosmetic(bool cosmetic) { m_cosmetic = cosmetic; }

private:
    PkBrush m_brush{Qt::black};
    qreal m_widthF = 1.0;
    Qt::PenStyle m_style = Qt::SolidLine;
    Qt::PenCapStyle m_capStyle = Qt::SquareCap;
    Qt::PenJoinStyle m_joinStyle = Qt::BevelJoin;
    qreal m_miterLimit = 2.0;
    qreal m_dashOffset = 0.0;
    std::vector<qreal> m_dashPattern;
    bool m_cosmetic = false;
};

#endif // PK_GEOMETRY_PKPEN_H
