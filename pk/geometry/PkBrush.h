#pragma once
#include "PkColor.h"
#include "PkTransform.h"
#include "../render/PkGradient.h"
#include "../namespace/PkNamespace.h"
#include <optional>

class PkBrush {
public:
    PkBrush() : m_color(Pk::black), m_style(Pk::NoBrush) {}
    explicit PkBrush(Pk::GlobalColor c) : m_color(c), m_style(Pk::SolidPattern) {}
    explicit PkBrush(const PkColor &c) : m_color(c), m_style(Pk::SolidPattern) {}
    explicit PkBrush(Pk::BrushStyle s) : PkBrush() { setStyle(s); }
    explicit PkBrush(const PkGradient &gradient) : PkBrush()
    {
        switch (gradient.type()) {
        case PkGradient::LinearGradient: m_style = Pk::LinearGradientPattern; break;
        case PkGradient::RadialGradient: m_style = Pk::RadialGradientPattern; break;
        case PkGradient::ConicalGradient: m_style = Pk::ConicalGradientPattern; break;
        default: return;
        }
        m_gradient = gradient;
    }
    const PkGradient *gradient() const { return m_gradient ? &*m_gradient : nullptr; }
    PkColor color() const { return m_color; }
    void setColor(Pk::GlobalColor c) { m_color = c; }
    void setColor(const PkColor &c) { m_color = c; }
    Pk::BrushStyle style() const { return m_style; }
    void setStyle(Pk::BrushStyle s)
    {
        // Gradient styles require an actual gradient constructor, as in QBrush.
        if (s == Pk::LinearGradientPattern || s == Pk::RadialGradientPattern ||
            s == Pk::ConicalGradientPattern) return;
        m_style = s;
        m_gradient.reset();
    }
    PkTransform transform() const { return m_transform; }
    void setTransform(const PkTransform &transform) { m_transform = transform; }
    bool operator==(const PkBrush &o) const { return m_color == o.m_color && m_style == o.m_style && m_transform == o.m_transform && m_gradient == o.m_gradient; }
    bool operator!=(const PkBrush &o) const { return !(*this == o); }
private:
    PkColor m_color;
    Pk::BrushStyle m_style;
    PkTransform m_transform;
    std::optional<PkGradient> m_gradient;
};
