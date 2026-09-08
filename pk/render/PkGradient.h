/*
    SPDX-FileCopyrightText: 2026 S-03-a, S-09-g
    SPDX-License-Identifier: LGPL-2.1-or-later
*/
#ifndef PK_GRADIENT_H
#define PK_GRADIENT_H

#include "../global/PkGlobal.h"
#include "../color/PkColor.h"
#include "../geometry/PkPoint.h"
#include "../container/PkVector.h"

namespace PkGradientEnums {
// Preserve the existing Pk enumeration values used by pigment consumers.
enum Type { NoGradient, LinearGradient, RadialGradient, ConicalGradient };
enum Spread { PadSpread, ReflectSpread, RepeatSpread };
enum CoordinateMode { LogicalMode, ObjectBoundingMode };
}

struct PkGradientStop {
    PkGradientStop() = default;
    PkGradientStop(qreal o, const PkColor &c) : offset(o), color(c) {}
    qreal offset = 0;
    PkColor color;
    bool operator==(const PkGradientStop &o) const { return offset == o.offset && color == o.color; }
    bool operator!=(const PkGradientStop &o) const { return !(*this == o); }
};
using PkGradientStops = PkVector<PkGradientStop>;
using GradientStop = PkGradientStop;
using GradientStops = PkGradientStops;

// Native value owner. Header-only so brushes and low-level geometry consumers
// need neither pigment's generated export header nor a pigment/render library.
class PkGradient {
public:
    enum InterpolationMode { ColorInterpolation, ComponentInterpolation };
    using Type = PkGradientEnums::Type;
    using Spread = PkGradientEnums::Spread;
    using CoordinateMode = PkGradientEnums::CoordinateMode;
    static constexpr Type NoGradient = PkGradientEnums::NoGradient;
    static constexpr Type LinearGradient = PkGradientEnums::LinearGradient;
    static constexpr Type RadialGradient = PkGradientEnums::RadialGradient;
    static constexpr Type ConicalGradient = PkGradientEnums::ConicalGradient;
    static constexpr Spread PadSpread = PkGradientEnums::PadSpread;
    static constexpr Spread ReflectSpread = PkGradientEnums::ReflectSpread;
    static constexpr Spread RepeatSpread = PkGradientEnums::RepeatSpread;
    static constexpr CoordinateMode LogicalMode = PkGradientEnums::LogicalMode;
    static constexpr CoordinateMode ObjectBoundingMode = PkGradientEnums::ObjectBoundingMode;

    PkGradient() = default;
    explicit PkGradient(Type type) : m_type(type) {}
    static PkGradient linear(const PkPointF &start, const PkPointF &end)
    {
        PkGradient g(LinearGradient); g.m_start = start; g.m_finalStop = end; return g;
    }
    static PkGradient radial(const PkPointF &center, qreal radius, const PkPointF &focalPoint)
    {
        PkGradient g(RadialGradient);
        g.m_center = center; g.m_radius = radius; g.m_focalPoint = focalPoint; return g;
    }
    static PkGradient conical(const PkPointF &center, qreal angle)
    {
        PkGradient g(ConicalGradient); g.m_center = center; g.m_angle = angle; return g;
    }
    Type type() const { return m_type; }
    void setType(Type type) { m_type = type; }
    Spread spread() const { return m_spread; }
    void setSpread(Spread spread) { m_spread = spread; }
    CoordinateMode coordinateMode() const { return m_coordinateMode; }
    void setCoordinateMode(CoordinateMode mode) { m_coordinateMode = mode; }
    InterpolationMode interpolationMode() const { return m_interpolationMode; }
    void setInterpolationMode(InterpolationMode mode) { m_interpolationMode = mode; }

    // Qt exposes a black-to-white ramp when no explicit stops were assigned.
    // The first setColorAt still starts with an empty list, not that ramp.
    PkGradientStops stops() const
    {
        if (m_stops.isEmpty()) return {{0, PkColor(Pk::black)}, {1, PkColor(Pk::white)}};
        return m_stops;
    }
    void setStops(const PkGradientStops &stops)
    {
        m_stops.clear();
        for (const auto &stop : stops) setColorAt(stop.offset, stop.color);
    }
    void setColorAt(qreal pos, const PkColor &color)
    {
        if (pos < 0 || pos > 1) return;
        int index = 0;
        while (index < m_stops.size() && m_stops.at(index).offset < pos) ++index;
        if (index < m_stops.size() && m_stops.at(index).offset == pos)
            m_stops[index].color = color;
        else m_stops.insert(index, PkGradientStop(pos, color));
    }

    // Existing pigment interpolation API (not a rasterizer). Preserve its
    // empty-list black fallback and unpremultiplied channel interpolation.
    PkColor colorAt(qreal pos) const
    {
        if (m_stops.isEmpty()) return PkColor(0, 0, 0);
        if (m_stops.size() == 1) return m_stops.at(0).color;
        pos = pkBound<qreal>(0, pos, 1);
        if (pos <= m_stops.at(0).offset) return m_stops.at(0).color;
        const int last = m_stops.size() - 1;
        if (pos >= m_stops.at(last).offset) return m_stops.at(last).color;
        for (int i = 0; i < last; ++i) {
            const auto &a = m_stops.at(i);
            const auto &b = m_stops.at(i + 1);
            if (pos >= a.offset && pos <= b.offset) {
                const qreal t = b.offset == a.offset ? 0 : (pos - a.offset) / (b.offset - a.offset);
                return PkColor(pkRound(a.color.red() + (b.color.red() - a.color.red()) * t),
                               pkRound(a.color.green() + (b.color.green() - a.color.green()) * t),
                               pkRound(a.color.blue() + (b.color.blue() - a.color.blue()) * t),
                               pkRound(a.color.alpha() + (b.color.alpha() - a.color.alpha()) * t));
            }
        }
        return m_stops.at(last).color;
    }
    bool operator==(const PkGradient &o) const
    {
        if (m_type != o.m_type || m_spread != o.m_spread ||
            m_coordinateMode != o.m_coordinateMode || m_interpolationMode != o.m_interpolationMode ||
            stops() != o.stops()) return false;
        switch (m_type) {
        case LinearGradient: return m_start == o.m_start && m_finalStop == o.m_finalStop;
        case RadialGradient: return m_center == o.m_center && m_radius == o.m_radius &&
                                    m_focalPoint == o.m_focalPoint && m_focalRadius == o.m_focalRadius;
        case ConicalGradient: return m_center == o.m_center && m_angle == o.m_angle;
        default: return true;
        }
    }
    bool operator!=(const PkGradient &o) const { return !(*this == o); }
    PkPointF start() const { return m_start; }
    void setStart(const PkPointF &p) { m_start = p; }
    void setStart(qreal x, qreal y) { setStart(PkPointF(x, y)); }
    PkPointF finalStop() const { return m_finalStop; }
    void setFinalStop(const PkPointF &p) { m_finalStop = p; }
    void setFinalStop(qreal x, qreal y) { setFinalStop(PkPointF(x, y)); }
    PkPointF center() const { return m_center; }
    void setCenter(const PkPointF &p) { m_center = p; }
    void setCenter(qreal x, qreal y) { setCenter(PkPointF(x, y)); }
    qreal radius() const { return m_radius; }
    void setRadius(qreal radius) { m_radius = radius; }
    qreal centerRadius() const { return m_radius; }
    void setCenterRadius(qreal radius) { m_radius = radius; }
    qreal focalRadius() const { return m_focalRadius; }
    void setFocalRadius(qreal radius) { m_focalRadius = radius; }
    PkPointF focalPoint() const { return m_focalPoint; }
    void setFocalPoint(const PkPointF &p) { m_focalPoint = p; }
    void setFocalPoint(qreal x, qreal y) { setFocalPoint(PkPointF(x, y)); }
    qreal angle() const { return m_angle; }
    void setAngle(qreal angle) { m_angle = angle; }
private:
    Type m_type = NoGradient;
    Spread m_spread = PadSpread;
    CoordinateMode m_coordinateMode = LogicalMode;
    InterpolationMode m_interpolationMode = ColorInterpolation;
    PkGradientStops m_stops;
    PkPointF m_start, m_finalStop, m_center, m_focalPoint;
    qreal m_radius = 0;
    qreal m_focalRadius = 0;
    qreal m_angle = 0;
};
#endif
