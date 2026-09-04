/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoGradientBackground.h"
#include "KoFlake.h"
#include <KoXmlNS.h>
#include <KoShapeSavingContext.h>

#include <FlakeDebug.h>

#include <QBrush>
#include <PkGradient.h>
#include <PkFlakeBridge.h>
#include <QPainter>
#include <QSharedData>
#include <PkPainterPath.h>

class KoGradientBackground::Private : public QSharedData
{
public:
    Private()
        : QSharedData()
        , gradient(0)
    {}

    PkGradient *gradient;
    PkTransform matrix;
};

namespace {
QGradient toQGradient(const PkGradient &g)
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
}

KoGradientBackground::KoGradientBackground(PkGradient * gradient, const PkTransform &matrix)
    : KoShapeBackground()
    , d(new Private)
{
    d->gradient = gradient;
    d->matrix = matrix;
    Q_ASSERT(d->gradient);
}

KoGradientBackground::KoGradientBackground(const PkGradient & gradient, const PkTransform &matrix)
    : KoShapeBackground()
    , d(new Private)
{
    d->gradient = KoFlake::cloneGradient(&gradient);
    d->matrix = matrix;
    Q_ASSERT(d->gradient);
}

KoGradientBackground::~KoGradientBackground()
{
    delete d->gradient;
}

KoGradientBackground::KoGradientBackground(const KoGradientBackground &rhs)
    : d(new Private(*rhs.d))
{
}

KoGradientBackground &KoGradientBackground::operator=(const KoGradientBackground &rhs)
{
    d = rhs.d;
    return *this;
}

bool KoGradientBackground::compareTo(const KoShapeBackground *other) const
{
    const KoGradientBackground *otherGradient = dynamic_cast<const KoGradientBackground*>(other);

    return otherGradient &&
        d->matrix == otherGradient->d->matrix &&
        *d->gradient == *otherGradient->d->gradient;
}

void KoGradientBackground::setTransform(const PkTransform &matrix)
{
    d->matrix = matrix;
}

PkTransform KoGradientBackground::transform() const
{
    return d->matrix;
}

void KoGradientBackground::setGradient(const PkGradient &gradient)
{
    delete d->gradient;

    d->gradient = KoFlake::cloneGradient(&gradient);
    Q_ASSERT(d->gradient);
}

const PkGradient * KoGradientBackground::gradient() const
{
    return d->gradient;
}

void KoGradientBackground::paint(QPainter &painter, const PkPainterPath &fillPath) const
{
    if (!d->gradient) return;

    if (d->gradient->coordinateMode() == PkGradientEnums::ObjectBoundingMode) {

        /**
         * NOTE: important hack!
         *
         * Qt has different notation of QBrush::setTransform() in comparison
         * to what SVG defines. SVG defines gradientToUser matrix to be postmultiplied
         * by QBrush::transform(), but Qt does exactly reverse!
         *
         * That most probably has been caused by the fact that Qt uses transposed
         * matrices and someone just mistyped the stuff long ago :(
         *
         * So here we basically emulate this feature by converting the gradient into
         * PkGradientEnums::LogicalMode and doing transformations manually.
         */

        const PkRectF boundingRect = fillPath.boundingRect();
        PkTransform gradientToUser(boundingRect.width(), 0, 0, boundingRect.height(),
                                  boundingRect.x(), boundingRect.y());

        // TODO: how about slicing the object?
        PkGradient g = *d->gradient;
        g.setCoordinateMode(PkGradientEnums::LogicalMode);

        QBrush b(toQGradient(g));
        b.setTransform(toQTransform(d->matrix * gradientToUser));
        painter.setBrush(b);
    } else {
        QBrush b(toQGradient(*d->gradient));
        b.setTransform(toQTransform(d->matrix));
        painter.setBrush(b);
    }

    painter.drawPath(toQPainterPath(fillPath));
}
