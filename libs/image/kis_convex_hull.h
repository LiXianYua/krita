#ifndef __KIS_CONVEX_HULL_H
#define __KIS_CONVEX_HULL_H

#include <PkPolygon.h>

#include "kis_types.h"

namespace KisConvexHull
{
    KRITAIMAGE_EXPORT PkPolygon findConvexHull(const PkVector<PkPoint> &points);
    KRITAIMAGE_EXPORT PkPolygon findConvexHull(KisPaintDeviceSP device);
    KRITAIMAGE_EXPORT PkPolygon findConvexHullSelectionLike(KisPaintDeviceSP device);
}

#endif /* __KIS_CONVEX_HULL_H */
