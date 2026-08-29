#include "PkPainter.h"
void retained_tools_compile(PkPainter &p, const PkPainterPath &path, const PkImage &image) {
 p.save(); p.setPen(Qt::NoPen); p.setBrush(PkColor(Qt::red)); p.drawPath(path);
 p.setClipRect(PkRectF(0,0,10,10), Qt::IntersectClip); p.drawLine(PkPointF(0,0),PkPointF(1,1));
 p.setRenderHint(1u,false); p.drawPolygon(PkPolygonF()); p.drawArc(PkRectF(0,0,1,1),0,90);
 p.drawEllipse(PkPointF(1,1),2,3); p.drawRect(PkRectF(0,0,2,2)); p.drawImage(PkRectF(0,0,1,1), image); p.restore();
}
