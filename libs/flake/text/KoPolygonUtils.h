/*
 *  SPDX-FileCopyrightText: Copyright (c) 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOPOLYGONUTILS_H
#define KOPOLYGONUTILS_H

#include <PkXmlCompat.h>

#include <pk/geometry/PkPolygon.h>

/**
 * @brief The KoPolygonUtils class
 *
 * This class allows us to use the Polygon Concept from boost for PkPolygons, which
 * has a very fast polygon offset algorithm, necessary for text-padding.
 *
 * For more info see:
 * https://www.boost.org/doc/libs/1_81_0/libs/polygon/doc/gtl_polygon_set_concept.htm
 */

class KoPolygonUtils
{
public:
    /**
     * @brief offsetPolygon
     * This offsets a single polygon, using the winding/nonzero fill-rule to determine inside and outside.
     *
     * @param polygon
     * @param offset
     *  the offset.
     * @param rounded
     *  whether the edges are rounded.
     * @param circleSegments
     *  segments in a 360° circle.
     * @return the offset polygon.
     */
    static PkPolygon offsetPolygon(const PkPolygon &polygon, int offset, bool rounded = true, int circleSegments = 16);

    static PkList<PkPolygon> offsetPolygons (const PkList<PkPolygon> polygons, int offset, bool rounded = true, int circleSegments = 16);
};

#endif // KOPOLYGONUTILS_H
