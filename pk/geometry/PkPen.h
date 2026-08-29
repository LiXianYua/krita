#ifndef PK_GEOMETRY_PKPEN_H
#define PK_GEOMETRY_PKPEN_H

#include <PkColor.h>   // Qt::GlobalColor（PkGlobal.h/PkNamespace.h）+ PkColor
#include <PkGlobal.h>  // qreal + namespace Qt 标量枚举（GlobalColor/black）
#include "PkBrush.h"
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
// **只做 measured data-carrier**：保存实测消费方需要的 brush/color、width、
// pen style、cap、dash pattern 与 cosmetic 状态。QPainter 路径栅格化仍不在
// pk/geometry；这里只传递值，不实现描边、join/miter 或 rasterization。
class PkPen {
public:
    PkPen() : m_brush(Qt::black), m_widthF(1.0), m_style(Qt::SolidLine), m_cap(Qt::SquareCap), m_cosmetic(false) {}
    explicit PkPen(Qt::GlobalColor color, qreal widthF = 1.0)
        : m_brush(color), m_widthF(widthF), m_style(Qt::SolidLine), m_cap(Qt::SquareCap), m_cosmetic(false) {}
    PkPen(const PkColor &color, qreal widthF = 1.0)
        : m_brush(color), m_widthF(widthF), m_style(Qt::SolidLine), m_cap(Qt::SquareCap), m_cosmetic(false) {}
    PkPen(const PkBrush &brush, qreal widthF = 1.0)
        : m_brush(brush), m_widthF(widthF), m_style(Qt::SolidLine), m_cap(Qt::SquareCap), m_cosmetic(false) {}
    PkPen(const PkPen &) = default;
    PkPen &operator=(const PkPen &) = default;

    void setColor(Qt::GlobalColor color) { m_brush.setColor(color); }
    void setColor(const PkColor &color) { m_brush.setColor(color); }
    PkBrush brush() const { return m_brush; }
    void setBrush(const PkBrush &brush) { m_brush = brush; }
    PkColor color() const { return m_brush.color(); }
    qreal widthF() const { return m_widthF; }
    int width() const { return qRound(m_widthF); }
    void setWidth(int w) { m_widthF = w; }
    void setWidthF(qreal w) { m_widthF = w; }
    Qt::PenStyle style() const { return m_style; }
    void setStyle(Qt::PenStyle s) { m_style = s; }
    Qt::PenCapStyle capStyle() const { return m_cap; }
    void setCapStyle(Qt::PenCapStyle c) { m_cap = c; }
    std::vector<qreal> dashPattern() const { return m_dash; }
    void setDashPattern(const std::vector<qreal> &d) { m_dash = d; }
    bool isCosmetic() const { return m_cosmetic; }
    void setCosmetic(bool c) { m_cosmetic = c; }

private:
    PkBrush m_brush;
    qreal m_widthF;
    Qt::PenStyle m_style;
    Qt::PenCapStyle m_cap;
    std::vector<qreal> m_dash;
    bool m_cosmetic;
};

#endif // PK_GEOMETRY_PKPEN_H
