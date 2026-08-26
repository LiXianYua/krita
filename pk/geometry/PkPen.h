#ifndef PK_GEOMETRY_PKPEN_H
#define PK_GEOMETRY_PKPEN_H

#include <PkColor.h>   // Qt::GlobalColor（PkGlobal.h/PkNamespace.h）+ PkColor
#include <PkGlobal.h>  // qreal + namespace Qt 标量枚举（GlobalColor/black）

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
    PkPen() : m_color(Qt::black), m_widthF(1.0) {}
    explicit PkPen(Qt::GlobalColor color, qreal widthF = 1.0)
        : m_color(color), m_widthF(widthF) {}
    PkPen(const PkColor &color, qreal widthF = 1.0)
        : m_color(color), m_widthF(widthF) {}
    PkPen(const PkPen &) = default;
    PkPen &operator=(const PkPen &) = default;

    void setColor(Qt::GlobalColor color) { m_color = color; }
    PkColor color() const { return m_color; }
    qreal widthF() const { return m_widthF; }

private:
    PkColor m_color;
    qreal m_widthF;
};

#endif // PK_GEOMETRY_PKPEN_H
