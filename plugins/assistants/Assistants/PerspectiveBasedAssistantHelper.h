/*
 * SPDX-FileCopyrightText: 2022 Agata Cacko <cacko.azh@gmail.com>
 */

#ifndef _PERSPECTIVE_BASED_ASSISTANT_HELPER_H_
#define _PERSPECTIVE_BASED_ASSISTANT_HELPER_H_


#include <boost/optional.hpp>

#include "Ellipse.h"
#include "kis_painting_assistant.h"

#include "kritaassistanttool_export.h"

class KRITAASSISTANTTOOL_EXPORT PerspectiveBasedAssistantHelper
{
private:
    PerspectiveBasedAssistantHelper();
    ~PerspectiveBasedAssistantHelper();


public:

    class CacheData
    {
    public:
        // vanishing points that one gets from getVanishingPoints
        boost::optional<PkPointF> vanishingPoint1 {boost::none};
        boost::optional<PkPointF> vanishingPoint2 {boost::none};

        // distances from the horizon line to point 1, 2, 3 and 4 on the final polygon
        PkVector<qreal> distancesFromPoints;
        qreal maxDistanceFromPoint {0.0};

        PkLineF horizon;

        // final polygon
        PkPolygonF polygon;


        typedef enum PerspectiveType {
            None,
            OneVp,
            TwoVps
        } PerspectiveType;


        PerspectiveType type {None};

    };



    // *** main functions ***

    // creates the convex hull, returns false if it's not a quadrilateral/tetragon
    static bool getTetragon(const PkList<KisPaintingAssistantHandleSP> &handles, bool isAssistantComplete, PkPolygonF& outPolygon);

    // creates a fully connected tetragon (as in, every vertex is connected to every other vertex)
    // this is useful for drawing a wrong state in perspective-based assistants (when one vertex is inside the triangle created by the rest of them)
    static PkPolygonF getAllConnectedTetragon(const PkList<KisPaintingAssistantHandleSP>& handles);

    // distance in Perspective grid
    // used for calculating the Perspective sensor
    static qreal distanceInGrid(const PkList<KisPaintingAssistantHandleSP>& handles, bool isAssistantComplete, const PkPointF &point);

    // distance in Perspective grid
    // used for calculating the Perspective sensor
    static qreal distanceInGrid(const CacheData& cache, const PkPointF &point);

    static void updateCacheData(CacheData& cache, const PkPolygonF& poly);

    // vp1 - vp for lines 0-1 and 2-3
    // vp2 - vp for lines 1-2 and 3-0
    static bool getVanishingPointsOptional(const PkPolygonF &poly, boost::optional<PkPointF>& vp1, boost::optional<PkPointF>& vp2);



    static qreal localScale(const PkTransform& transform, PkPointF pt);

    // returns the reciprocal of the maximum local scale at the points (0,0),(0,1),(1,0),(1,1)
    static qreal inverseMaxLocalScale(const PkTransform& transform);


    // *** small helper functions ***

    // perpendicular dot product
    // it's basically a dot product between vector A(xa, ya) and vector B'(xb, -yb)
    // or a dot product between vector A'(xa, -ya) and vector B(xb, yb)
    // if it's needed elsewhere in Krita too, you can move it to KisAlgebra2D
    static qreal pdot(const PkPointF& a, const PkPointF& b);

};

#endif
