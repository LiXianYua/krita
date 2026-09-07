#include "PkPainter.h"

PkPainter::PkPainter(PkPainterBackend &b) : m_backend(b) {}

void PkPainter::save() { m_stack.push_back(m_state); m_backend.submit(PkSaveCommand{}); }
void PkPainter::restore() { if (m_stack.empty()) return; m_state=m_stack.back(); m_stack.pop_back(); m_backend.submit(PkRestoreCommand{}); }

PkPen PkPainter::pen() const { return m_state.pen; }
void PkPainter::setPen(const PkPen &p) { m_state.pen=p; m_backend.submit(PkSetPenCommand{p}); }
void PkPainter::setPen(const PkColor &c,qreal w) { setPen(PkPen(c,w)); }
void PkPainter::setPen(Pk::PenStyle s) { PkPen p; p.setStyle(s); setPen(p); }

PkBrush PkPainter::brush() const { return m_state.brush; }
void PkPainter::setBrush(const PkBrush &b) { m_state.brush=b; m_backend.submit(PkSetBrushCommand{b}); }
void PkPainter::setBrush(const PkColor &c) { setBrush(PkBrush(c)); }
void PkPainter::setBrush(Pk::BrushStyle s) { setBrush(PkBrush(s)); }

void PkPainter::setFont(const PkFont &f) { m_state.font=f; m_backend.submit(PkSetFontCommand{f}); }
PkFont PkPainter::font() const { return m_state.font; }

PkTransform PkPainter::transform() const { return m_state.transform; }
void PkPainter::setTransform(const PkTransform &t,bool combine) {
    m_state.transform = combine ? t*m_state.transform : t;
    m_backend.submit(PkSetTransformCommand{t,combine});
}
void PkPainter::translate(qreal dx,qreal dy) {
    PkTransform t; t.translate(dx,dy);
    m_state.transform = t*m_state.transform;
    m_backend.submit(PkSetTransformCommand{t,true});
}
void PkPainter::scale(qreal sx,qreal sy) {
    PkTransform t; t.scale(sx,sy);
    m_state.transform = t*m_state.transform;
    m_backend.submit(PkSetTransformCommand{t,true});
}
void PkPainter::rotate(qreal degrees) {
    PkTransform t; t.rotate(degrees);
    m_state.transform = t*m_state.transform;
    m_backend.submit(PkSetTransformCommand{t,true});
}

void PkPainter::setRenderHint(unsigned h,bool e) {
    if(e) m_state.hints|=h; else m_state.hints&=~h;
    m_backend.submit(PkSetRenderHintCommand{h,e});
}
void PkPainter::setRenderHints(unsigned h,bool e) { setRenderHint(h,e); }
bool PkPainter::testRenderHint(unsigned hint) const { return (m_state.hints & hint) != 0; }

void PkPainter::setCompositionMode(Pk::CompositionMode mode) {
    m_state.mode = mode;
    m_backend.submit(PkSetCompositionModeCommand{mode});
}
Pk::CompositionMode PkPainter::compositionMode() const { return m_state.mode; }

void PkPainter::setOpacity(qreal opacity) {
    m_state.opacity = opacity;
    m_backend.submit(PkSetOpacityCommand{opacity});
}
qreal PkPainter::opacity() const { return m_state.opacity; }

void PkPainter::setClipRect(const PkRectF &r,Pk::ClipOperation o) {
    PkPainterPath path;
    path.addRect(r);
    if (o == Pk::NoClip) {
        m_state.clipPath = PkPainterPath();
        m_state.hasClip = false;
    } else if (o == Pk::ReplaceClip || !m_state.hasClip) {
        m_state.clipPath = path;
        m_state.hasClip = true;
    } else {
        m_state.clipPath &= path;
    }
    m_backend.submit(PkSetClipRectCommand{r,o});
}
void PkPainter::setClipPath(const PkPainterPath &p,Pk::ClipOperation o) {
    if (o == Pk::NoClip) {
        m_state.clipPath = PkPainterPath();
        m_state.hasClip = false;
    } else if (o == Pk::ReplaceClip || !m_state.hasClip) {
        m_state.clipPath = p;
        m_state.hasClip = true;
    } else {
        m_state.clipPath &= p;
    }
    m_backend.submit(PkSetClipPathCommand{p,o});
}
bool PkPainter::hasClipping() const { return m_state.hasClip; }

PkRectF PkPainter::clipBoundingRect() const {
    // Qt 语义：当前裁剪区域与设备矩形的交。这里没有设备矩形概念，
    // 退化为「裁剪路径的包围盒」；未设裁剪时返回空矩形（Qt 返回设备矩形）。
    return m_state.hasClip ? m_state.clipPath.boundingRect() : PkRectF();
}

void PkPainter::drawLine(const PkLineF &l) { m_backend.submit(PkDrawLineCommand{l}); }
void PkPainter::drawLine(const PkPointF &a,const PkPointF &b) { drawLine(PkLineF(a,b)); }
void PkPainter::drawRect(const PkRectF &r) { m_backend.submit(PkDrawRectCommand{r}); }
void PkPainter::drawEllipse(const PkRectF &r) { m_backend.submit(PkDrawEllipseCommand{r}); }
void PkPainter::drawEllipse(const PkPointF &c,qreal rx,qreal ry) { drawEllipse(PkRectF(c.x()-rx,c.y()-ry,2*rx,2*ry)); }
void PkPainter::drawArc(const PkRectF &r,int s,int span) { m_backend.submit(PkDrawArcCommand{r,s,span}); }
void PkPainter::drawPath(const PkPainterPath &p) { m_backend.submit(PkDrawPathCommand{p}); }
void PkPainter::drawPolygon(const PkPolygonF &p) { m_backend.submit(PkDrawPolygonCommand{p}); }
void PkPainter::drawPoint(const PkPointF &p) { m_backend.submit(PkDrawPointCommand{p}); }
void PkPainter::strokePath(const PkPainterPath &p,const PkPen &pen) { m_backend.submit(PkStrokePathCommand{p,pen}); }

void PkPainter::fillRect(const PkRectF &r) { fillRect(r, m_state.brush); }
void PkPainter::fillRect(const PkRectF &r,const PkBrush &b) { m_backend.submit(PkFillRectCommand{r,b}); }
void PkPainter::fillPath(const PkPainterPath &p) { fillPath(p, m_state.brush); }
void PkPainter::fillPath(const PkPainterPath &p,const PkBrush &b) { m_backend.submit(PkFillPathCommand{p,b}); }

void PkPainter::drawImage(const PkRectF &r,const PkImage &i) { m_backend.submit(PkDrawImageCommand{r,i}); }
void PkPainter::drawPixmap(const PkPointF &pos,const PkImage &img) {
    m_backend.submit(PkDrawPixmapCommand{PkRectF(pos, PkSizeF(img.width(), img.height())), img, PkRectF()});
}
void PkPainter::drawPixmap(const PkRectF &target,const PkImage &img,const PkRectF &source) {
    m_backend.submit(PkDrawPixmapCommand{target, img, source});
}
void PkPainter::drawTiledPixmap(const PkRectF &r,const PkImage &img,const PkPointF &offset) {
    m_backend.submit(PkDrawTiledPixmapCommand{r, img, offset});
}

void PkPainter::drawText(const PkPointF &pos,const PkString &text) {
    m_backend.submit(PkDrawTextAtPointCommand{pos, text});
}
void PkPainter::drawText(const PkRectF &rect,const PkString &text) {
    m_backend.submit(PkDrawTextInRectCommand{rect, text});
}
