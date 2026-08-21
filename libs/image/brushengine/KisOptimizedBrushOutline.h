/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISOPTIMIZEDBRUSHOUTLINE_H
#define KISOPTIMIZEDBRUSHOUTLINE_H

#include <PkPolygon.h>
#include <PkTransform.h>
#include <PkRect.h>
#include <boost/iterator/iterator_facade.hpp>
#include <optional>
#include <kritaimage_export.h>

class PkPainterPath;

/**
 * An special class for storing the brush outline
 * in an optimized way. It converts the outline into
 * the vector of PkPolygonF objects right away and avoids
 * doing any modifications and/or transformations to it
 * until the final stage, when the outline is requested
 * to be drawn.
 */
class KRITAIMAGE_EXPORT KisOptimizedBrushOutline
{
public:
    class KRITAIMAGE_EXPORT const_iterator :
        public boost::iterator_facade <const_iterator,
                                       PkPolygonF,
                                       boost::forward_traversal_tag,
                                       PkPolygonF>
    {
    public:
        const_iterator()
            : m_outline(0),
              m_index(0) {}

        const_iterator(const KisOptimizedBrushOutline *outline, int index)
            : m_outline(outline),
              m_index(index) {}

    private:
        friend class boost::iterator_core_access;

        void increment() {
            m_index++;
        }

        bool equal(const_iterator const& other) const {
            return m_index == other.m_index &&
                m_outline == other.m_outline;
        }

        PkPolygonF dereference() const;

    private:
        const KisOptimizedBrushOutline *m_outline;
        int m_index;
    };

public:
    KisOptimizedBrushOutline();
    KisOptimizedBrushOutline(const PkPainterPath &path, const std::optional<PkRectF> &bounds = std::nullopt);
    KisOptimizedBrushOutline(const PkVector<PkPolygonF> &subpaths, const std::optional<PkRectF> &bounds = std::nullopt);

    PkRectF boundingRect() const;

    bool isEmpty() const;

    void addRect(const PkRectF &rc);
    void addEllipse(const PkPointF &center, qreal rx, qreal ry);
    void addPath(const PkPainterPath &path);
    void addPath(const KisOptimizedBrushOutline &path);

    void translate(qreal tx, qreal ty);
    void translate(const PkPointF &offset);

    /**
     * Transforms all the polygons belonging to the outline.
     * The transformation is done in optimized way, that is,
     * no polygons are transformed until the final iteration
     * over them.
     */
    void map(const PkTransform &t);

    /**
     * A helper function for \see map()
     */
    KisOptimizedBrushOutline mapped(const PkTransform &t) const;

    /**
     * Begins iteration over the polygons contained in the
     * brush outline. KisOptimizedBrushOutline will never return
     * a constructed PkVector of the polygons, because it may
     * require too many memory allocations.
     *
     * One cannot change the internal polygon, because the
     * returned polygon is transformed using the transformation
     * that is stored separately.
     */
    const_iterator begin() const;

    /**
     * End iterator for iteration over all the embedded polygons
     */
    const_iterator end() const;

private:
    PkVector<PkPolygonF> m_subpaths;
    PkVector<PkPolygonF> m_additionalDecorations;
    std::optional<PkRectF> m_explicitBounds;
    PkTransform m_transform;
    mutable PkRectF m_cachedBoundingRect;
};

#endif // KISOPTIMIZEDBRUSHOUTLINE_H
