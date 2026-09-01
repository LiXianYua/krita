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

#include <PkPainterPath.h>

#include <algorithm>
#include <cstddef>
#include <limits>

namespace KisPathRasterizer {
namespace {

void writeSpans(const Private::Span *spans, int count, void *userData)
{
    auto *mask = static_cast<CoverageMask *>(userData);
    const int left = mask->bounds.x();
    const int right = left + mask->bounds.width();
    const int top = mask->bounds.y();
    const int bottom = top + mask->bounds.height();
    for (int i = 0; i < count; ++i) {
        const auto &span = spans[i];
        if (span.y < top || span.y >= bottom || span.length <= 0) {
            continue;
        }
        const int start = std::max(left, span.x);
        const int end = std::min(right, span.x + span.length);
        if (start >= end) {
            continue;
        }
        uint8_t *row = mask->alpha.data()
            + std::size_t(span.y - top) * std::size_t(mask->stride);
        std::fill(row + (start - left), row + (end - left), span.coverage);
    }
}

} // namespace

CoverageMask rasterizeFill(const PkPainterPath &path,
                           const PkRect &clip,
                           bool antialiased)
{
    CoverageMask mask;
    if (clip.isEmpty() || path.isEmpty()) {
        return mask;
    }
    const std::size_t width = std::size_t(clip.width());
    const std::size_t height = std::size_t(clip.height());
    if (height != 0 && width > std::numeric_limits<std::size_t>::max() / height) {
        return mask;
    }
    mask.bounds = clip;
    mask.stride = clip.width();
    mask.alpha.assign(width * height, uint8_t(0));
    Private::rasterizePath(path, clip, antialiased, writeSpans, &mask);
    return mask;
}

} // namespace KisPathRasterizer
