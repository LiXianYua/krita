/*
 *  SPDX-FileCopyrightText: 2025 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SPATIAL_CONTAINER_H
#define __KIS_SPATIAL_CONTAINER_H

#include <PkPoint.h>
#include <PkVector.h>
#include <PkRect.h>
#include <PkString.h>
#include <cmath>
#include <kis_global.h>
#include <kritaimage_export.h>
#include <boost/optional.hpp>
#include <optional>


class KRITAIMAGE_EXPORT KisSpatialContainer
{
    // right now it's just a standard Quadtree, since it seemed like it would work well
    // for points that are mostly uniformly located
    // and need a decent amount of changes and need to have fast queries


public:
    struct SpatialNode;

public:


    KisSpatialContainer(PkRectF startArea, int maxPointsInDict = 100);
    KisSpatialContainer(PkRectF startArea, PkVector<PkPointF> &points);
    KisSpatialContainer(const KisSpatialContainer &rhs);

    ~KisSpatialContainer();

    void initializeFor(int numPoints, PkRectF startArea);
    void initializeWith(const PkVector<PkPointF> &points);

    void initializeWithGridPoints(PkRectF gridRect, int pixelPrecision);

    void addPoint(int index, PkPointF position);
    void removePoint(int index, PkPointF position);
    void movePoint(int index, PkPointF positionBefore, PkPointF positionAfter);
    PkVector<PkPointF> toVector();

    void findAllInRange(PkVector<int> &indexes, PkPointF center, qreal range);

    int count();
    // O(log(n)*m_maxPointsInDict)
    PkPointF getTopLeft();
    // O(log(n)*m_maxPointsInDict)
    PkRectF exactBounds();

    // erase everything
    void clear();


    void debugWriteOut();


private:

    void addPointRec(int index, PkPointF position, SpatialNode* node);
    void removePointRec(int index, PkPointF position, SpatialNode* node);
    void movePointRec(int index, PkPointF positionBefore, PkPointF positionAfter, SpatialNode* node);
    SpatialNode *createNodeForPoint(int index, PkPointF position);


    void findAllInRangeRec(PkVector<int> &indexes, PkPointF center, qreal range, SpatialNode* node);


    void gatherDataRec(PkVector<PkPointF> &vector, SpatialNode* node);

    void debugWriteOutRec(SpatialNode* node, PkString prefix);

    std::optional<qreal> getBoundaryOnAxis(bool positive, bool xAxis, SpatialNode* node);
    PkPointF getBoundaryPoint(bool left, bool top);

    void initializeLevels(SpatialNode* node, int levelsLeft, PkRectF area);
    void initializeWithGridPointsRec(PkRectF gridRect, int pixelPrecision, SpatialNode* node, int startRow, int startColumn, int columnCount);

    void clearRec(SpatialNode* node);

    void deepCopyData(SpatialNode* node, const SpatialNode* from);



private:

    int m_count {0};
    int m_maxPointsInDict = {100};
    int m_nextNodeId = {0}; // just for debug
    SpatialNode* m_root {nullptr};

    friend class KisSpatialContainerTest;


};





#endif /* __KIS_SPATIAL_CONTAINER_H */
