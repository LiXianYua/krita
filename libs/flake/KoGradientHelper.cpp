/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoGradientHelper.h"

#include <PkGradient.h>
#include <math.h>

// S-09-g：PkGradient 是值类型（S-03-a，非多态），QGradient 的多态 clone/cast
// 模式全部换成 PkGradient 的静态工厂 + 非虚接口。

PkGradient* KoGradientHelper::defaultGradient(PkGradient::Type type, PkGradient::Spread spread, const PkGradientStops &stops)
{
    PkGradient *gradient = 0;
    switch (type) {
    case PkGradient::LinearGradient:
        gradient = new PkGradient(PkGradient::linear(PkPointF(0.0, 0.5), PkPointF(1, 0.5)));
        break;
    case PkGradient::RadialGradient:
        gradient = new PkGradient(PkGradient::radial(PkPointF(0.5, 0.5), sqrt(0.5), PkPointF(0.5, 0.5)));
        break;
    case PkGradient::ConicalGradient:
        gradient = new PkGradient(PkGradient::conical(PkPointF(0.5, 0.5), 0.0));
        break;
    default:
        return 0;
    }
    gradient->setCoordinateMode(PkGradient::ObjectBoundingMode);
    gradient->setSpread(spread);
    gradient->setStops(stops);

    return gradient;
}

PkGradient* KoGradientHelper::convertGradient(const PkGradient * gradient, PkGradient::Type newType)
{
    PkPointF start, stop;
    // try to preserve gradient positions
    switch (gradient->type()) {
    case PkGradient::LinearGradient: {
        start = gradient->start();
        stop = gradient->finalStop();
        break;
    }
    case PkGradient::RadialGradient: {
        start = gradient->center();
        stop = PkPointF(gradient->radius(), 0.0);
        break;
    }
    case PkGradient::ConicalGradient: {
        start = gradient->center();
        qreal radAngle = gradient->angle() * M_PI / 180.0;
        stop = PkPointF(0.5 * cos(radAngle), 0.5 * sin(radAngle));
        break;
    }
    default:
        start = PkPointF(0.0, 0.0);
        stop = PkPointF(0.5, 0.5);
    }

    PkGradient *newGradient = 0;
    switch (newType) {
    case PkGradient::LinearGradient:
        newGradient = new PkGradient(PkGradient::linear(start, stop));
        break;
    case PkGradient::RadialGradient: {
        PkPointF diff(stop - start);
        qreal radius = sqrt(diff.x()*diff.x() + diff.y()*diff.y());
        newGradient = new PkGradient(PkGradient::radial(start, radius, start));
        break;
    }
    case PkGradient::ConicalGradient: {
        PkPointF diff(stop - start);
        qreal angle = atan2(diff.y(), diff.x());
        if (angle < 0.0)
            angle += 2 * M_PI;
        newGradient = new PkGradient(PkGradient::conical(start, angle * 180/M_PI));
        break;
    }
    default:
        return 0;
    }
    newGradient->setCoordinateMode(PkGradient::ObjectBoundingMode);
    newGradient->setSpread(gradient->spread());
    newGradient->setStops(gradient->stops());

    return newGradient;
}

PkColor KoGradientHelper::colorAt(qreal position, const PkGradientStops &stops)
{
    if (! stops.count())
        return PkColor();

    if (stops.count() == 1)
        return stops.first().color;

    PkGradientStop prevStop(-1.0, PkColor());
    PkGradientStop nextStop(2.0, PkColor());
    // find framing gradient stops
    Q_FOREACH (const PkGradientStop & stop, stops) {
        if (stop.offset > prevStop.offset && stop.offset < position)
            prevStop = stop;
        if (stop.offset < nextStop.offset && stop.offset > position)
            nextStop = stop;
    }

    PkColor theColor;

    if (prevStop.offset < 0.0) {
        // new stop is before the first stop
        theColor = nextStop.color;
    } else if (nextStop.offset > 1.0) {
        // new stop is after the last stop
        theColor = prevStop.color;
    } else {
        // linear interpolate colors between framing stops
        PkColor prevColor = prevStop.color, nextColor = nextStop.color;
        qreal colorScale = (position - prevStop.offset) / (nextStop.offset - prevStop.offset);
        theColor.setRgbF(prevColor.redF() + colorScale * (nextColor.redF() - prevColor.redF()),
                         prevColor.greenF() + colorScale * (nextColor.greenF() - prevColor.greenF()),
                         prevColor.blueF() + colorScale * (nextColor.blueF() - prevColor.blueF()),
                         prevColor.alphaF() + colorScale * (nextColor.alphaF() - prevColor.alphaF()));
    }
    return theColor;
}
