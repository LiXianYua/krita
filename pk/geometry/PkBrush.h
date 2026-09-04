#pragma once
#include "PkColor.h"
#include "../namespace/PkNamespace.h"

class PkBrush {
public:
    PkBrush() : m_color(Pk::black), m_style(Pk::NoBrush) {}
    explicit PkBrush(Pk::GlobalColor c) : m_color(c), m_style(Pk::SolidPattern) {}
    explicit PkBrush(const PkColor &c) : m_color(c), m_style(Pk::SolidPattern) {}
    explicit PkBrush(Pk::BrushStyle s) : m_color(Pk::black), m_style(s) {}
    PkColor color() const { return m_color; }
    void setColor(Pk::GlobalColor c) { m_color = c; }
    void setColor(const PkColor &c) { m_color = c; }
    Pk::BrushStyle style() const { return m_style; }
    void setStyle(Pk::BrushStyle s) { m_style = s; }
private:
    PkColor m_color;
    Pk::BrushStyle m_style;
};
