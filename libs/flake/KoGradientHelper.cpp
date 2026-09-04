/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoGradientHelper.h"

#include <PkGradient.h>
#include <math.h>
// [migrate] missing include for Pk/Qt type
#include <QGradient>

PkGradient* KoGradientHelper::defaultGradient(PkGradient::Type type, PkGradient::Spread spread, const PkGradientStops &stops)
{
    PkGradient *gradient = 0;
    switch (type) {
    case PkGradient::LinearGradient:
        gradient = new QLinearGradient(PkPointF(0.0, 0.5), PkPointF(1, 0.5));
        break;
    case PkGradient::RadialGradient:
        gradient = new QRadialGradient(PkPointF(0.5, 0.5), sqrt(0.5));
        break;
    case PkGradient::ConicalGradient:
        gradient = new QConicalGradient(PkPointF(0.5, 0.5), 0.0);
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
        const QLinearGradient *g = static_cast<const QLinearGradient*>(gradient);
        start = g->start();
        stop = g->finalStop();
        break;
    }
    case PkGradient::RadialGradient: {
        const QRadialGradient *g = static_cast<const QRadialGradient*>(gradient);
        start = g->center();
        stop = PkPointF(g->radius(), 0.0);
        break;
    }
    case PkGradient::ConicalGradient: {
        const QConicalGradient *g = static_cast<const QConicalGradient*>(gradient);
        start = g->center();
        qreal radAngle = g->angle() * M_PI / 180.0;
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
        newGradient = new QLinearGradient(start, stop);
        break;
    case PkGradient::RadialGradient: {
        PkPointF diff(stop - start);
        qreal radius = sqrt(diff.x()*diff.x() + diff.y()*diff.y());
        newGradient = new QRadialGradient(start, radius, start);
        break;
    }
    case PkGradient::ConicalGradient: {
        PkPointF diff(stop - start);
        qreal angle = atan2(diff.y(), diff.x());
        if (angle < 0.0)
            angle += 2 * M_PI;
        newGradient = new QConicalGradient(start, angle * 180/M_PI);
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
        return stops.first().second;

    PkGradientStop prevStop(-1.0, PkColor());
    PkGradientStop nextStop(2.0, PkColor());
    // find framing gradient stops
    Q_FOREACH (const PkGradientStop & stop, stops) {
        if (stop.first > prevStop.first && stop.first < position)
            prevStop = stop;
        if (stop.first < nextStop.first && stop.first > position)
            nextStop = stop;
    }

    PkColor theColor;

    if (prevStop.first < 0.0) {
        // new stop is before the first stop
        theColor = nextStop.second;
    } else if (nextStop.first > 1.0) {
        // new stop is after the last stop
        theColor = prevStop.second;
    } else {
        // linear interpolate colors between framing stops
        PkColor prevColor = prevStop.second, nextColor = nextStop.second;
        qreal colorScale = (position - prevStop.first) / (nextStop.first - prevStop.first);
        theColor.setRedF(prevColor.redF() + colorScale *(nextColor.redF() - prevColor.redF()));
        theColor.setGreenF(prevColor.greenF() + colorScale *(nextColor.greenF() - prevColor.greenF()));
        theColor.setBlueF(prevColor.blueF() + colorScale *(nextColor.blueF() - prevColor.blueF()));
        theColor.setAlphaF(prevColor.alphaF() + colorScale *(nextColor.alphaF() - prevColor.alphaF()));
    }
    return theColor;
}
