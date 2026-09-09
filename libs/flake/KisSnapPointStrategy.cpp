/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisSnapPointStrategy.h"

#include <PkPainterPath.h>
#include <KoViewConverter.h>
#include "kis_global.h"

#include <limits>

struct KisSnapPointStrategy::Private
{
    PkList<PkPointF> points;
};

KisSnapPointStrategy::KisSnapPointStrategy(KoSnapGuide::Strategy type)
    : KoSnapStrategy(type),
      m_d(new Private)
{
}

KisSnapPointStrategy::~KisSnapPointStrategy()
{
}

bool KisSnapPointStrategy::snap(const PkPointF &mousePosition, KoSnapProxy *proxy, qreal maxSnapDistance)
{
    (void)proxy;

    PkPointF snappedPoint = mousePosition;
    qreal minDistance = std::numeric_limits<qreal>::max();

    for (const PkPointF &pt : m_d->points) {
        const qreal dist = kisDistance(mousePosition, pt);

        if (dist < maxSnapDistance && dist < minDistance) {
            minDistance = dist;
            snappedPoint = pt;
        }
    }

    setSnappedPosition(snappedPoint, ToPoint);
    return minDistance < std::numeric_limits<qreal>::max();
}

PkPainterPath KisSnapPointStrategy::decoration(const KoViewConverter &converter) const
{
    PkRectF unzoomedRect = converter.viewToDocument(PkRectF(0, 0, 11, 11));
    unzoomedRect.moveCenter(snappedPosition());
    PkPainterPath decoration;
    decoration.addEllipse(unzoomedRect);
    return decoration;
}

void KisSnapPointStrategy::addPoint(const PkPointF &pt)
{
    m_d->points << pt;
}
