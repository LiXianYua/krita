/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is derived from the QtGui module of the Qt Toolkit.
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
** Alternatively, this file may be used under the terms of the GNU General
** Public License version 2.0 or (at your option) the GNU General Public
** license version 3 or any later version approved by the KDE Free Qt
** Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "kis_path_rasterizer_p.h"

#include "kis_path_scan_converter_p.h"
#include "kis_path_stroker_p.h"

#include <PkPainterPath.h>
#include <PkPen.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <new>

namespace KisPathRasterizer {
namespace {

void writeSpans(const Private::Span *spans, int count, void *userData)
{
    auto *mask = static_cast<CoverageMask *>(userData);
    const int64_t left = mask->bounds.x();
    const int64_t right = left + int64_t(mask->bounds.width());
    const int64_t top = mask->bounds.y();
    const int64_t bottom = top + int64_t(mask->bounds.height());
    for (int i = 0; i < count; ++i) {
        const auto &span = spans[i];
        const int64_t spanY = span.y;
        if (spanY < top || spanY >= bottom || span.length <= 0) {
            continue;
        }
        const int64_t start = std::max(left, int64_t(span.x));
        const int64_t end = std::min(right, int64_t(span.x) + int64_t(span.length));
        if (start >= end) {
            continue;
        }
        uint8_t *row = mask->alpha.data()
            + std::size_t(spanY - top) * std::size_t(mask->stride);
        std::fill(row + std::size_t(start - left),
                  row + std::size_t(end - left),
                  span.coverage);
    }
}

bool hasOnlyFiniteElements(const PkPainterPath &path)
{
    for (int i = 0; i < path.elementCount(); ++i) {
        const auto element = path.elementAt(i);
        if (!std::isfinite(element.x) || !std::isfinite(element.y)) {
            return false;
        }
    }
    return true;
}

bool hasValidStrokeState(const PkPen &pen)
{
    if (!std::isfinite(pen.widthF()) || pen.widthF() < 0
        || !std::isfinite(pen.miterLimit()) || pen.miterLimit() < 0
        || !std::isfinite(pen.dashOffset())) {
        return false;
    }
    if (pen.style() < Qt::NoPen || pen.style() > Qt::CustomDashLine) {
        return false;
    }
    if (pen.capStyle() != Qt::FlatCap && pen.capStyle() != Qt::SquareCap
        && pen.capStyle() != Qt::RoundCap) {
        return false;
    }
    if (pen.joinStyle() != Qt::MiterJoin && pen.joinStyle() != Qt::BevelJoin
        && pen.joinStyle() != Qt::RoundJoin && pen.joinStyle() != Qt::SvgMiterJoin) {
        return false;
    }
    const auto pattern = pen.dashPattern();
    for (int i = 0; i < pattern.size(); ++i) {
        if (!std::isfinite(pattern.at(i))) {
            return false;
        }
    }
    return true;
}

} // namespace

CoverageMask rasterizeFill(const PkPainterPath &path,
                           const PkRect &clip,
                           bool antialiased)
{
    CoverageMask mask;
    if (clip.isEmpty() || path.isEmpty() || !hasOnlyFiniteElements(path)) {
        return mask;
    }
    const std::size_t width = std::size_t(clip.width());
    const std::size_t height = std::size_t(clip.height());
    if (height != 0 && width > std::numeric_limits<std::size_t>::max() / height) {
        return mask;
    }
    const std::size_t pixelCount = width * height;
    if (pixelCount > mask.alpha.max_size()) {
        return mask;
    }
    try {
        mask.alpha.assign(pixelCount, uint8_t(0));
        mask.bounds = clip;
        mask.stride = clip.width();
        Private::rasterizePath(path, clip, antialiased, writeSpans, &mask);
    } catch (const std::bad_alloc &) {
        return CoverageMask{};
    }
    return mask;
}

CoverageMask rasterizeStroke(const PkPainterPath &path,
                             const PkPen &pen,
                             const PkRect &clip,
                             bool antialiased)
{
    if (clip.isEmpty() || path.isEmpty() || pen.style() == Qt::NoPen
        || !hasOnlyFiniteElements(path) || !hasValidStrokeState(pen)) {
        return {};
    }
    const PkPainterPath outline = Private::createStrokeOutline(path, pen, clip);
    if (outline.isEmpty()) {
        return {};
    }
    return rasterizeFill(outline, clip, antialiased);
}

} // namespace KisPathRasterizer
