/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoColorBackground.h"
#include <PkFlakeBridge.h>
#include "KoShapeSavingContext.h"
#include <KoXmlNS.h>

#include <PkColor.h>
#include <PkPainter.h>

class KoColorBackground::Private
{
public:
    Private()
        : color(Pk::black)
        , style(Pk::SolidPattern)
        {}

    PkColor color;
    Pk::BrushStyle style;
};

KoColorBackground::KoColorBackground()
    : KoShapeBackground()
    , d(new Private)
{
}

KoColorBackground::KoColorBackground(const PkColor &color, Pk::BrushStyle style)
    : KoShapeBackground()
    , d(new Private)
{
    if (style < Pk::SolidPattern || style >= Pk::LinearGradientPattern) {
        style = Pk::SolidPattern;
    }

    d->style = style;
    d->color = color;
}

KoColorBackground::~KoColorBackground()
{
}

KoColorBackground::KoColorBackground(const KoColorBackground &rhs)
    : d(new Private(*rhs.d))
{
}

KoColorBackground &KoColorBackground::operator=(const KoColorBackground &rhs)
{
    d = rhs.d;
    return *this;
}

bool KoColorBackground::compareTo(const KoShapeBackground *other) const
{
    const KoColorBackground *bg = dynamic_cast<const KoColorBackground*>(other);
    return bg && bg->color() == d->color;
}

PkColor KoColorBackground::color() const
{
    return d->color;
}

void KoColorBackground::setColor(const PkColor &color)
{
    d->color = color;
}

Pk::BrushStyle KoColorBackground::style() const
{
    return d->style;
}

PkBrush KoColorBackground::brush() const
{
    PkBrush brush(d->color);
    brush.setStyle(d->style);
    return brush;
}

void KoColorBackground::paint(PkPainter &painter, const PkPainterPath &fillPath) const
{
    painter.setBrush(brush());
    painter.drawPath(fillPath);
}
