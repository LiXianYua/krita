/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoSnapStrategy.h"
#include "KoSnapProxy.h"
#include "KoSnapGuide.h"
#include <KoPathShape.h>
#include <KoPathPoint.h>
#include <KoPathSegment.h>
#include <KoCanvasBase.h>
#include <KoViewConverter.h>

#include <PkPainterPath.h>

#include <cassert>
#include <cmath>

#if defined(_MSC_VER) && (_MSC_VER < 1800)
#define isfinite(x) (double)(x)
#endif

KoSnapStrategy::KoSnapStrategy(KoSnapGuide::Strategy type)
    : m_snapStrategyType(type)
{
}

PkPointF KoSnapStrategy::snappedPosition() const
{
    return m_snappedPosition;
}

KoSnapStrategy::SnapType KoSnapStrategy::snappedType() const
{
    return m_snappedType;
}

void KoSnapStrategy::setSnappedPosition(const PkPointF &position, SnapType snapType)
{
    m_snappedPosition = position;
    m_snappedType = snapType;
}

KoSnapGuide::Strategy KoSnapStrategy::type() const
{
    return m_snapStrategyType;
}

qreal KoSnapStrategy::squareDistance(const PkPointF &p1, const PkPointF &p2)
{
    const qreal dx = p1.x() - p2.x();
    const qreal dy = p1.y() - p2.y();

    return dx*dx + dy*dy;
}

qreal KoSnapStrategy::scalarProduct(const PkPointF &p1, const PkPointF &p2)
{
    return p1.x() * p2.x() + p1.y() * p2.y();
}

OrthogonalSnapStrategy::OrthogonalSnapStrategy()
    : KoSnapStrategy(KoSnapGuide::OrthogonalSnapping)
{
}

bool OrthogonalSnapStrategy::snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance)
{
    assert(std::isfinite(maxSnapDistance));
    PkPointF horzSnap, vertSnap;
    qreal minVertDist = HUGE_VAL;
    qreal minHorzDist = HUGE_VAL;

    PkList<KoShape*> shapes = proxy->shapes(true);
    for (KoShape *shape : shapes) {
        PkList<PkPointF> points = proxy->pointsFromShape(shape);
        for (const PkPointF &point : points) {
            qreal dx = fabs(point.x() - mousePosition.x());
            if (dx < minHorzDist && dx < maxSnapDistance) {
                minHorzDist = dx;
                horzSnap = point;
            }
            qreal dy = fabs(point.y() - mousePosition.y());
            if (dy < minVertDist && dy < maxSnapDistance) {
                minVertDist = dy;
                vertSnap = point;
            }
        }
    }

    PkPointF snappedPoint = mousePosition;
    SnapType snappedType = ToPoint;

    if (minHorzDist < HUGE_VAL)
        snappedPoint.setX(horzSnap.x());
    if (minVertDist < HUGE_VAL)
        snappedPoint.setY(vertSnap.y());

    if (minHorzDist < HUGE_VAL) {
        m_hLine = PkLineF(horzSnap, snappedPoint);
    } else {
        m_hLine = PkLineF();
        snappedType = ToLine;
    }

    if (minVertDist < HUGE_VAL) {
        m_vLine = PkLineF(vertSnap, snappedPoint);
    } else {
        m_vLine = PkLineF();
        snappedType = ToLine;
    }

    setSnappedPosition(snappedPoint, snappedType);

    return (minHorzDist < HUGE_VAL || minVertDist < HUGE_VAL);
}

PkPainterPath OrthogonalSnapStrategy::decoration(const KoViewConverter &/*converter*/) const
{
    PkPainterPath decoration;
    if (! m_hLine.isNull()) {
        decoration.moveTo(m_hLine.p1());
        decoration.lineTo(m_hLine.p2());
    }
    if (! m_vLine.isNull()) {
        decoration.moveTo(m_vLine.p1());
        decoration.lineTo(m_vLine.p2());
    }
    return decoration;
}

NodeSnapStrategy::NodeSnapStrategy()
    : KoSnapStrategy(KoSnapGuide::NodeSnapping)
{
}

bool NodeSnapStrategy::snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance)
{
    assert(std::isfinite(maxSnapDistance));
    const qreal maxDistance = maxSnapDistance * maxSnapDistance;
    qreal minDistance = HUGE_VAL;

    PkRectF rect(-maxSnapDistance, -maxSnapDistance, maxSnapDistance, maxSnapDistance);
    rect.moveCenter(mousePosition);
    PkList<PkPointF> points = proxy->pointsInRect(rect, false);
    PkPointF snappedPoint = mousePosition;

    for (const PkPointF &point : points) {
        qreal distance = squareDistance(mousePosition, point);
        if (distance < maxDistance && distance < minDistance) {
            snappedPoint = point;
            minDistance = distance;
        }
    }

    setSnappedPosition(snappedPoint, ToPoint);

    return (minDistance < HUGE_VAL);
}

PkPainterPath NodeSnapStrategy::decoration(const KoViewConverter &converter) const
{
    PkRectF unzoomedRect = converter.viewToDocument(PkRectF(0, 0, 11, 11));
    unzoomedRect.moveCenter(snappedPosition());
    PkPainterPath decoration;
    decoration.addEllipse(unzoomedRect);
    return decoration;
}

ExtensionSnapStrategy::ExtensionSnapStrategy()
    : KoSnapStrategy(KoSnapGuide::ExtensionSnapping)
{
}

bool ExtensionSnapStrategy::snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance)
{
    assert(std::isfinite(maxSnapDistance));

    const qreal maxDistance = maxSnapDistance * maxSnapDistance;
    qreal minDistances[2] = { HUGE_VAL, HUGE_VAL };

    PkPointF snappedPoints[2] = { mousePosition, mousePosition };
    PkPointF startPoints[2];

    PkList<KoShape*> shapes = proxy->shapes(true);

    for (KoShape *shape : shapes) {
        KoPathShape * path = dynamic_cast<KoPathShape*>(shape);
        if (! path) {
            continue;
        }
        PkTransform matrix = path->absoluteTransformation();

        const int subpathCount = path->subpathCount();
        for (int subpathIndex = 0; subpathIndex < subpathCount; ++subpathIndex) {
            if (path->isClosedSubpath(subpathIndex))
                continue;

            int pointCount = path->subpathPointCount(subpathIndex);

            // check the extension from the start point
            KoPathPoint * first = path->pointByIndex(KoPathPointIndex(subpathIndex, 0));
            PkPointF firstSnapPosition = mousePosition;
            if (snapToExtension(firstSnapPosition, first, matrix)) {
                qreal distance = squareDistance(firstSnapPosition, mousePosition);
                if (distance < maxDistance) {
                    if (distance < minDistances[0]) {
                        minDistances[1] = minDistances[0];
                        snappedPoints[1] = snappedPoints[0];
                        startPoints[1] = startPoints[0];

                        minDistances[0] = distance;
                        snappedPoints[0] = firstSnapPosition;
                        startPoints[0] = matrix.map(first->point());
                    }
                    else if (distance < minDistances[1]) {
                        minDistances[1] = distance;
                        snappedPoints[1] = firstSnapPosition;
                        startPoints[1] = matrix.map(first->point());
                    }
                }
            }

            // now check the extension from the last point
            KoPathPoint * last = path->pointByIndex(KoPathPointIndex(subpathIndex, pointCount - 1));
            PkPointF lastSnapPosition = mousePosition;
            if (snapToExtension(lastSnapPosition, last, matrix)) {
                qreal distance = squareDistance(lastSnapPosition, mousePosition);
                if (distance < maxDistance) {
                    if (distance < minDistances[0]) {
                        minDistances[1] = minDistances[0];
                        snappedPoints[1] = snappedPoints[0];
                        startPoints[1] = startPoints[0];

                        minDistances[0] = distance;
                        snappedPoints[0] = lastSnapPosition;
                        startPoints[0] = matrix.map(last->point());
                    }
                    else if (distance < minDistances[1]) {
                        minDistances[1] = distance;
                        snappedPoints[1] = lastSnapPosition;
                        startPoints[1] = matrix.map(last->point());
                    }
                }
            }
        }
    }

    m_lines.clear();
    // if we have to extension near our mouse position, they might have an intersection
    // near our mouse position which we want to use as the snapped position
    if (minDistances[0] < HUGE_VAL && minDistances[1] < HUGE_VAL) {
        // check if intersection of extension lines is near mouse position
        KoPathSegment s1(startPoints[0], snappedPoints[0] + snappedPoints[0]-startPoints[0]);
        KoPathSegment s2(startPoints[1], snappedPoints[1] + snappedPoints[1]-startPoints[1]);
        PkList<PkPointF> isects = s1.intersections(s2);
        if (isects.count() == 1 && squareDistance(isects[0], mousePosition) < maxDistance) {
            // add both extension lines
            m_lines.append(PkLineF(startPoints[0], isects[0]));
            m_lines.append(PkLineF(startPoints[1], isects[0]));
            setSnappedPosition(isects[0], ToLine);
        }
        else {
            // only add nearest extension line of both
            uint index = minDistances[0] < minDistances[1] ? 0 : 1;
            m_lines.append(PkLineF(startPoints[index], snappedPoints[index]));
            setSnappedPosition(snappedPoints[index], ToLine);
        }
    }
    else  if (minDistances[0] < HUGE_VAL) {
        m_lines.append(PkLineF(startPoints[0], snappedPoints[0]));
        setSnappedPosition(snappedPoints[0], ToLine);
    }
    else if (minDistances[1] < HUGE_VAL) {
        m_lines.append(PkLineF(startPoints[1], snappedPoints[1]));
        setSnappedPosition(snappedPoints[1], ToLine);
    }
    else {
        // none of the extension lines is near our mouse position
        return false;
    }
    return true;
}

PkPainterPath ExtensionSnapStrategy::decoration(const KoViewConverter &/*converter*/) const
{
    PkPainterPath decoration;
    for (const PkLineF &line : m_lines) {
        decoration.moveTo(line.p1());
        decoration.lineTo(line.p2());
    }
    return decoration;
}

bool ExtensionSnapStrategy::snapToExtension(PkPointF &position, KoPathPoint * point, const PkTransform &matrix)
{
    assert(point);
    PkPointF direction = extensionDirection(point, matrix);
    if (direction.isNull())
        return false;

    PkPointF extensionStart = matrix.map(point->point());
    PkPointF extensionStop = matrix.map(point->point()) + direction;
    float posOnExtension = project(extensionStart, extensionStop, position);
    if (posOnExtension < 0.0)
        return false;

    position = extensionStart + posOnExtension * direction;
    return true;
}

qreal ExtensionSnapStrategy::project(const PkPointF &lineStart, const PkPointF &lineEnd, const PkPointF &point)
{
    // This is how the returned value should be used to get the
    // projectionPoint: ProjectionPoint = lineStart(1-resultingReal) + resultingReal*lineEnd;

    PkPointF diff = lineEnd - lineStart;
    PkPointF relPoint = point - lineStart;
    qreal diffLength = sqrt(diff.x() * diff.x() + diff.y() * diff.y());
    if (diffLength == 0.0)
        return 0.0;

    diff /= diffLength;
    // project mouse position relative to stop position on extension line
    qreal scalar = relPoint.x() * diff.x() + relPoint.y() * diff.y();
    return scalar /= diffLength;
}

PkPointF ExtensionSnapStrategy::extensionDirection(KoPathPoint * point, const PkTransform &matrix)
{
    assert(point);

    KoPathShape * path = point->parent();
    KoPathPointIndex index = path->pathPointIndex(point);

    // check if it is a start point
    if (point->properties() & KoPathPoint::StartSubpath) {
        if (point->activeControlPoint2()) {
            return matrix.map(point->point()) - matrix.map(point->controlPoint2());
        } else {
            KoPathPoint * next = path->pointByIndex(KoPathPointIndex(index.first, index.second + 1));
            if (! next){
                return PkPointF();
            }
            else if (next->activeControlPoint1()) {
                return matrix.map(point->point()) - matrix.map(next->controlPoint1());
            }
            else {
                return matrix.map(point->point()) - matrix.map(next->point());
            }
        }
    }
    else {
        if (point->activeControlPoint1()) {
            return matrix.map(point->point()) - matrix.map(point->controlPoint1());
        }
        else {
            KoPathPoint * prev = path->pointByIndex(KoPathPointIndex(index.first, index.second - 1));
            if (! prev){
                return PkPointF();
            }
            else if (prev->activeControlPoint2()) {
                return matrix.map(point->point()) - matrix.map(prev->controlPoint2());
            }
            else {
                return matrix.map(point->point()) - matrix.map(prev->point());
            }
        }
    }
}

IntersectionSnapStrategy::IntersectionSnapStrategy()
    : KoSnapStrategy(KoSnapGuide::IntersectionSnapping)
{
}

bool IntersectionSnapStrategy::snap(const PkPointF &mousePosition, KoSnapProxy *proxy, qreal maxSnapDistance)
{
    assert(std::isfinite(maxSnapDistance));
    const qreal maxDistance = maxSnapDistance * maxSnapDistance;
    qreal minDistance = HUGE_VAL;

    PkRectF rect(-maxSnapDistance, -maxSnapDistance, maxSnapDistance, maxSnapDistance);
    rect.moveCenter(mousePosition);
    PkPointF snappedPoint = mousePosition;

    PkList<KoPathSegment> segments = proxy->segmentsInRect(rect, false);
    int segmentCount = segments.count();
    for (int i = 0; i < segmentCount; ++i) {
        const KoPathSegment &s1 = segments[i];
        for (int j = i + 1; j < segmentCount; ++j) {
            PkList<PkPointF> isects = s1.intersections(segments[j]);
            for (const PkPointF &point : isects) {
                if (! rect.contains(point))
                    continue;
                qreal distance = squareDistance(mousePosition, point);
                if (distance < maxDistance && distance < minDistance) {
                    snappedPoint = point;
                    minDistance = distance;
                }
            }
        }
    }

    setSnappedPosition(snappedPoint, ToPoint);

    return (minDistance < HUGE_VAL);
}

PkPainterPath IntersectionSnapStrategy::decoration(const KoViewConverter &converter) const
{
    PkRectF unzoomedRect = converter.viewToDocument(PkRectF(0, 0, 11, 11));
    unzoomedRect.moveCenter(snappedPosition());
    PkPainterPath decoration;
    decoration.addRect(unzoomedRect);
    return decoration;
}

GridSnapStrategy::GridSnapStrategy()
    : KoSnapStrategy(KoSnapGuide::GridSnapping)
{
}

bool GridSnapStrategy::snap(const PkPointF &mousePosition, KoSnapProxy *proxy, qreal maxSnapDistance)
{
    assert(std::isfinite(maxSnapDistance));
    if (! proxy->canvas()->snapToGrid())
        return false;

    // The 1e-10 here is a workaround for some weird division problem.
    // 360.00062366 / 2.83465058 gives 127 'exactly' when shown as a qreal,
    // but when casting into an int, we get 126. In fact it's 127 - 5.64e-15 !
    PkPointF offset;
    PkSizeF spacing;
    proxy->canvas()->gridSize(&offset, &spacing);

    // we want to snap to the nearest grid point, so calculate
    // the grid rows/columns before and after the points position
    int col = static_cast<int>((mousePosition.x() - offset.x()) / spacing.width() + 1e-10);
    int nextCol = col + 1;
    int row = static_cast<int>((mousePosition.y() - offset.y()) / spacing.height() + 1e-10);
    int nextRow = row + 1;

    // now check which grid line has less distance to the point
    qreal distToCol = std::abs(offset.x() + col * spacing.width() - mousePosition.x());
    qreal distToNextCol = std::abs(offset.x() + nextCol * spacing.width() - mousePosition.x());

    if (distToCol > distToNextCol) {
        col = nextCol;
        distToCol = distToNextCol;
    }

    qreal distToRow = std::abs(offset.y() + row * spacing.height() - mousePosition.y());
    qreal distToNextRow = std::abs(offset.y() + nextRow * spacing.height() - mousePosition.y());
    if (distToRow > distToNextRow) {
        row = nextRow;
        distToRow = distToNextRow;
    }

    PkPointF snappedPoint = mousePosition;
    SnapType snapType = ToPoint;

    bool pointIsSnapped = false;

    const qreal sqDistance = distToCol * distToCol + distToRow * distToRow;
    const qreal maxSqDistance = maxSnapDistance * maxSnapDistance;
    // now check if we are inside the snap distance
    if (sqDistance < maxSqDistance) {
        snappedPoint = PkPointF(offset.x() + col * spacing.width(), offset.y() + row * spacing.height());
        pointIsSnapped = true;
    } else if (distToRow < maxSnapDistance) {
        snappedPoint.ry() = offset.y() + row * spacing.height();
        snapType = ToLine;
        pointIsSnapped = true;
    } else if (distToCol < maxSnapDistance) {
        snappedPoint.rx() = offset.x() + col * spacing.width();
        snapType = ToLine;
        pointIsSnapped = true;
    }

    setSnappedPosition(snappedPoint, snapType);

    return pointIsSnapped;
}

PkPainterPath GridSnapStrategy::decoration(const KoViewConverter &converter) const
{
    PkSizeF unzoomedSize = converter.viewToDocument(PkSizeF(5, 5));
    PkPainterPath decoration;
    decoration.moveTo(snappedPosition() - PkPointF(unzoomedSize.width(), 0));
    decoration.lineTo(snappedPosition() + PkPointF(unzoomedSize.width(), 0));
    decoration.moveTo(snappedPosition() - PkPointF(0, unzoomedSize.height()));
    decoration.lineTo(snappedPosition() + PkPointF(0, unzoomedSize.height()));
    return decoration;
}

BoundingBoxSnapStrategy::BoundingBoxSnapStrategy()
    : KoSnapStrategy(KoSnapGuide::BoundingBoxSnapping)
{
}

bool BoundingBoxSnapStrategy::snap(const PkPointF &mousePosition, KoSnapProxy *proxy, qreal maxSnapDistance)
{
    assert(std::isfinite(maxSnapDistance));
    const qreal maxDistance = maxSnapDistance * maxSnapDistance;
    qreal minDistance = HUGE_VAL;

    PkRectF rect(-maxSnapDistance, -maxSnapDistance, maxSnapDistance, maxSnapDistance);

    rect.moveCenter(mousePosition);
    PkPointF snappedPoint = mousePosition;
    SnapType snapType = ToPoint;

    KoFlake::AnchorPosition pointId[5] = {
        KoFlake::TopLeft,
        KoFlake::TopRight,
        KoFlake::BottomRight,
        KoFlake::BottomLeft,
        KoFlake::Center
    };

    PkList<KoShape*> shapes = proxy->shapesInRect(rect, true);
    for (KoShape *shape : shapes) {
        qreal shapeMinDistance = HUGE_VAL;
        // first check the corner and center points
        for (int i = 0; i < 5; ++i) {
            m_boxPoints[i] = shape->absolutePosition(pointId[i]);
            qreal d = squareDistance(mousePosition, m_boxPoints[i]);
            if (d < minDistance && d < maxDistance) {
                shapeMinDistance = d;
                minDistance = d;
                snappedPoint = m_boxPoints[i];
                snapType = ToPoint;
            }
        }
        // prioritize points over edges
        if (shapeMinDistance < maxDistance)
            continue;

        // now check distances to edges of bounding box
        for (int i = 0; i < 4; ++i) {
            PkPointF pointOnLine;
            qreal d = squareDistanceToLine(m_boxPoints[i], m_boxPoints[(i+1)%4], mousePosition, pointOnLine);
            if (d < minDistance && d < maxDistance) {
                minDistance = d;
                snappedPoint = pointOnLine;
                snapType = ToLine;
            }
        }
    }
    setSnappedPosition(snappedPoint, snapType);

    return (minDistance < maxDistance);
}

qreal BoundingBoxSnapStrategy::squareDistanceToLine(const PkPointF &lineA, const PkPointF &lineB, const PkPointF &point, PkPointF &pointOnLine)
{
    PkPointF diff = lineB - lineA;
    if(lineA == lineB)
        return HUGE_VAL;
    const qreal diffLength = sqrt(diff.x() * diff.x() + diff.y() * diff.y());

    // project mouse position relative to start position on line
    const qreal scalar = KoSnapStrategy::scalarProduct(point - lineA, diff / diffLength);

    if (scalar < 0.0 || scalar > diffLength)
        return HUGE_VAL;
    // calculate vector between relative mouse position and projected mouse position
    pointOnLine = lineA + scalar / diffLength * diff;
    PkPointF distVec = pointOnLine - point;
    return distVec.x()*distVec.x() + distVec.y()*distVec.y();
}

PkPainterPath BoundingBoxSnapStrategy::decoration(const KoViewConverter &converter) const
{
    PkSizeF unzoomedSize = converter.viewToDocument(PkSizeF(5, 5));

    PkPainterPath decoration;
    decoration.moveTo(snappedPosition() - PkPointF(unzoomedSize.width(), unzoomedSize.height()));
    decoration.lineTo(snappedPosition() + PkPointF(unzoomedSize.width(), unzoomedSize.height()));
    decoration.moveTo(snappedPosition() - PkPointF(unzoomedSize.width(), -unzoomedSize.height()));
    decoration.lineTo(snappedPosition() + PkPointF(unzoomedSize.width(), -unzoomedSize.height()));

    return decoration;
}

// KoGuidesData has been moved into Krita. Please port this class!

// LineGuideSnapStrategy::LineGuideSnapStrategy()
//     : KoSnapStrategy(KoSnapGuide::GuideLineSnapping)
// {
// }

// bool LineGuideSnapStrategy::snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance)
// {
//     Q_ASSERT(std::isfinite(maxSnapDistance));

//     KoGuidesData * guidesData = proxy->canvas()->guidesData();

//     if (!guidesData || !guidesData->showGuideLines())
//         return false;

//     PkPointF snappedPoint = mousePosition;
//     m_orientation = 0;

//     qreal minHorzDistance = maxSnapDistance;
//     Q_FOREACH (qreal guidePos, guidesData->horizontalGuideLines()) {
//         qreal distance = qAbs(guidePos - mousePosition.y());
//         if (distance < minHorzDistance) {
//             snappedPoint.setY(guidePos);
//             minHorzDistance = distance;
//             m_orientation |= Qt::Horizontal;
//         }
//     }
//     qreal minVertSnapDistance = maxSnapDistance;
//     Q_FOREACH (qreal guidePos, guidesData->verticalGuideLines()) {
//         qreal distance = qAbs(guidePos - mousePosition.x());
//         if (distance < minVertSnapDistance) {
//             snappedPoint.setX(guidePos);
//             minVertSnapDistance = distance;
//             m_orientation |= Qt::Vertical;
//         }
//     }
//     setSnappedPosition(snappedPoint);
//     return (minHorzDistance < maxSnapDistance || minVertSnapDistance < maxSnapDistance);
// }

// PkPainterPath LineGuideSnapStrategy::decoration(const KoViewConverter &converter) const
// {
//     PkSizeF unzoomedSize = converter.viewToDocument(PkSizeF(5, 5));
//     Q_ASSERT(unzoomedSize.isValid());

//     PkPainterPath decoration;
//     if (m_orientation & Qt::Horizontal) {
//         decoration.moveTo(snappedPosition() - PkPointF(unzoomedSize.width(), 0));
//         decoration.lineTo(snappedPosition() + PkPointF(unzoomedSize.width(), 0));
//     }
//     if (m_orientation & Qt::Vertical) {
//         decoration.moveTo(snappedPosition() - PkPointF(0, unzoomedSize.height()));
//         decoration.lineTo(snappedPosition() + PkPointF(0, unzoomedSize.height()));
//     }

//     return decoration;
// }
