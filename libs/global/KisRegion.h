/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISREGION_H
#define KISREGION_H

#include "kritaglobal_export.h"
#include <PkVector.h>
#include <PkRect.h>
#include <boost/operators.hpp>

class PkRegion;

/**
 * An more efficient (and more limited) replacement for PkRegion.
 *
 * Its main purpose it to be able to merge a huge set of rectangles
 * into a smaller set of bigger rectangles, the same thing that PkRegion
 * is supposed to do. The main difference (and limitation) is: all the
 * input rects must be non-intersecting. This requirement is perfectly
 * fine for Krita's tiles, which do never intersect.
 */
class KRITAGLOBAL_EXPORT KisRegion :
        public boost::equality_comparable<KisRegion>,
        public boost::andable<KisRegion, PkRect>
{
public:
    /**
     * @brief merge a set of rectangles into a smaller set of bigger rectangles
     *
     * The algorithm does two passes over the rectangles. First it tries to
     * merge all the rectangles horizontally, then vertically. The merge happens
     * in-place, that is, all the merged elements will be moved to the front
     * of the original range.
     *
     * The final range is defined by [beginIt, retvalIt)
     *
     * @param beginIt iterator to the beginning of the source range
     * @param endIt iterator to the end of the source range
     * @return iteration pointing past the last element of the merged range
     */
    static QVector<PkRect>::iterator mergeSparseRects(QVector<PkRect>::iterator beginIt, QVector<PkRect>::iterator endIt);

    /**
     * Simplifies \p rects in a way that they don't overlap anymore. The actual
     * resulting area may be larger than original \p rects, but not more than
     * \p gridSize in any dimension.
     */
    static void approximateOverlappingRects(QVector<PkRect> &rects, int gridSize);

    static void makeGridLikeRectsUnique(QVector<PkRect> &rects);

public:
    KisRegion() = default;
    KisRegion(const KisRegion &rhs) = default;
    KisRegion(const PkRect &rect);
    KisRegion(std::initializer_list<PkRect> rects);

    /**
     * @brief creates a region from a set of non-intersecting rectangles
     * @param rects rectangles that should be merged. Rectangles must not intersect.
     */
    KisRegion(const QVector<PkRect> &rects);
    KisRegion(QVector<PkRect> &&rects);

    KisRegion& operator=(const KisRegion &rhs);
    friend KRITAGLOBAL_EXPORT bool operator==(const KisRegion &lhs, const KisRegion &rhs);

    KisRegion& operator&=(const PkRect &rect);

    PkRect boundingRect() const;
    QVector<PkRect> rects() const;
    int rectCount() const;
    bool isEmpty() const;

    PkRegion toQRegion() const;

    void translate(int dx, int dy);
    KisRegion translated(int x, int y) const;

    static KisRegion fromQRegion(const PkRegion &region);

    /**
     * Approximates a KisRegion from \p rects, which may overlap. The resulting
     * KisRegion may be larger than the original set of rects, but it is guaranteed
     * to cover it completely.
     */
    static KisRegion fromOverlappingRects(const QVector<PkRect> &rects, int gridSize);

private:
    void mergeAllRects();

private:
    QVector<PkRect> m_rects;
};

KRITAGLOBAL_EXPORT bool operator==(const KisRegion &lhs, const KisRegion &rhs);

#endif // KISREGION_H
