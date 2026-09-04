/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoColorBackground.h"
#include "KoShapeSavingContext.h"
#include <KoXmlNS.h>

#include <PkColor.h>
#include <QPainter>

class KoColorBackground::Private : public QSharedData
{
public:
    Private()
        : QSharedData()
        , color(Pk::black)
        , style(Qt::SolidPattern)
        {}

    PkColor color;
    Qt::BrushStyle style;
};

KoColorBackground::KoColorBackground()
    : KoShapeBackground()
    , d(new Private)
{
}

KoColorBackground::KoColorBackground(const PkColor &color, Qt::BrushStyle style)
    : KoShapeBackground()
    , d(new Private)
{
    if (style < Qt::SolidPattern || style >= Qt::LinearGradientPattern) {
        style = Qt::SolidPattern;
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

Qt::BrushStyle KoColorBackground::style() const
{
    return d->style;
}

QBrush KoColorBackground::brush() const
{
    return QBrush(d->color, d->style);
}

void KoColorBackground::paint(QPainter &painter, const PkPainterPath &fillPath) const
{
    painter.setBrush(brush());
    painter.drawPath(fillPath);
}

