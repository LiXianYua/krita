#pragma once
#include "PkColor.h"
#include "PkTransform.h"
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
    PkTransform transform() const { return m_transform; }
    void setTransform(const PkTransform &transform) { m_transform = transform; }
    bool operator==(const PkBrush &o) const { return m_color == o.m_color && m_style == o.m_style && m_transform == o.m_transform; }
    bool operator!=(const PkBrush &o) const { return !(*this == o); }
private:
    PkColor m_color;
    Pk::BrushStyle m_style;
    PkTransform m_transform;
};
