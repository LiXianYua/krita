/*
 *  SPDX-FileCopyrightText: 2020 Sharaf Zaman <sharafzaz121@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef SVGMESHPATCH_H
#define SVGMESHPATCH_H
#include <PkColor.h>
#include <PkGlobal.h>
#include <PkPainterPath.h>
#include <PkPair.h>
#include <PkPoint.h>
#include <PkRect.h>
#include <PkSize.h>
#include <PkString.h>
#include <PkTransform.h>
#include <PkVector.h>


#include <pk/color/PkColor.h>

#include <array>

#include <KoPathShape.h>

struct SvgMeshStop {
    PkColor color;
    PkPointF point;

    SvgMeshStop()
    {}

    SvgMeshStop(PkColor color, PkPointF point)
        : color(color), point(point)
    {}

    bool isValid() const { return color.isValid(); }
};

using SvgMeshPath = std::array<PkPointF, 4>;

class KRITAFLAKE_EXPORT SvgMeshPatch
{
public:
    /// Position of stop in the patch
    enum Type {
        Top = 0,
        Right,
        Bottom,
        Left,
        Size,
    };

    SvgMeshPatch(PkPointF startingPoint);
    SvgMeshPatch(const SvgMeshPatch& other);

    // NOTE: NO path is created here
    // sets a new starting point for the patch
    void moveTo(const PkPointF& p);
    /// Helper to convert to a cubic curve internally.
    void lineTo(const PkPointF& p);
    /// add points as curve.
    void curveTo(const PkPointF& c1, const PkPointF& c2, const PkPointF& p);

    /// returns the starting point of the stop
    SvgMeshStop getStop(Type type) const;

    /// returns the midPoint in parametric space
    inline PkPointF getMidpointParametric(Type type) const {
        return (m_parametricCoords[type] + m_parametricCoords[(type + 1) % Size]) * 0.5;
    }

    /// get the point on a segment using De Casteljau's algorithm
    PkPointF segmentPointAt(Type type, qreal t) const;

    /// split a segment using De Casteljau's algorithm
    PkPair<std::array<PkPointF, 4>, std::array<PkPointF, 4>> segmentSplitAt(Type type, qreal t) const;

    /// Get a segment of the path in the meshpatch
    std::array<PkPointF, 4> getSegment(Type type) const;

    /// Get full (closed) meshpath
    PkPainterPath getPath() const;

    /// Get size swept by mesh in pts
    PkSizeF size() const;

    PkRectF boundingRect() const;

    /// Gets the curve passing through the middle of meshpatch
    std::array<PkPointF, 4> getMidCurve(bool isVertical) const;

    void subdivideHorizontally(PkVector<SvgMeshPatch*>& subdivided,
                               const PkVector<PkColor>& colors) const;

    void subdivideVertically(PkVector<SvgMeshPatch*>& subdivided,
                             const PkVector<PkColor>& colors) const;

    void subdivide(PkVector<SvgMeshPatch*>& subdivided,
                   const PkVector<PkColor>& colors) const;

    bool isDivisibleVertically() const;
    bool isDivisibleHorizontally() const;

    int countPoints() const;

    /* Parses raw pathstr and adds path to the shape, if the path isn't
     * complete, it will have to be computed and given with pathIncomplete = true
     * (Ideal case for std::optional)
     */
    void addStop(const PkString& pathStr, PkColor color, Type edge, bool pathIncomplete = false, PkPointF lastPoint = PkPointF());

    /// Adds path to the shape
    void addStop(const std::array<PkPointF, 4>& pathPoints, PkColor color, Type edge);

    /// Adds linear path to the shape
    void addStopLinear(const std::array<PkPointF, 2>& pathPoints, PkColor color, Type edge);

    void modifyPath(SvgMeshPatch::Type type, std::array<PkPointF, 4> newPath);
    void modifyCorner(SvgMeshPatch::Type type, const PkPointF &delta);

    void setStopColor(SvgMeshPatch::Type type, const PkColor &color);

    void setTransform(const PkTransform& matrix);

private:
    /* Parses path and adds it to m_path and returns the last point of the curve/line
     * see also: SvgMeshPatch::addStop
     */
    PkPointF parseMeshPath(const PkString& path, bool pathIncomplete = false, const PkPointF lastPoint = PkPointF());
    const char* getCoord(const char* ptr, qreal& number);

private:
    bool m_newPath;
    int counter {0};

    /// This is the starting point for each path
    PkPointF m_startingPoint;

    std::array<SvgMeshStop, Size> m_nodes;
    std::array<std::array<PkPointF, 4>, 4> controlPoints;
    /// Coordinates in UV space
    std::array<PkPointF, 4> m_parametricCoords;
};

#endif // SVGMESHPATCH_H
