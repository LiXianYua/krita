#pragma once
#include "PkColor.h"
#include "../namespace/PkNamespace.h"

class PkBrush {
public:
    PkBrush() : m_color(Qt::black), m_style(Qt::NoBrush) {}
    explicit PkBrush(Qt::GlobalColor c) : m_color(c), m_style(Qt::SolidPattern) {}
    explicit PkBrush(const PkColor &c) : m_color(c), m_style(Qt::SolidPattern) {}
    explicit PkBrush(Qt::BrushStyle s) : m_color(Qt::black), m_style(s) {}
    PkColor color() const { return m_color; }
    void setColor(Qt::GlobalColor c) { m_color = c; }
    void setColor(const PkColor &c) { m_color = c; }
    Qt::BrushStyle style() const { return m_style; }
    void setStyle(Qt::BrushStyle s) { m_style = s; }
private:
    PkColor m_color;
    Qt::BrushStyle m_style;
};
