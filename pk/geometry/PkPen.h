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
// **只做 data-carrier**：QPainter 路径栅格化在壳/内核里不可用（无 pk 栅格器），
// 这里只存一条颜色 + 宽度。消费方仅做透传（FreehandStrokeStrategy::Data 把
// QPAINTER_PATH/QPAINTER_PATH_FILL dab 的 pen 参数传给
// KisPainter::drawPainterPath）。路径→遮罩栅格化归 S-09/M5 重实现。
// 禁当完整 QPainter 描边。
class PkPen {
public:
    PkPen() : m_color(Qt::black), m_widthF(1.0), m_style(Qt::SolidLine), m_cap(Qt::SquareCap), m_cosmetic(false) {}
    explicit PkPen(Qt::GlobalColor color, qreal widthF = 1.0)
        : m_color(color), m_widthF(widthF), m_style(Qt::SolidLine), m_cap(Qt::SquareCap), m_cosmetic(false) {}
    PkPen(const PkColor &color, qreal widthF = 1.0)
        : m_color(color), m_widthF(widthF), m_style(Qt::SolidLine), m_cap(Qt::SquareCap), m_cosmetic(false) {}
    PkPen(const PkBrush &brush, qreal widthF = 1.0)
        : m_color(brush.color()), m_widthF(widthF), m_style(Qt::SolidLine), m_cap(Qt::SquareCap), m_cosmetic(false) {}
    PkPen(const PkPen &) = default;
    PkPen &operator=(const PkPen &) = default;

    void setColor(Qt::GlobalColor color) { m_color = color; }
    void setColor(const PkColor &color) { m_color = color; }
    PkBrush brush() const { PkBrush b(m_color); b.setStyle(Qt::SolidPattern); return b; }
    void setBrush(const PkBrush &brush) { m_color = brush.color(); }
    PkColor color() const { return m_color; }
    qreal widthF() const { return m_widthF; }
    int width() const { return static_cast<int>(m_widthF); }
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
    PkColor m_color;
    qreal m_widthF;
    Qt::PenStyle m_style;
    Qt::PenCapStyle m_cap;
    std::vector<qreal> m_dash;
    bool m_cosmetic;
};

#endif // PK_GEOMETRY_PKPEN_H
