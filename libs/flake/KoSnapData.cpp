/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoSnapData.h"

#include <PkPoint.h>

KoSnapData::KoSnapData()
{
}

KoSnapData::~KoSnapData()
{
}

PkList<PkPointF> KoSnapData::snapPoints() const
{
    return m_points;
}

void KoSnapData::setSnapPoints(const PkList<PkPointF> &snapPoints)
{
    m_points = snapPoints;
}

PkList<KoPathSegment> KoSnapData::snapSegments() const
{
    return m_segments;
}

void KoSnapData::setSnapSegments(const PkList<KoPathSegment> &snapSegments)
{
    m_segments = snapSegments;
}
