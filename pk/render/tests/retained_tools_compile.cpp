#include "PkPainter.h"

void cropPainterCalls(PkPainter &p, const PkPainterPath &path, const PkPen &pen)
{
    p.save();
    p.setPen(Qt::NoPen);
    p.setPen(pen);
    p.setBrush(PkColor(Qt::red));
    p.drawPath(path);
    p.setClipRect(PkRectF(0, 0, 10, 10), Qt::IntersectClip);
    p.drawLine(PkPointF(0, 0), PkPointF(1, 1));
    p.restore();
}

void knifePainterCalls(PkPainter &p, PkPen pen, const PkLineF &line,
                       const PkPolygonF &polygon)
{
    pen.setColor(PkColor(Qt::black));
    pen.setBrush(PkBrush(Qt::Dense3Pattern));
    pen.setWidth(2);
    pen.setWidthF(2.5);
    pen.setStyle(Qt::DashLine);
    pen.setCapStyle(Qt::RoundCap);
    pen.setDashPattern({2.0, 3.0});
    pen.setCosmetic(true);
    const PkColor color = pen.color();
    const PkBrush brush = pen.brush();
    const int width = pen.width();
    const qreal widthF = pen.widthF();
    const Qt::PenStyle style = pen.style();
    const Qt::PenCapStyle cap = pen.capStyle();
    const std::vector<qreal> dash = pen.dashPattern();
    const bool cosmetic = pen.isCosmetic();
    (void)color; (void)brush; (void)width; (void)widthF;
    (void)style; (void)cap; (void)dash; (void)cosmetic;

    p.save();
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);
    p.setTransform(p.transform(), false);
    p.setRenderHint(1u, false);
    p.drawLine(line);
    p.drawLine(PkPointF(0, 0), PkPointF(1, 1));
    p.drawPolygon(polygon);
    p.drawArc(PkRectF(0, 0, 1, 1), 0, 90);
    p.drawEllipse(PkPointF(1, 1), 2, 3);
    p.restore();
}

void karbonPainterCalls(PkPainter &p, const PkColor &color)
{
    p.save();
    p.setRenderHints(1u, false);
    p.setPen(color);
    p.drawRect(PkRectF(0, 0, 2, 2));
    p.setTransform(p.transform());
    p.restore();
}

void smartPatchPainterCalls(PkPainter &p, const PkImage &image)
{
    p.save();
    p.setBrush(PkColor(Qt::red));
    p.drawImage(PkRectF(0, 0, 1, 1), image);
    p.restore();
}
