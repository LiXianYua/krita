#include "PkPainter.h"
PkPainter::PkPainter(PkPainterBackend &b) : m_backend(b) {}
void PkPainter::save() { m_stack.push_back(m_state); m_backend.submit(PkSaveCommand{}); }
void PkPainter::restore() { if (m_stack.empty()) return; m_state=m_stack.back(); m_stack.pop_back(); m_backend.submit(PkRestoreCommand{}); }
PkPen PkPainter::pen() const { return m_state.pen; }
void PkPainter::setPen(const PkPen &p) { m_state.pen=p; m_backend.submit(PkSetPenCommand{p}); }
void PkPainter::setPen(const PkColor &c,qreal w) { setPen(PkPen(c,w)); }
void PkPainter::setPen(Qt::PenStyle s) { PkPen p=m_state.pen; p.setStyle(s); setPen(p); }
PkBrush PkPainter::brush() const { return m_state.brush; }
void PkPainter::setBrush(const PkBrush &b) { m_state.brush=b; m_backend.submit(PkSetBrushCommand{b}); }
void PkPainter::setBrush(const PkColor &c) { setBrush(PkBrush(c)); }
void PkPainter::setBrush(Qt::BrushStyle s) { PkBrush b=m_state.brush; b.setStyle(s); setBrush(b); }
PkTransform PkPainter::transform() const { return m_state.transform; }
void PkPainter::setTransform(const PkTransform &t,bool combine) { m_state.transform=combine ? m_state.transform*t : t; m_backend.submit(PkSetTransformCommand{t,combine}); }
void PkPainter::setRenderHint(unsigned h,bool e) { if(e) m_state.hints|=h; else m_state.hints&=~h; m_backend.submit(PkSetRenderHintCommand{h,e}); }
void PkPainter::setRenderHints(unsigned h,bool e) { setRenderHint(h,e); }
void PkPainter::setClipRect(const PkRectF &r,Qt::ClipOperation o) { m_backend.submit(PkSetClipRectCommand{r,o}); }
void PkPainter::drawLine(const PkLineF &l) { m_backend.submit(PkDrawLineCommand{l}); }
void PkPainter::drawLine(const PkPointF &a,const PkPointF &b) { drawLine(PkLineF(a,b)); }
void PkPainter::drawRect(const PkRectF &r) { m_backend.submit(PkDrawRectCommand{r}); }
void PkPainter::drawEllipse(const PkRectF &r) { m_backend.submit(PkDrawEllipseCommand{r}); }
void PkPainter::drawEllipse(const PkPointF &c,qreal rx,qreal ry) { drawEllipse(PkRectF(c.x()-rx,c.y()-ry,2*rx,2*ry)); }
void PkPainter::drawArc(const PkRectF &r,int s,int span) { m_backend.submit(PkDrawArcCommand{r,s,span}); }
void PkPainter::drawPath(const PkPainterPath &p) { m_backend.submit(PkDrawPathCommand{p}); }
void PkPainter::drawPolygon(const PkPolygonF &p) { m_backend.submit(PkDrawPolygonCommand{p}); }
void PkPainter::drawImage(const PkRectF &r,const PkImage &i) { m_backend.submit(PkDrawImageCommand{r,i}); }
