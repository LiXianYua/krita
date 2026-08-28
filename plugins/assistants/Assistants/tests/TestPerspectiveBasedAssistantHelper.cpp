/*
 * SPDX-FileCopyrightText: 2022 Agata Cacko <agata.cacko@krita.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "TestPerspectiveBasedAssistantHelper.h"

#include <PerspectiveBasedAssistantHelper.h>

#include <cmath>

namespace
{

PkList<KisPaintingAssistantHandleSP> getHandles(const PkList<PkPointF> &points)
{
    PkList<KisPaintingAssistantHandleSP> handles;
    for (const PkPointF &point : points) {
        handles << KisPaintingAssistantHandleSP(new KisPaintingAssistantHandle(point));
    }
    return handles;
}

bool fuzzyCompare(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < 0.0000001;
}

}

int runPerspectiveBasedAssistantHelperTest()
{
    PkList<KisPaintingAssistantHandleSP> handles =
        getHandles({PkPointF(-4, 4), PkPointF(4, 4), PkPointF(8, 8), PkPointF(-8, 8)});
    PkList<PkPointF> pointsToCheck;
    for (int i = 0; i <= 8; ++i) pointsToCheck << PkPointF(0, i);

    PkPolygonF polygon;
    if (!PerspectiveBasedAssistantHelper::getTetragon(handles, true, polygon)) return 1;

    PerspectiveBasedAssistantHelper::CacheData cache;
    PerspectiveBasedAssistantHelper::updateCacheData(cache, polygon);
    for (int i = 0; i < pointsToCheck.size(); ++i) {
        if (!fuzzyCompare(PerspectiveBasedAssistantHelper::distanceInGrid(cache, pointsToCheck[i]),
                          i / 8.0)) return 2;
    }

    const PkLineF first(PkPointF(10, 0), PkPointF(-10, 8));
    const PkLineF second(PkPointF(-10, 0), PkPointF(0, 13));
    PkPointF intersection;
    if (first.intersects(second, &intersection) == PkLineF::NoIntersection) return 3;

    handles = getHandles({PkPointF(0, 4), intersection, PkPointF(0, 13),
                          PkPointF(-intersection.x(), intersection.y())});
    pointsToCheck.clear();
    for (int i = 0; i <= 13; ++i) pointsToCheck << PkPointF(0, i);

    if (!PerspectiveBasedAssistantHelper::getTetragon(handles, true, polygon)) return 4;
    PerspectiveBasedAssistantHelper::updateCacheData(cache, polygon);
    for (int i = 0; i < pointsToCheck.size(); ++i) {
        if (!fuzzyCompare(PerspectiveBasedAssistantHelper::distanceInGrid(cache, pointsToCheck[i]),
                          i / 13.0)) return 5;
    }
    return 0;
}

int main()
{
    return runPerspectiveBasedAssistantHelperTest();
}
