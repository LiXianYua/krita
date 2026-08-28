/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "animated_brush_annotation_data.h"

#include <PkMemoryStream.h>

#include <limits>

PkByteArray captureAnimatedBrushAnnotation(const std::function<bool(PkStream *)> &serialize)
{
    if (!serialize) {
        return {};
    }

    PkMemoryStream buffer;
    if (!buffer.open(PkStream::WriteOnly) || !serialize(&buffer)) {
        return {};
    }
    if (buffer.size() < 0 ||
        buffer.size() > static_cast<PkStream::pk_int64>(std::numeric_limits<int>::max())) {
        return {};
    }
    return PkByteArray(buffer.data(), static_cast<int>(buffer.size()));
}
