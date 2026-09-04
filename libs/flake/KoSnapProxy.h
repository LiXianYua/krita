/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008-2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KOSNAPPROXY_H
#define KOSNAPPROXY_H


class KoSnapGuide;
class KoShape;
class KoPathSegment;
class KoCanvasBase;
#include <PkList.h>
#include "kritaflake_export.h"

class PkPointF;
class PkRectF;
/**
 * This class provides access to different shape related snap targets to snap strategies.
 */
class KRITAFLAKE_EXPORT KoSnapProxy
{
public:
    KoSnapProxy(KoSnapGuide *snapGuide);

    /// returns list of points in given rectangle in document coordinates
    PkList<PkPointF> pointsInRect(const PkRectF &rect, bool omitEditedShape);

    /// returns list of shape in given rectangle in document coordinates
    PkList<KoShape*> shapesInRect(const PkRectF &rect, bool omitEditedShape = false);

    /// returns list of points from given shape
    PkList<PkPointF> pointsFromShape(KoShape *shape);

    /// returns list of points in given rectangle in document coordinates
    PkList<KoPathSegment> segmentsInRect(const PkRectF &rect, bool omitEditedShape);

    /// returns list of all shapes
    PkList<KoShape*> shapes(bool omitEditedShape = false);

    /// returns canvas we are working on
    KoCanvasBase *canvas();

private:
    KoSnapGuide *m_snapGuide;
};

#endif
