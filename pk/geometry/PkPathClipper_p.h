/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#ifndef PKPATHCLIPPER_P_H
#define PKPATHCLIPPER_P_H

// Adapted for R-39 from Qt 5.15.7 qpathclipper_p.h. Type substitutions:
// QPainterPath/QPointF/QLineF/QRectF -> their Pk counterparts;
// QDataBuffer/QVector -> private std::vector-backed containers. The winged-edge
// topology and clipping decisions below intentionally remain upstream-shaped.
// Upstream source: qtbase/src/gui/painting/qpathclipper_p.h (Qt 5.15.7).

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include "PkPainterPath.h"
#include "PkLine.h"

#include <cstddef>
#include <cstdio>
#include <utility>
#include <vector>

template <typename T>
class PkDataBuffer
{
public:
    explicit PkDataBuffer(int reserveSize) { m_data.reserve(static_cast<std::size_t>(reserveSize)); }
    PkDataBuffer(const PkDataBuffer &) = delete;
    PkDataBuffer &operator=(const PkDataBuffer &) = delete;
    PkDataBuffer(PkDataBuffer &&) = default;
    PkDataBuffer &operator=(PkDataBuffer &&) = default;

    void reset() { m_data.clear(); }
    bool isEmpty() const { return m_data.empty(); }
    int size() const { return static_cast<int>(m_data.size()); }
    T *data() { return m_data.data(); }
    const T *data() const { return m_data.data(); }
    T &at(int i) { return m_data.at(static_cast<std::size_t>(i)); }
    const T &at(int i) const { return m_data.at(static_cast<std::size_t>(i)); }
    T &last() { return m_data.back(); }
    const T &last() const { return m_data.back(); }
    T &first() { return m_data.front(); }
    const T &first() const { return m_data.front(); }
    void add(const T &value) { m_data.push_back(value); }
    void pop_back() { m_data.pop_back(); }
    void resize(int size) { m_data.resize(static_cast<std::size_t>(size)); }
    void reserve(int size) { m_data.reserve(static_cast<std::size_t>(size)); }
    void shrink(int size) { m_data.resize(static_cast<std::size_t>(size)); m_data.shrink_to_fit(); }
    void swap(PkDataBuffer &other) { m_data.swap(other.m_data); }
    PkDataBuffer &operator<<(const T &value) { add(value); return *this; }

private:
    std::vector<T> m_data;
};

template <typename T>
class PkClipperVector : public std::vector<T>
{
public:
    using std::vector<T>::vector;
    bool isEmpty() const { return this->empty(); }
    PkClipperVector &operator<<(const T &value) { this->push_back(value); return *this; }
    PkClipperVector &operator+=(const T &value) { this->push_back(value); return *this; }
};


class PkWingedEdge;

class PkPathClipper
{
public:
    enum Operation {
        BoolAnd,
        BoolOr,
        BoolSub,
        Simplify
    };
public:
    PkPathClipper(const PkPainterPath &subject,
                 const PkPainterPath &clip);

    PkPainterPath clip(Operation op = BoolAnd);

    bool intersect();
    bool contains();

    static bool pathToRect(const PkPainterPath &path, PkRectF *rect = nullptr);
    static PkPainterPath intersect(const PkPainterPath &path, const PkRectF &rect);

private:
    PkPathClipper(const PkPathClipper &) = delete;
    PkPathClipper &operator=(const PkPathClipper &) = delete;
    PkPathClipper(PkPathClipper &&) = delete;
    PkPathClipper &operator=(PkPathClipper &&) = delete;

    enum ClipperMode {
        ClipMode, // do the full clip
        CheckMode // for contains/intersects (only interested in whether the result path is non-empty)
    };

    bool handleCrossingEdges(PkWingedEdge &list, qreal y, ClipperMode mode);
    bool doClip(PkWingedEdge &list, ClipperMode mode);

    PkPainterPath subjectPath;
    PkPainterPath clipPath;
    Operation op;

    int aMask;
    int bMask;
};

struct PkPathVertex
{
public:
    PkPathVertex(const PkPointF &p = PkPointF(), int e = -1);
    operator PkPointF() const;

    int edge;

    qreal x;
    qreal y;
};

class PkPathEdge
{
public:
    enum Traversal {
        RightTraversal,
        LeftTraversal
    };

    enum Direction {
        Forward,
        Backward
    };

    enum Type {
        Line,
        Curve
    };

    explicit PkPathEdge(int a = -1, int b = -1);

    mutable int flag;

    int windingA;
    int windingB;

    int first;
    int second;

    double angle;
    double invAngle;

    int next(Traversal traversal, Direction direction) const;

    void setNext(Traversal traversal, Direction direction, int next);
    void setNext(Direction direction, int next);

    Direction directionTo(int vertex) const;
    int vertex(Direction direction) const;

private:
    int m_next[2][2] = { { -1, -1 }, { -1, -1 } };
};

class PkPathSegments
{
public:
    struct Intersection {
        qreal t;
        int vertex;
        int next;

        bool operator<(const Intersection &o) const {
            return t < o.t;
        }
    };

    struct Segment {
        Segment(int pathId, int vertexA, int vertexB)
            : path(pathId)
            , va(vertexA)
            , vb(vertexB)
            , intersection(-1)
        {
        }

        int path;

        // vertices
        int va;
        int vb;

        // intersection index
        int intersection;

        PkRectF bounds;
    };


    PkPathSegments(int reserve);

    void setPath(const PkPainterPath &path);
    void addPath(const PkPainterPath &path);

    int intersections() const;
    int segments() const;
    int points() const;

    const Segment &segmentAt(int index) const;
    const PkLineF lineAt(int index) const;
    const PkRectF &elementBounds(int index) const;
    int pathId(int index) const;

    const PkPointF &pointAt(int vertex) const;
    int addPoint(const PkPointF &point);

    const Intersection *intersectionAt(int index) const;
    void addIntersection(int index, const Intersection &intersection);

    void mergePoints();

private:
    PkDataBuffer<PkPointF> m_points;
    PkDataBuffer<Segment> m_segments;
    PkDataBuffer<Intersection> m_intersections;

    int m_pathId;
};

class PkWingedEdge
{
public:
    struct TraversalStatus
    {
        int edge;
        PkPathEdge::Traversal traversal;
        PkPathEdge::Direction direction;

        void flipDirection();
        void flipTraversal();

        void flip();
    };

    PkWingedEdge();
    PkWingedEdge(const PkPainterPath &subject, const PkPainterPath &clip);

    void simplify();
    PkPainterPath toPath() const;

    int edgeCount() const;

    PkPathEdge *edge(int edge);
    const PkPathEdge *edge(int edge) const;

    int vertexCount() const;

    int addVertex(const PkPointF &p);

    PkPathVertex *vertex(int vertex);
    const PkPathVertex *vertex(int vertex) const;

    TraversalStatus next(const TraversalStatus &status) const;

    int addEdge(const PkPointF &a, const PkPointF &b);
    int addEdge(int vertexA, int vertexB);

    bool isInside(qreal x, qreal y) const;

    static PkPathEdge::Traversal flip(PkPathEdge::Traversal traversal);
    static PkPathEdge::Direction flip(PkPathEdge::Direction direction);

private:
    void intersectAndAdd();

    void printNode(int i, FILE *handle);

    void removeEdge(int ei);

    int insert(const PkPathVertex &vertex);
    TraversalStatus findInsertStatus(int vertex, int edge) const;

    qreal delta(int vertex, int a, int b) const;

    PkDataBuffer<PkPathEdge> m_edges;
    PkDataBuffer<PkPathVertex> m_vertices;

    PkClipperVector<qreal> m_splitPoints;

    PkPathSegments m_segments;
};

inline PkPathEdge::PkPathEdge(int a, int b)
    : flag(0)
    , windingA(0)
    , windingB(0)
    , first(a)
    , second(b)
    , angle(0)
    , invAngle(0)
{
}

inline int PkPathEdge::next(Traversal traversal, Direction direction) const
{
    return m_next[int(traversal)][int(direction)];
}

inline void PkPathEdge::setNext(Traversal traversal, Direction direction, int next)
{
    m_next[int(traversal)][int(direction)] = next;
}

inline void PkPathEdge::setNext(Direction direction, int next)
{
    m_next[0][int(direction)] = next;
    m_next[1][int(direction)] = next;
}

inline PkPathEdge::Direction PkPathEdge::directionTo(int vertex) const
{
    return first == vertex ? Backward : Forward;
}

inline int PkPathEdge::vertex(Direction direction) const
{
    return direction == Backward ? first : second;
}

inline PkPathVertex::PkPathVertex(const PkPointF &p, int e)
    : edge(e)
    , x(p.x())
    , y(p.y())
{
}

inline PkPathVertex::operator PkPointF() const
{
    return PkPointF(x, y);
}

inline PkPathSegments::PkPathSegments(int reserve) :
    m_points(reserve),
    m_segments(reserve),
    m_intersections(reserve),
    m_pathId(0)
{
}

inline int PkPathSegments::segments() const
{
    return m_segments.size();
}

inline int PkPathSegments::points() const
{
    return m_points.size();
}

inline const PkPointF &PkPathSegments::pointAt(int i) const
{
    return m_points.at(i);
}

inline int PkPathSegments::addPoint(const PkPointF &point)
{
    m_points << point;
    return m_points.size() - 1;
}

inline const PkPathSegments::Segment &PkPathSegments::segmentAt(int index) const
{
    return m_segments.at(index);
}

inline const PkLineF PkPathSegments::lineAt(int index) const
{
    const Segment &segment = m_segments.at(index);
    return PkLineF(m_points.at(segment.va), m_points.at(segment.vb));
}

inline const PkRectF &PkPathSegments::elementBounds(int index) const
{
    return m_segments.at(index).bounds;
}

inline int PkPathSegments::pathId(int index) const
{
    return m_segments.at(index).path;
}

inline const PkPathSegments::Intersection *PkPathSegments::intersectionAt(int index) const
{
    const int intersection = m_segments.at(index).intersection;
    if (intersection < 0)
        return nullptr;
    else
        return &m_intersections.at(intersection);
}

inline int PkPathSegments::intersections() const
{
    return m_intersections.size();
}

inline void PkPathSegments::addIntersection(int index, const Intersection &intersection)
{
    m_intersections << intersection;

    Segment &segment = m_segments.at(index);
    if (segment.intersection < 0) {
        segment.intersection = m_intersections.size() - 1;
    } else {
        Intersection *isect = &m_intersections.at(segment.intersection);

        while (isect->next != 0)
            isect += isect->next;

        isect->next = (m_intersections.size() - 1) - (isect - m_intersections.data());
    }
}

inline int PkWingedEdge::edgeCount() const
{
    return m_edges.size();
}

inline PkPathEdge *PkWingedEdge::edge(int edge)
{
    return edge < 0 ? nullptr : &m_edges.at(edge);
}

inline const PkPathEdge *PkWingedEdge::edge(int edge) const
{
    return edge < 0 ? nullptr : &m_edges.at(edge);
}

inline int PkWingedEdge::vertexCount() const
{
    return m_vertices.size();
}

inline int PkWingedEdge::addVertex(const PkPointF &p)
{
    m_vertices << p;
    return m_vertices.size() - 1;
}

inline PkPathVertex *PkWingedEdge::vertex(int vertex)
{
    return vertex < 0 ? nullptr : &m_vertices.at(vertex);
}

inline const PkPathVertex *PkWingedEdge::vertex(int vertex) const
{
    return vertex < 0 ? nullptr : &m_vertices.at(vertex);
}

inline PkPathEdge::Traversal PkWingedEdge::flip(PkPathEdge::Traversal traversal)
{
    return traversal == PkPathEdge::RightTraversal ? PkPathEdge::LeftTraversal : PkPathEdge::RightTraversal;
}

inline void PkWingedEdge::TraversalStatus::flipTraversal()
{
    traversal = PkWingedEdge::flip(traversal);
}

inline PkPathEdge::Direction PkWingedEdge::flip(PkPathEdge::Direction direction)
{
    return direction == PkPathEdge::Forward ? PkPathEdge::Backward : PkPathEdge::Forward;
}

inline void PkWingedEdge::TraversalStatus::flipDirection()
{
    direction = PkWingedEdge::flip(direction);
}

inline void PkWingedEdge::TraversalStatus::flip()
{
    flipDirection();
    flipTraversal();
}

#endif // PKPATHCLIPPER_P_H
