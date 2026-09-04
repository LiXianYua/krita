/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QDebug>
#include <PkFlakeBridge.h>
#include "KoViewTransformStillPoint.h"

KoViewTransformStillPoint::KoViewTransformStillPoint(const PkPointF &docPoint, const PkPointF &viewPoint)
    : std::pair<PkPointF, PkPointF>(docPoint, viewPoint)
{
}

KoViewTransformStillPoint::KoViewTransformStillPoint(const std::pair<PkPointF, PkPointF> &rhs)
    : std::pair<PkPointF, PkPointF>(rhs)
{
}

PkPointF KoViewTransformStillPoint::docPoint() const {
    return first;
}

PkPointF KoViewTransformStillPoint::viewPoint() const {
    return second;
}

QDebug operator<<(QDebug dbg, const KoViewTransformStillPoint &point)
{
    dbg.nospace() << "KoViewTransformStillPoint(docPoint: " << point.docPoint() << ", viewPoint: " << point.viewPoint() << ")";
    return dbg.space();
}