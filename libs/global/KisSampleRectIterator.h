/*
 *  SPDX-FileCopyrightText: 2023 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISSAMPLERECTITERATOR_H
#define KISSAMPLERECTITERATOR_H

#include "kritaglobal_export.h"

#include <boost/iterator_adaptors.hpp>

#include <memory>

#include <PkRect.h>

/**
 * A simple generator-style iterator that samples the passed rectangle
 * (PkRectF) with semi-random points.
 *
 * The first _nine_ points returned by the iterator correspond to
 * the corners and midpoints of the rectangle. From 10th and further
 * the iterator returns "random" samples inside the rectangle generated
 * by a fixed Halton sequence.
 *
 * Usage:
 *        \code{.cpp}
 *
 *        KisSampleRectIterator sampler(rect);
 *        while (1) {
 *             const PkPointF sampledPoint = *sampler++;
 *             /// ... do something ...
 *        }
 *
 *        \endcode
 */
class KRITAGLOBAL_EXPORT KisSampleRectIterator
    : public boost::iterator_facade<KisSampleRectIterator,
                                    PkPointF,
                                    boost::forward_traversal_tag,
                                    PkPointF>
{
public:
    KisSampleRectIterator();
    KisSampleRectIterator(const PkRectF &rect);
    KisSampleRectIterator(const KisSampleRectIterator &rhs);
    KisSampleRectIterator(KisSampleRectIterator &&rhs);
    KisSampleRectIterator& operator=(const KisSampleRectIterator &rhs);
    KisSampleRectIterator& operator=(KisSampleRectIterator &&rhs);
    ~KisSampleRectIterator();

public:
    int numSamples() const;

private:
    friend class boost::iterator_core_access;

    void increment();
    PkPointF dereference() const;

private:
    struct HaltonSampler;
    std::shared_ptr<HaltonSampler> m_sampler;

    PkRectF m_rect;
    int m_index = 0;
};

#endif // KISSAMPLERECTITERATOR_H
