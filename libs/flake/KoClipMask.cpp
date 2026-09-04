/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoClipMask.h"

#include <PkRect.h>
#include <PkTransform.h>
#include <QPainter>
#include <QSharedData>
#include <PkPainterPath.h>
#include <KoShape.h>
#include "kis_algebra_2d.h"

#include <KoShapePainter.h>

struct Q_DECL_HIDDEN KoClipMask::Private : public QSharedData
{
    Private()
        : QSharedData()
    {}

    Private(const Private &rhs)
        : QSharedData()
        , coordinates(rhs.coordinates)
        , contentCoordinates(rhs.contentCoordinates)
        , maskRect(rhs.maskRect)
        , extraShapeTransform(rhs.extraShapeTransform)
    {
        Q_FOREACH (KoShape *shape, rhs.shapes) {
            KoShape *clonedShape = shape->cloneShape();
            KIS_ASSERT_RECOVER(clonedShape) { continue; }

            shapes << clonedShape;
        }
    }

    ~Private() {
        qDeleteAll(shapes);
        shapes.clear();
    }


    KoFlake::CoordinateSystem coordinates = KoFlake::ObjectBoundingBox;
    KoFlake::CoordinateSystem contentCoordinates = KoFlake::UserSpaceOnUse;

    PkRectF maskRect = PkRectF(-0.1, -0.1, 1.2, 1.2);

    PkList<KoShape*> shapes;
    PkTransform extraShapeTransform; // TODO: not used anymore, use direct shape transform instead

};

KoClipMask::KoClipMask()
    : m_d(new Private)
{
}

KoClipMask::~KoClipMask()
{
}

KoClipMask::KoClipMask(const KoClipMask &rhs)
    : m_d(new Private(*rhs.m_d))
{
}

KoClipMask &KoClipMask::operator=(const KoClipMask &rhs)
{
    m_d = rhs.m_d;
    return *this;
}

KoClipMask *KoClipMask::clone() const
{
    return new KoClipMask(*this);
}

KoFlake::CoordinateSystem KoClipMask::coordinates() const
{
    return m_d->coordinates;
}

void KoClipMask::setCoordinates(KoFlake::CoordinateSystem value)
{
    m_d->coordinates = value;
}

KoFlake::CoordinateSystem KoClipMask::contentCoordinates() const
{
    return m_d->contentCoordinates;
}

void KoClipMask::setContentCoordinates(KoFlake::CoordinateSystem value)
{
    m_d->contentCoordinates = value;
}

PkRectF KoClipMask::maskRect() const
{
    return m_d->maskRect;
}

void KoClipMask::setMaskRect(const PkRectF &value)
{
    m_d->maskRect = value;
}

PkList<KoShape *> KoClipMask::shapes() const
{
    return m_d->shapes;
}

void KoClipMask::setShapes(const PkList<KoShape *> &value)
{
    m_d->shapes = value;
}

bool KoClipMask::isEmpty() const
{
    return m_d->shapes.isEmpty();
}

void KoClipMask::setExtraShapeOffset(const PkPointF &value)
{
    /**
     * TODO: when we implement source shapes sharing, please wrap the shapes
     *       into a group and apply this transform to the group instead
     */

    if (m_d->contentCoordinates == KoFlake::UserSpaceOnUse) {
        const PkTransform t = PkTransform::fromTranslate(value.x(), value.y());

        Q_FOREACH (KoShape *shape, m_d->shapes) {
            shape->applyAbsoluteTransformation(t);
        }
    }

    if (m_d->coordinates == KoFlake::UserSpaceOnUse) {
        m_d->maskRect.translate(value);
    }
}

void KoClipMask::drawMask(QPainter *painter, KoShape *shape)
{
    painter->save();

    PkPainterPath clipPathInShapeSpace;

    if (m_d->coordinates == KoFlake::ObjectBoundingBox) {
        PkTransform relativeToShape = toQTransform(KisAlgebra2D::mapToRect(toPkRectF(shape->outlineRect())));
        clipPathInShapeSpace.addPolygon(relativeToShape.map(m_d->maskRect));
    } else {
        clipPathInShapeSpace.addRect(m_d->maskRect);
        clipPathInShapeSpace = m_d->extraShapeTransform.map(clipPathInShapeSpace);
    }

    painter->setClipPath(clipPathInShapeSpace, Qt::IntersectClip);

    if (m_d->contentCoordinates == KoFlake::ObjectBoundingBox) {
        PkTransform relativeToShape = toQTransform(KisAlgebra2D::mapToRect(toPkRectF(shape->outlineRect())));

        painter->setTransform(relativeToShape, true);
    } else {
        painter->setTransform(m_d->extraShapeTransform, true);
    }

    KoShapePainter p;
    p.setShapes(m_d->shapes);
    p.paint(*painter);

    painter->restore();
}
