#pragma once
#include "PkPaintCommand.h"
#include <vector>

class PkPainter {
public:
    explicit PkPainter(PkPainterBackend &backend);
    void save(); void restore();
    PkPen pen() const; void setPen(const PkPen &); void setPen(const PkColor &, qreal width=1.0); void setPen(Qt::PenStyle);
    PkBrush brush() const; void setBrush(const PkBrush &); void setBrush(const PkColor &); void setBrush(Qt::BrushStyle);
    PkTransform transform() const; void setTransform(const PkTransform &, bool combine=false);
    void setRenderHint(unsigned, bool enabled=true); void setRenderHints(unsigned, bool enabled=true);
    void setClipRect(const PkRectF &, Qt::ClipOperation=Qt::ReplaceClip);
    void drawLine(const PkLineF &); void drawLine(const PkPointF &, const PkPointF &);
    void drawRect(const PkRectF &);
    void drawEllipse(const PkRectF &); void drawEllipse(const PkPointF &, qreal, qreal);
    void drawArc(const PkRectF &, int, int);
    void drawPath(const PkPainterPath &); void drawPolygon(const PkPolygonF &);
    void drawImage(const PkRectF &, const PkImage &);
private:
    struct State { PkPen pen; PkBrush brush; PkTransform transform; unsigned hints=0; };
    PkPainterBackend &m_backend; State m_state; std::vector<State> m_stack;
};
