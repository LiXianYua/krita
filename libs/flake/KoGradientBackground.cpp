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

#include <PkBrush.h>
#include <PkGradient.h>
#include <PkFlakeBridge.h>
#include <PkPainter.h>
#include <PkPainterPath.h>

class KoGradientBackground::Private
{
public:
    Private()
        : gradient(0)
    {}

    Private(const Private &other)
        : gradient(other.gradient ? KoFlake::cloneGradient(other.gradient) : nullptr)
        , matrix(other.matrix)
    {}

    ~Private() { delete gradient; }

    PkGradient *gradient;
    PkTransform matrix;
};

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
    PkGradient *replacement = KoFlake::cloneGradient(&gradient);
    delete d->gradient;
    d->gradient = replacement;
    Q_ASSERT(d->gradient);
}

const PkGradient * KoGradientBackground::gradient() const
{
    return d->gradient;
}

void KoGradientBackground::paint(PkPainter &painter, const PkPainterPath &fillPath) const
{
    if (!d->gradient) return;

    if (d->gradient->coordinateMode() == PkGradientEnums::ObjectBoundingMode) {

        /**
         * NOTE: important hack!
         *
         * Qt has different notation of PkBrush::setTransform() in comparison
         * to what SVG defines. SVG defines gradientToUser matrix to be postmultiplied
         * by PkBrush::transform(), but Qt does exactly reverse!
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

        PkBrush b(g);
        b.setTransform(d->matrix * gradientToUser);
        painter.setBrush(b);
    } else {
        PkBrush b(*d->gradient);
        b.setTransform(d->matrix);
        painter.setBrush(b);
    }

    painter.drawPath(fillPath);
}
