/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoVectorPatternBackground.h"

#include <PkTransform.h>
#include <KoShape.h>
#include <KoShapePainter.h>
#include <KoBakedShapeRenderer.h>

class KoVectorPatternBackground::Private : public QSharedData
{
public:
    Private()
        : QSharedData()
    {
    }

    ~Private()
    {
        qDeleteAll(shapes);
        shapes.clear();
    }

    PkList<KoShape*> shapes;
    KoFlake::CoordinateSystem referenceCoordinates =
            KoFlake::ObjectBoundingBox;
    KoFlake::CoordinateSystem contentCoordinates =
            KoFlake::UserSpaceOnUse;
    PkRectF referenceRect;
    PkTransform patternTransform;
};

KoVectorPatternBackground::KoVectorPatternBackground()
    : KoShapeBackground()
    , d(new Private)
{
}

KoVectorPatternBackground::~KoVectorPatternBackground()
{

}

bool KoVectorPatternBackground::compareTo(const KoShapeBackground *other) const
{
    Q_UNUSED(other);
    return false;
}

void KoVectorPatternBackground::setReferenceCoordinates(KoFlake::CoordinateSystem value)
{
    d->referenceCoordinates = value;
}

KoFlake::CoordinateSystem KoVectorPatternBackground::referenceCoordinates() const
{
    return d->referenceCoordinates;
}

void KoVectorPatternBackground::setContentCoordinates(KoFlake::CoordinateSystem value)
{
    d->contentCoordinates = value;
}

KoFlake::CoordinateSystem KoVectorPatternBackground::contentCoordinates() const
{
    return d->contentCoordinates;
}

void KoVectorPatternBackground::setReferenceRect(const PkRectF &value)
{
    d->referenceRect = value;
}

PkRectF KoVectorPatternBackground::referenceRect() const
{
    return d->referenceRect;
}

void KoVectorPatternBackground::setPatternTransform(const PkTransform &value)
{
    d->patternTransform = value;
}

PkTransform KoVectorPatternBackground::patternTransform() const
{
    return d->patternTransform;
}

void KoVectorPatternBackground::setShapes(const PkList<KoShape*> value)
{
    qDeleteAll(d->shapes);
    d->shapes.clear();

    d->shapes = value;
}

PkList<KoShape *> KoVectorPatternBackground::shapes() const
{
    return d->shapes;
}

void KoVectorPatternBackground::paint(QPainter &painter, const PkPainterPath &fillPath) const
{
    const PkPainterPath dstShapeOutline = fillPath;
    const PkRectF dstShapeBoundingBox = dstShapeOutline.boundingRect();

    KoBakedShapeRenderer renderer(dstShapeOutline, PkTransform(),
                                  PkTransform(),
                                  d->referenceRect,
                                  d->contentCoordinates != KoFlake::UserSpaceOnUse,
                                  dstShapeBoundingBox,
                                  d->referenceCoordinates != KoFlake::UserSpaceOnUse,
                                  d->patternTransform);

    QPainter *patchPainter = renderer.bakeShapePainter();

    KoShapePainter p;
    p.setShapes(d->shapes);
    p.paint(*patchPainter);

    // uncomment for debug
    // renderer.patchImage().save("dd_patch_image.png");

    painter.setPen(Qt::NoPen);
    renderer.renderShape(painter);
}

bool KoVectorPatternBackground::hasTransparency() const
{
    return true;
}
