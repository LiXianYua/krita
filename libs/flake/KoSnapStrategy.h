/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008-2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOSNAPSTRATEGY_H
#define KOSNAPSTRATEGY_H

#include "KoSnapGuide.h"

#include <PkLine.h>

class TestSnapStrategy;
class KoPathPoint;
class KoSnapProxy;
class KoViewConverter;

class PkTransform;
class PkPainterPath;

class KRITAFLAKE_EXPORT KoSnapStrategy
{
public:
    enum SnapType {
        ToPoint = 0,
        ToLine
    };

public:
    KoSnapStrategy(KoSnapGuide::Strategy type);
    virtual ~KoSnapStrategy() {};

    virtual bool snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance) = 0;

    /// returns the strategies type
    KoSnapGuide::Strategy type() const;

    static qreal squareDistance(const PkPointF &p1, const PkPointF &p2);
    static qreal scalarProduct(const PkPointF &p1, const PkPointF &p2);

    /// returns the snapped position form the last call to snapToPoints
    PkPointF snappedPosition() const;
    SnapType snappedType() const;

    /// returns the current snap strategy decoration
    virtual PkPainterPath decoration(const KoViewConverter &converter) const = 0;

protected:
    /// sets the current snapped position
    void setSnappedPosition(const PkPointF &position, SnapType snapType);

private:
    KoSnapGuide::Strategy m_snapStrategyType;
    PkPointF m_snappedPosition;
    SnapType m_snappedType = ToPoint;
};

inline bool operator<(KoSnapStrategy::SnapType lhs, KoSnapStrategy::SnapType rhs)
{
    return int(lhs) < int(rhs);
}

/// snaps to x- or y-coordinates of path points
class KRITAFLAKE_EXPORT OrthogonalSnapStrategy : public KoSnapStrategy
{
public:
    OrthogonalSnapStrategy();
    bool snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance) override;
    PkPainterPath decoration(const KoViewConverter &converter) const override;
private:
    PkLineF m_hLine;
    PkLineF m_vLine;
};

/// snaps to path points
class KRITAFLAKE_EXPORT NodeSnapStrategy : public KoSnapStrategy
{
public:
    NodeSnapStrategy();
    bool snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance) override;
    PkPainterPath decoration(const KoViewConverter &converter) const override;
};

/// snaps extension lines of path shapes
class KRITAFLAKE_EXPORT ExtensionSnapStrategy : public KoSnapStrategy
{
    friend class TestSnapStrategy;
public:
    ExtensionSnapStrategy();
    bool snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance) override;
    PkPainterPath decoration(const KoViewConverter &converter) const override;
private:
    qreal project(const PkPointF &lineStart , const PkPointF &lineEnd, const PkPointF &point);
    PkPointF extensionDirection(KoPathPoint * point, const PkTransform &matrix);
    bool snapToExtension(PkPointF &position, KoPathPoint * point, const PkTransform &matrix);
    PkList<PkLineF> m_lines;
};

/// snaps to intersections of shapes
class KRITAFLAKE_EXPORT IntersectionSnapStrategy : public KoSnapStrategy
{
public:
    IntersectionSnapStrategy();
    bool snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance) override;
    PkPainterPath decoration(const KoViewConverter &converter) const override;
};

/// snaps to the canvas grid
class KRITAFLAKE_EXPORT GridSnapStrategy : public KoSnapStrategy
{
public:
    GridSnapStrategy();
    bool snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance) override;
    PkPainterPath decoration(const KoViewConverter &converter) const override;
};

/// snaps to shape bounding boxes
class KRITAFLAKE_EXPORT BoundingBoxSnapStrategy : public KoSnapStrategy
{
    friend class TestSnapStrategy;
public:
    BoundingBoxSnapStrategy();
    bool snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance) override;
    PkPainterPath decoration(const KoViewConverter &converter) const override;
private:
    qreal squareDistanceToLine(const PkPointF &lineA, const PkPointF &lineB, const PkPointF &point, PkPointF &pointOnLine);
    PkPointF m_boxPoints[5];
};

// KoGuidesData has been moved into Krita. Please port this class!
//
/// snaps to line guides
// class KRITAFLAKE_EXPORT LineGuideSnapStrategy : public KoSnapStrategy
// {
// public:
//     LineGuideSnapStrategy();
//     virtual bool snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance);
//     virtual PkPainterPath decoration(const KoViewConverter &converter) const;
// private:
//     int m_orientation;
// };

#endif // KOSNAPSTRATEGY_H
