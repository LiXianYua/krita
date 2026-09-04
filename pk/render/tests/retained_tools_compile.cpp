#include "PkPainter.h"

void cropPainterCalls(PkPainter &p, const PkPainterPath &path)
{
    PkPen pen(Pk::SolidLine);
    p.save();
    p.setPen(Pk::NoPen);
    p.setPen(pen);
    p.setBrush(PkColor(Pk::red));
    p.drawPath(path);
    p.setClipRect(PkRectF(0, 0, 10, 10), Pk::IntersectClip);
    p.drawLine(PkPointF(0, 0), PkPointF(1, 1));
    p.restore();
}

void knifePainterCalls(PkPainter &p, PkPen pen, const PkLineF &line,
                       const PkPolygonF &polygon)
{
    pen.setColor(PkColor(Pk::black));
    pen.setBrush(PkBrush(Pk::Dense3Pattern));
    pen.setWidth(2);
    pen.setWidthF(2.5);
    pen.setStyle(Pk::DashLine);
    pen.setCapStyle(Pk::RoundCap);
    pen.setDashPattern({2.0, 3.0});
    pen.setCosmetic(true);
    const PkColor color = pen.color();
    const PkBrush brush = pen.brush();
    const int width = pen.width();
    const qreal widthF = pen.widthF();
    const Pk::PenStyle style = pen.style();
    const Pk::PenCapStyle cap = pen.capStyle();
    const std::vector<qreal> dash = pen.dashPattern();
    const bool cosmetic = pen.isCosmetic();
    (void)color; (void)brush; (void)width; (void)widthF;
    (void)style; (void)cap; (void)dash; (void)cosmetic;

    p.save();
    p.setPen(pen);
    p.setBrush(Pk::NoBrush);
    p.setTransform(p.transform(), false);
    p.setRenderHint(PkPainter::RenderHint::Antialiasing, true);
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
    p.setRenderHints(PkPainter::Antialiasing, false);
    p.setPen(color);
    p.drawRect(PkRectF(0, 0, 2, 2));
    p.setTransform(p.transform());
    p.restore();
}

void smartPatchPainterCalls(PkPainter &p, const PkImage &image)
{
    p.save();
    p.setBrush(PkColor(Pk::red));
    p.drawImage(PkRectF(0, 0, 1, 1), image);
    p.restore();
}
