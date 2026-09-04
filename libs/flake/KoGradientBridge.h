#pragma once

// PkGradient ↔ QGradient（过渡期转换；S-09-g）。
// 独立成头的理由：toQGradient/toPkGradientPtr 需要 PkGradient 完整定义，
// 而 PkGradient.h 在 libs/pigment——不能塞进 PkFlakeBridge.h（跨库 include）。

#include <PkGradient.h>
#include <PkFlakeBridge.h>
#include <QGradient>
#include <QLinearGradient>
#include <QRadialGradient>
#include <QConicalGradient>

inline QGradient toQGradient(const PkGradient &g)
{
    QGradient out;
    switch (g.type()) {
    case PkGradient::LinearGradient:
        out = QLinearGradient(toQPointF(g.start()), toQPointF(g.finalStop()));
        break;
    case PkGradient::RadialGradient:
        out = QRadialGradient(toQPointF(g.center()), g.radius(), toQPointF(g.focalPoint()));
        break;
    case PkGradient::ConicalGradient:
        out = QConicalGradient(toQPointF(g.center()), g.angle());
        break;
    default:
        break;
    }
    out.setCoordinateMode(static_cast<QGradient::CoordinateMode>(g.coordinateMode()));
    out.setSpread(static_cast<QGradient::Spread>(g.spread()));
    QGradientStops stops;
    const PkGradientStops src = g.stops();
    for (const PkGradientStop &stop : src) {
        stops << qMakePair(stop.offset, toQColor(stop.color));
    }
    out.setStops(stops);
    return out;
}

// QGradient* → PkGradient*（过渡期深拷；QBrush::gradient() 的所有权在 QBrush，
// 这里深拷后由调用方管理——对应 KoMarker/KoShapeFillWrapper 的单点消费）
inline PkGradient* toPkGradientPtr(const QGradient *g)
{
    if (!g) return nullptr;
    PkGradient *out = new PkGradient();
    out->setType(static_cast<PkGradientEnums::Type>(g->type()));
    out->setSpread(static_cast<PkGradientEnums::Spread>(g->spread()));
    out->setCoordinateMode(static_cast<PkGradientEnums::CoordinateMode>(g->coordinateMode()));
    PkGradientStops stops;
    const auto qs = g->stops();
    for (const auto &stop : qs) {
        stops.append(PkGradientStop(stop.first, toPkColor(stop.second)));
    }
    switch (g->type()) {
    case QGradient::LinearGradient: {
        const QLinearGradient *lg = static_cast<const QLinearGradient*>(g);
        out->setStart(PkPointF(lg->start().x(), lg->start().y()));
        out->setFinalStop(PkPointF(lg->finalStop().x(), lg->finalStop().y()));
        break;
    }
    case QGradient::RadialGradient: {
        const QRadialGradient *rg = static_cast<const QRadialGradient*>(g);
        out->setCenter(PkPointF(rg->center().x(), rg->center().y()));
        out->setRadius(rg->radius());
        out->setFocalPoint(PkPointF(rg->focalPoint().x(), rg->focalPoint().y()));
        break;
    }
    case QGradient::ConicalGradient: {
        const QConicalGradient *cg = static_cast<const QConicalGradient*>(g);
        out->setCenter(PkPointF(cg->center().x(), cg->center().y()));
        out->setAngle(cg->angle());
        break;
    }
    default:
        break;
    }
    return out;
}
