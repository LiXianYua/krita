/*
 *  SPDX-FileCopyrightText: 2025 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisSpatialContainer.h"

#include <PkList.h>
#include <PkSize.h>
#include <kis_debug.h>
#include <kis_algebra_2d.h>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/max.hpp>

#include <array>

#include <config-gsl.h>

#ifdef HAVE_GSL
#include <gsl/gsl_multimin.h>
#endif /*HAVE_GSL*/

#include <Eigen/Eigenvalues>

#define SANITY_CHECKS

#include <kis_grid_interpolation_tools.h>


struct KisSpatialContainer::SpatialNode {

public:
    struct SpatialNodeData {
        int index;
        PkPointF position;

        SpatialNodeData() {}
        SpatialNodeData(int _index, PkPointF _position)
            : index(_index)
            , position(_position)
        {}
    };

public:

    SpatialNode() {children = {}; pointsData = {};}
    ~SpatialNode() {nodeId = -100;}

    int childIndexForPoint(PkPointF position);

public:

    PkVector<SpatialNodeData> pointsData = {};
    PkVector<SpatialNode*> children = {}; // four children // the instance is owned by the parent
    qreal xPartition {0.0};
    qreal yPartition {0.0};

    int pointsCount {0};
    bool isLeaf {true};

    // this area contains all the points
    // but it isn't necessarily the smallest possible PkRectF that contains them
    PkRectF maximumArea;

    int nodeId {-1};

};



typedef KisSpatialContainer::SpatialNode::SpatialNodeData SpatialNodeData;



KisSpatialContainer::KisSpatialContainer(PkRectF startArea, int maxPointsInDict)
    : m_maxPointsInDict(maxPointsInDict)
{
    m_root = new KisSpatialContainer::SpatialNode();
    m_root->xPartition = startArea.x() + startArea.width()/2.0;
    m_root->yPartition = startArea.y() + startArea.height()/2.0;
    m_root->nodeId = m_nextNodeId++;
}

KisSpatialContainer::KisSpatialContainer(PkRectF startArea, PkVector<PkPointF> &points)
{
    m_root = new KisSpatialContainer::SpatialNode();
    m_root->xPartition = startArea.x() + startArea.width()/2.0;
    m_root->yPartition = startArea.y() + startArea.height()/2.0;
    m_root->nodeId = m_nextNodeId++;

    initializeWith(points);
}

KisSpatialContainer::KisSpatialContainer(const KisSpatialContainer &rhs)
{
    m_root = rhs.m_root;
    m_root = new SpatialNode();
    m_maxPointsInDict = rhs.m_maxPointsInDict;
    m_count = rhs.m_count;
    m_nextNodeId = rhs.m_nextNodeId;
    deepCopyData(m_root, rhs.m_root);
}

KisSpatialContainer::~KisSpatialContainer()
{
    clear();
    delete m_root;
    m_root = nullptr;
}

void KisSpatialContainer::initializeFor(int numPoints, PkRectF startArea)
{
    clear();

    int levels = 1;
    int leaves = (int)pkCeil(numPoints/(double)m_maxPointsInDict);
    int nodes = leaves;
    while (nodes > 0) {
        nodes >>= 2;
        levels++;
    }

    initializeLevels(m_root, levels, startArea);

}

void KisSpatialContainer::initializeWith(const PkVector<PkPointF> &points)
{
    clear();

    for (int i = 0; i < points.length(); i++) {
        addPoint(i, points[i]);
    }
}

void KisSpatialContainer::initializeWithGridPoints(PkRectF gridRect, int pixelPrecision)
{
    clear();
    int columnsNum = GridIterationTools::calcGridSize(gridRect.toRect(), pixelPrecision).width();
    initializeWithGridPointsRec(gridRect, pixelPrecision, m_root, 0, 0, columnsNum);
}

void KisSpatialContainer::addPoint(int index, PkPointF position)
{
    addPointRec(index, position, m_root);
}

void KisSpatialContainer::removePoint(int index, PkPointF position)
{
    removePointRec(index, position, m_root);
}

void KisSpatialContainer::movePoint(int index, PkPointF positionBefore, PkPointF positionAfter)
{
    movePointRec(index, positionBefore, positionAfter, m_root);
}

PkVector<PkPointF> KisSpatialContainer::toVector()
{
    if (m_count == 0) {
        return PkVector<PkPointF>();
    }

    PkVector<PkPointF> response;
    response.resize(m_count);
    gatherDataRec(response, m_root);
    return response;

}

void KisSpatialContainer::findAllInRange(PkVector<int> &indexes, PkPointF center, qreal range)
{
    findAllInRangeRec(indexes, center, range, m_root);
}

int KisSpatialContainer::count()
{
    return m_count;
}

PkPointF KisSpatialContainer::getTopLeft()
{
    return getBoundaryPoint(false, false);
}

PkRectF KisSpatialContainer::exactBounds()
{
    PkPointF topLeft = getTopLeft();
    PkPointF bottomRight = getBoundaryPoint(true, true);
    return PkRectF(topLeft, bottomRight);
}

void KisSpatialContainer::clear()
{
    clearRec(m_root);

    // now clean up root
    m_root->isLeaf = true;
    m_root->children = {};
    m_root->pointsCount = 0;
    m_root->pointsData = {};
}

void KisSpatialContainer::addPointRec(int index, PkPointF position, SpatialNode *node)
{
    if (!node) return;
    if (node->isLeaf) { // leaf
        SpatialNodeData childData(index, position);

        node->pointsData << childData;
        KisAlgebra2D::accumulateBounds(position, &node->maximumArea);
        m_count++;

        if (node->pointsCount == m_maxPointsInDict) {
            // gotta make it into a parent node
            node->xPartition = node->maximumArea.x() + node->maximumArea.width()/2.0;
            node->yPartition = node->maximumArea.y() + node->maximumArea.height()/2.0;
            node->children = PkVector<SpatialNode*> {nullptr, nullptr, nullptr, nullptr};
            node->isLeaf = false;


            PkVector<SpatialNodeData>::iterator it = node->pointsData.begin();
            PkVector<SpatialNodeData>::iterator end = node->pointsData.end();

            for (; it != end; ++it) {
                int childIndex = node->childIndexForPoint(it->position);
                if (!node->children[childIndex]) {
                    node->children[childIndex] = createNodeForPoint(it->index, it->position);
                } else {
                    addPointRec(it->index, it->position, node->children[childIndex]);
                }
            }
            node->pointsData.clear();
            node->pointsCount = 0;

        } else {
            node->pointsCount++;
        }
    } else { // inner node
        int childIndex = node->childIndexForPoint(position);
        if (!node->children[childIndex]) { // no node for points there yet
            SpatialNode* newChild = createNodeForPoint(index, position);
            node->children[childIndex] = newChild;
        } else {
            addPointRec(index, position, node->children[childIndex]);
        }
    }
}

void KisSpatialContainer::removePointRec(int index, PkPointF position, SpatialNode *node)
{
    if (!node) return;
    if (node->isLeaf) {

        auto sameIndex = [index] (const SpatialNodeData& item) {
            return item.index == index;
        };
        // quickest way: move the last item into the place where the found item was; keeping the order isn't necessary
        PkVector<SpatialNodeData>::iterator foundItem = std::find_if(node->pointsData.begin(), node->pointsData.end(), sameIndex);
        if (foundItem != node->pointsData.end()) {
            PkVector<SpatialNodeData>::reverse_iterator lastItem = node->pointsData.rbegin();
            *foundItem = *lastItem;
            // PkVector has no removeLast()（PkList 才有）；remove(size-1) 对末元素等价
            node->pointsData.remove(node->pointsData.size() - 1);
            node->pointsCount--;
        }

    } else {
        int childIndex = node->childIndexForPoint(position);
        SpatialNode* child = node->children[childIndex];
        if (child) {
            removePointRec(index, position, child);
            // this below isn't really necessary but keeps the tree tidy by removing no longer needed nodes
            if (child->isLeaf) {
                if (child->pointsCount == 0) {
                    // child = empty leaf
                    delete node->children[childIndex];
                    node->children[childIndex] = nullptr;
                    child = nullptr;
                }
            } else {
                bool foundExistingGrandChild = false;
                for (int i = 0; i < child->children.length(); i++) {
                    if (child->children[i]) {
                        foundExistingGrandChild = true;
                    }
                }
                if (!foundExistingGrandChild) {
                    // child = empty parent node
                    delete node->children[childIndex];
                    node->children[childIndex] = nullptr;
                    child = nullptr;
                }
            }
        }
    }
}

void KisSpatialContainer::movePointRec(int index, PkPointF positionBefore, PkPointF positionAfter, SpatialNode *node)
{
    if (!node) {
        return;
    }
    if (node->isLeaf) {
        for (int i = 0; i < node->pointsCount; i++) {
            if(node->pointsData[i].index == index) {
                node->pointsData[i].position = positionAfter;
            }
        }
        KisAlgebra2D::accumulateBounds(positionAfter, &node->maximumArea);
    } else {
        int childBefore = node->childIndexForPoint(positionBefore);
        int childAfter = node->childIndexForPoint(positionAfter);
        if (childBefore == childAfter) {
            movePointRec(index, positionBefore, positionAfter, node->children[childBefore]);
        } else {
            removePointRec(index, positionBefore, node->children[childBefore]);
            if (!node->children[childAfter]) {
                node->children[childAfter] = createNodeForPoint(index, positionAfter);
            } else {
                addPointRec(index, positionAfter, node->children[childAfter]);
            }
        }
    }
}

KisSpatialContainer::SpatialNode *KisSpatialContainer::createNodeForPoint(int index, PkPointF position)
{
    SpatialNode* newChild = new SpatialNode();
    newChild->pointsCount = 1;
    newChild->pointsData = PkVector<SpatialNodeData> {SpatialNodeData(index, position)};
    newChild->maximumArea = PkRectF(position, position);
    newChild->nodeId = m_nextNodeId++;
    return newChild;
}


bool isInRange(const PkPointF &center, qreal range, const SpatialNodeData& data) {
    if(pkAbs(data.position.x() - center.x()) <= range && pkAbs(data.position.y() - center.y()) <= range) {
        if(KisAlgebra2D::norm(data.position - center) <= range) {
            return true;
        }
    }
    return false;
}

void KisSpatialContainer::findAllInRangeRec(PkVector<int> &indexes, PkPointF center, qreal range, SpatialNode *node)
{
    if (!node) return;
    if (node->isLeaf) {
        for (int i = 0; i < node->pointsCount; i++) {
            if (isInRange(center, range, node->pointsData[i])) {
                indexes.append(node->pointsData[i].index);
            }
        }
    } else {
        PkList<int> childrenToVisit;
        childrenToVisit << node->childIndexForPoint(center + PkPointF(range, range));
        childrenToVisit << node->childIndexForPoint(center + PkPointF(range, -range));
        childrenToVisit << node->childIndexForPoint(center + PkPointF(-range, range));
        childrenToVisit << node->childIndexForPoint(center + PkPointF(-range, -range));

        for (int i = 0; i < node->children.length(); i++) {
            if (childrenToVisit.contains(i)) { // that eliminates all the leaves outside of the area
                findAllInRangeRec(indexes, center, range, node->children[i]);
            }
        }
    }
}

void KisSpatialContainer::gatherDataRec(PkVector<PkPointF> &vector, SpatialNode *node)
{
    if (!node) return;
    if (node->isLeaf) {
        for (int i = 0; i < node->pointsData.size(); i++) {
            KIS_SAFE_ASSERT_RECOVER(node->pointsData[i].index < vector.length()) { continue; };
            vector[node->pointsData[i].index] = node->pointsData[i].position;
        }
    } else {
        for (int i = 0; i < node->children.length(); i++) {
            gatherDataRec(vector, node->children[i]);
        }
    }
}

void KisSpatialContainer::debugWriteOut()
{
    debugWriteOutRec(m_root, "");
}

void KisSpatialContainer::debugWriteOutRec(SpatialNode *node, PkString prefix)
{
    if (!node) {
        return;
    }
    if (node->isLeaf) {
        qCritical() << prefix << "* (" << node->nodeId << ") is Leaf?" << node->isLeaf << ppVar(node->pointsCount) << ppVar(node->pointsData.length());
        for (int i = 0; i < node->pointsCount; i++) {
            qCritical() << prefix << " - " << node->pointsData[i].index << " | " << node->pointsData[i].position;
        }
    } else {
        qCritical() << prefix << "* (" << node->nodeId << ") is Leaf?" << node->isLeaf << ppVar(node->xPartition) << ppVar(node->yPartition);

        qCritical() << prefix << "0: ";
        debugWriteOutRec(node->children[0], prefix + "0.");
        qCritical() << prefix << "1: ";
        debugWriteOutRec(node->children[1], prefix + "1.");
        qCritical() << prefix << "2: ";
        debugWriteOutRec(node->children[2], prefix + "2.");
        qCritical() << prefix << "3: ";
        debugWriteOutRec(node->children[3], prefix + "3.");
    }
}

std::optional<qreal> KisSpatialContainer::getBoundaryOnAxis(bool positive, bool xAxis, SpatialNode *node)
{
    if (!node) {
        return std::nullopt;
    }

    if (node->isLeaf) {
        if (node->pointsData.length() == 0) {
            return std::nullopt;
        }

        auto getNum = [xAxis] (const SpatialNodeData& a, const SpatialNodeData& b) {
            if (xAxis) {
                return a.position.x() < b.position.x();
            } else {
                return a.position.y() < b.position.y();
            }
        };
        if (positive) {
            PkVector<SpatialNodeData>::iterator data = std::max_element(node->pointsData.begin(), node->pointsData.end(), getNum);
            if (data != node->pointsData.end()) {
                if (xAxis) {
                    return data->position.x();
                } else {
                    return data->position.y();
                }
            }
        } else {
            PkVector<SpatialNodeData>::iterator data = std::min_element(node->pointsData.begin(), node->pointsData.end(), getNum);

            if (data != node->pointsData.end()) {
                if (xAxis) {
                    return data->position.x();
                } else {
                    return data->position.y();
                }
            }
        }
        return std::nullopt;

    } else {
        int diff = positive? 1 : -1;
        int childIndex1, childIndex2;
        if (xAxis) {
            childIndex1 = node->childIndexForPoint(PkPointF(node->xPartition + diff, node->yPartition + 1));
            childIndex2 = node->childIndexForPoint(PkPointF(node->xPartition + diff, node->yPartition - 1));
        } else {
            childIndex1 = node->childIndexForPoint(PkPointF(node->xPartition + 1, node->yPartition + diff));
            childIndex2 = node->childIndexForPoint(PkPointF(node->xPartition - 1, node->yPartition + diff));
        }
        std::optional<qreal> response1 = getBoundaryOnAxis(positive, xAxis, node->children[childIndex1]);
        std::optional<qreal> response2 = getBoundaryOnAxis(positive, xAxis, node->children[childIndex2]);

        if (!response1.has_value() && !response2.has_value()) {
            // gotta check the other two
            diff = -diff;
            if (xAxis) {
                childIndex1 = node->childIndexForPoint(PkPointF(node->xPartition + diff, node->yPartition + 1));
                childIndex2 = node->childIndexForPoint(PkPointF(node->xPartition + diff, node->yPartition - 1));
            } else {
                childIndex1 = node->childIndexForPoint(PkPointF(node->xPartition + 1, node->yPartition + diff));
                childIndex2 = node->childIndexForPoint(PkPointF(node->xPartition - 1, node->yPartition + diff));
            }
            response1 = getBoundaryOnAxis(positive, xAxis, node->children[childIndex1]);
            response2 = getBoundaryOnAxis(positive, xAxis, node->children[childIndex2]);
        }


        if (!response1.has_value()) {
            return response2;
        }
        if (!response2.has_value()) {
            return response1;
        }

        if (positive) {
            return pkMax(response1.value(), response2.value());
        } else {
            return pkMin(response1.value(), response2.value());
        }
    }
}

PkPointF KisSpatialContainer::getBoundaryPoint(bool left, bool top)
{
    std::optional<qreal> x = getBoundaryOnAxis(left, true, m_root);
    std::optional<qreal> y = getBoundaryOnAxis(top, false, m_root);

    PkPointF response = PkPointF();
    if (x.has_value()) {
        response.setX(x.value());
    }
    if (y.has_value()) {
        response.setY(y.value());
    }

    return response;
}

void KisSpatialContainer::initializeLevels(SpatialNode *node, int levelsLeft, PkRectF area)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(node);

    if (levelsLeft > 1) {

        node->xPartition = area.x() + area.width();
        node->yPartition = area.y() + area.height();
        PkPointF center = PkPointF(node->xPartition, node->yPartition);
        node->isLeaf = false;
        node->children = {nullptr, nullptr, nullptr, nullptr};
        for (int i = 0; i < node->children.length(); i++) {
            node->children[i] = new SpatialNode();
            node->children[i]->nodeId = m_nextNodeId++;
        }

        int topLeftChild = node->childIndexForPoint(center + PkPointF(-1, -1));
        initializeLevels(node->children[topLeftChild], levelsLeft - 1, PkRectF(area.topLeft(), center));

        int topRightChild = node->childIndexForPoint(center + PkPointF(1, -1));
        initializeLevels(node->children[topRightChild], levelsLeft - 1, PkRectF(PkPointF(node->xPartition, area.top()), PkPointF(area.right(), node->yPartition)));

        int bottomLeftChild = node->childIndexForPoint(center + PkPointF(-1, 1));
        initializeLevels(node->children[bottomLeftChild], levelsLeft - 1, PkRectF(PkPointF(area.left(), node->yPartition), PkPointF(node->xPartition, area.bottom())));

        int bottomRightChild = node->childIndexForPoint(center + PkPointF(1, 1));
        initializeLevels(node->children[bottomRightChild], levelsLeft - 1, PkRectF(PkPointF(node->xPartition, node->yPartition), PkPointF(area.right(), area.bottom())));

    } else {
        node->isLeaf = true;
        node->children = {};
    }


}

void KisSpatialContainer::initializeWithGridPointsRec(PkRectF gridRect, int pixelPrecision, SpatialNode *node, int startRow, int startColumn, int columnCount)
{
    node->maximumArea = gridRect;

    PkSize size = GridIterationTools::calcGridSize(gridRect.toRect(), pixelPrecision);
    int width = size.width();
    int height = size.height();

    int marginX = int(gridRect.left())%pixelPrecision;
    marginX = marginX == 0 ? 0 : pixelPrecision - marginX;
    int marginY = int(gridRect.top())%pixelPrecision;
    marginY = marginY == 0 ? 0 : pixelPrecision - marginY;

    // ceiling will always be on the corner or outside
    // floor will always be on the corner or inside
    PkPoint topLeftDividableCeiling = PkPoint((pkCeil(gridRect.left()/(qreal)pixelPrecision))*pixelPrecision, (pkCeil(gridRect.top()/(qreal)pixelPrecision))*pixelPrecision);
    PkPoint topLeftDividableFloor = PkPoint((pkFloor(gridRect.left()/(qreal)pixelPrecision))*pixelPrecision, (pkFloor(gridRect.top()/(qreal)pixelPrecision))*pixelPrecision);


    if (width * height <= m_maxPointsInDict) {
        node->isLeaf = true;
        node->pointsData.reserve(width * height);

        for (int i = 0; i < width; i++) {
            for (int j = 0; j < height; j++) {

                int pointIndex = (startRow + j)*columnCount + (startColumn + i);

                PkPointF nextPoint = PkPointF(topLeftDividableFloor.x() + i * pixelPrecision, topLeftDividableFloor.y() + j * pixelPrecision);
                if (i == 0) {
                    nextPoint.setX(gridRect.x());
                }
                if (j == 0) {
                    nextPoint.setY(gridRect.y());
                }

                // must be -1, because it's actually last included pixels and gridRect here is PkRectF, not PkRect
                if (nextPoint.x() > gridRect.right() - 1) {
                    nextPoint.setX(gridRect.right() - 1);
                }
                if (nextPoint.y() > gridRect.bottom() - 1) {
                    nextPoint.setY(gridRect.bottom() - 1);
                }

                node->pointsData << SpatialNodeData(pointIndex, nextPoint);
            }
        }
        node->pointsCount = node->pointsData.count();
        m_count += node->pointsCount;
        node->maximumArea = gridRect;

    } else {

        node->isLeaf = false;

        PkPoint bottomRightDividable = PkPoint((pkFloor(gridRect.right()/(qreal)pixelPrecision))*pixelPrecision, (pkFloor(gridRect.bottom()/(qreal)pixelPrecision))*pixelPrecision);
        PkPoint topLeftDividable = topLeftDividableCeiling;

        PkPoint sizeDividable = PkPoint(bottomRightDividable - topLeftDividable);
        int xLeftDividable = (sizeDividable.x()) / pixelPrecision / 2;
        int yTopDividable = (sizeDividable.y()) / pixelPrecision / 2;

        qreal xLeftCenter = topLeftDividable.x() + xLeftDividable*pixelPrecision;
        qreal yTopCenter = topLeftDividable.y() + yTopDividable*pixelPrecision;

        qreal xRightCenter = topLeftDividable.x() + (xLeftDividable + 1)*pixelPrecision;
        qreal yBottomCenter = topLeftDividable.y() + (yTopDividable + 1)*pixelPrecision;


        int xLeft = width/2;
        int yTop = height/2;

        if (bottomRightDividable.x() <= topLeftDividable.x()) {
            // shouldn't really happen for Liquify, but let's have sane values
            xLeftCenter = gridRect.left() + xLeft*pixelPrecision;
            xRightCenter = gridRect.left() + (xLeft + 1)*pixelPrecision;
        }

        if (bottomRightDividable.y() <= topLeftDividable.y()) {
            yTopCenter = gridRect.top() + yTop*pixelPrecision;
            yBottomCenter = gridRect.top() + (yTop + 1)*pixelPrecision;
        }


        node->xPartition = (xLeftCenter + xRightCenter)/2;
        node->yPartition = (yTopCenter + yBottomCenter)/2;

        node->children = {nullptr, nullptr, nullptr, nullptr};
        for (int i = 0; i < node->children.count(); i++) {
            node->children[i] = new SpatialNode();
            node->children[i]->nodeId = m_nextNodeId++;
        }

        PkPointF center = PkPointF(node->xPartition, node->yPartition);

        int topLeftChild = node->childIndexForPoint(center + PkPointF(-1, -1));

        initializeWithGridPointsRec(PkRectF(gridRect.topLeft(), PkPointF(xLeftCenter + 1, yTopCenter + 1)), pixelPrecision, node->children[topLeftChild], startRow, startColumn, columnCount);

        int rightStartColumn = startColumn + xLeftDividable + 1 + (topLeftDividableCeiling.x() > gridRect.left() ? 1 : 0);
        int bottomStartRow = startRow + yTopDividable + 1 + (topLeftDividableCeiling.y() > gridRect.top() ? 1 : 0);

        int topRightChild = node->childIndexForPoint(center + PkPointF(1, -1));
        initializeWithGridPointsRec(PkRectF(PkPointF(xRightCenter, gridRect.top()), PkPointF(gridRect.right(), yTopCenter + 1)), pixelPrecision,
                                    node->children[topRightChild], startRow, rightStartColumn, columnCount);

        int bottomLeftChild = node->childIndexForPoint(center + PkPointF(-1, 1));
        initializeWithGridPointsRec(PkRectF(PkPointF(gridRect.left(), yBottomCenter), PkPointF(xLeftCenter + 1, gridRect.bottom())), pixelPrecision,
                                    node->children[bottomLeftChild], bottomStartRow, startColumn, columnCount);

        int bottomRightChild = node->childIndexForPoint(center + PkPointF(1, 1));
        initializeWithGridPointsRec(PkRectF(PkPointF(xRightCenter, yBottomCenter), PkPointF(gridRect.right(), gridRect.bottom())), pixelPrecision,
                                    node->children[bottomRightChild], bottomStartRow, rightStartColumn, columnCount);

    }

}

void KisSpatialContainer::clearRec(SpatialNode *node)
{
    if (!node) return;
    bool hadChildren = false;
    for (int i = 0; i < node->children.length(); i++) {
        if (node->children[i]) {
            hadChildren = true;

            clearRec(node->children[i]);

            delete node->children[i];
            node->children[i] = nullptr;
        }
    }
    KIS_SAFE_ASSERT_RECOVER_RETURN(!node->isLeaf || !hadChildren);
}

void KisSpatialContainer::deepCopyData(SpatialNode *node, const SpatialNode *from)
{
    node->isLeaf = from->isLeaf;
    node->maximumArea = from->maximumArea;
    node->nodeId = from->nodeId;
    node->pointsCount = from->pointsCount;
    node->pointsData = from->pointsData;
    node->xPartition = from->xPartition;
    node->yPartition = from->yPartition;

    if (!from->isLeaf) {
        node->children = {nullptr, nullptr, nullptr, nullptr};
        for (int i = 0; i < node->children.size(); i++) {
            if (from->children[i]) {
                node->children[i] = new SpatialNode();
                deepCopyData(node->children[i], from->children[i]);
            }
        }
    }
}

int KisSpatialContainer::SpatialNode::childIndexForPoint(PkPointF position)
{
    if (position.x() > xPartition) {
        if (position.y() > yPartition) {
            return 0;
        }
        return 1;
    } else {
        if (position.y() > yPartition) {
            return 2;
        }
        return 3;
    }
}

