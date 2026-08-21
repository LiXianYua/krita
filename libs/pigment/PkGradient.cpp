/*
    SPDX-FileCopyrightText: 2026 S-03-a
    SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "PkGradient.h"

PkGradient::PkGradient()
    : m_type(PkGradientEnums::NoGradient)
    , m_spread(PkGradientEnums::PadSpread)
    , m_coordinateMode(PkGradientEnums::LogicalMode)
    , m_start(0.0, 0.0)
    , m_finalStop(0.0, 0.0)
    , m_center(0.0, 0.0)
    , m_focalPoint(0.0, 0.0)
    , m_radius(0.0)
    , m_angle(0.0)
{
}

PkGradient::PkGradient(PkGradientEnums::Type type)
    : PkGradient()
{
    m_type = type;
}

PkGradient PkGradient::linear(const PkPointF &start, const PkPointF &finalStop)
{
    PkGradient g(PkGradientEnums::LinearGradient);
    g.m_start = start;
    g.m_finalStop = finalStop;
    return g;
}

PkGradient PkGradient::radial(const PkPointF &center, qreal radius, const PkPointF &focalPoint)
{
    PkGradient g(PkGradientEnums::RadialGradient);
    g.m_center = center;
    g.m_radius = radius;
    g.m_focalPoint = focalPoint;
    return g;
}

PkGradient PkGradient::conical(const PkPointF &center, qreal angle)
{
    PkGradient g(PkGradientEnums::ConicalGradient);
    g.m_center = center;
    g.m_angle = angle;
    return g;
}

PkGradientEnums::Type PkGradient::type() const
{
    return m_type;
}

void PkGradient::setType(PkGradientEnums::Type type)
{
    m_type = type;
}

PkGradientEnums::Spread PkGradient::spread() const
{
    return m_spread;
}

void PkGradient::setSpread(PkGradientEnums::Spread spread)
{
    m_spread = spread;
}

PkGradientEnums::CoordinateMode PkGradient::coordinateMode() const
{
    return m_coordinateMode;
}

void PkGradient::setCoordinateMode(PkGradientEnums::CoordinateMode mode)
{
    m_coordinateMode = mode;
}

PkGradientStops PkGradient::stops() const
{
    return m_stops;
}

void PkGradient::setStops(const PkGradientStops &stops)
{
    m_stops = stops;
}

void PkGradient::setColorAt(qreal pos, const PkColor &color)
{
    // 对齐 Qt 5.15 setColorAt 语义（实现见 Qt 源码 qbrush.cpp:1669-1684）：
    //   · pos 越界（<0 或 >1）忽略；
    //   · 升序扫描找到第一个 offset >= pos 的插入位 index；
    //   · 该位已有 offset == pos 的 stop 则原地替换颜色；
    //   · 否则在 index 插入（保持 stops 升序；重复 pos 天然折叠成单个 stop）。
    // 消费方确实会产生重复 pos：KoSegmentGradient::toQGradient 在每个段边界调用
    // 两次（段 i 末 offset == 段 i+1 首 offset）；KoStopGradient 的 SVG 载入会把
    // off 夹回上一 stop 位置。Qt 对重复 pos 是「后写颜色替换先写」，此处一致。
    if (pos < 0.0 || pos > 1.0) {
        return;
    }

    int index = 0;
    while (index < m_stops.size() && m_stops.at(index).offset < pos) {
        ++index;
    }

    if (index < m_stops.size() && m_stops.at(index).offset == pos) {
        m_stops[index].color = color;
    } else {
        m_stops.insert(index, PkGradientStop{pos, color});
    }
}

PkColor PkGradient::colorAt(qreal pos) const
{
    if (m_stops.isEmpty()) {
        return PkColor(0, 0, 0);
    }
    if (m_stops.size() == 1) {
        return m_stops.at(0).color;
    }

    pos = qBound<qreal>(0.0, pos, 1.0);

    if (pos <= m_stops.at(0).offset) {
        return m_stops.at(0).color;
    }
    const int last = m_stops.size() - 1;
    if (pos >= m_stops.at(last).offset) {
        return m_stops.at(last).color;
    }

    for (int i = 0; i + 1 < m_stops.size(); ++i) {
        const qreal o0 = m_stops.at(i).offset;
        const qreal o1 = m_stops.at(i + 1).offset;
        if (pos >= o0 && pos <= o1) {
            const qreal t = (o1 == o0) ? 0.0 : (pos - o0) / (o1 - o0);
            const PkColor &c0 = m_stops.at(i).color;
            const PkColor &c1 = m_stops.at(i + 1).color;
            return PkColor(qRound(c0.red() + (c1.red() - c0.red()) * t),
                           qRound(c0.green() + (c1.green() - c0.green()) * t),
                           qRound(c0.blue() + (c1.blue() - c0.blue()) * t),
                           qRound(c0.alpha() + (c1.alpha() - c0.alpha()) * t));
        }
    }
    return m_stops.at(last).color;
}

PkPointF PkGradient::start() const
{
    return m_start;
}

void PkGradient::setStart(const PkPointF &p)
{
    m_start = p;
}

void PkGradient::setStart(qreal x, qreal y)
{
    m_start = PkPointF(x, y);
}

PkPointF PkGradient::finalStop() const
{
    return m_finalStop;
}

void PkGradient::setFinalStop(const PkPointF &p)
{
    m_finalStop = p;
}

void PkGradient::setFinalStop(qreal x, qreal y)
{
    m_finalStop = PkPointF(x, y);
}

PkPointF PkGradient::center() const
{
    return m_center;
}

void PkGradient::setCenter(const PkPointF &p)
{
    m_center = p;
}

void PkGradient::setCenter(qreal x, qreal y)
{
    m_center = PkPointF(x, y);
}

qreal PkGradient::radius() const
{
    return m_radius;
}

void PkGradient::setRadius(qreal radius)
{
    m_radius = radius;
}

PkPointF PkGradient::focalPoint() const
{
    return m_focalPoint;
}

void PkGradient::setFocalPoint(const PkPointF &p)
{
    m_focalPoint = p;
}

void PkGradient::setFocalPoint(qreal x, qreal y)
{
    m_focalPoint = PkPointF(x, y);
}

qreal PkGradient::angle() const
{
    return m_angle;
}

void PkGradient::setAngle(qreal angle)
{
    m_angle = angle;
}
