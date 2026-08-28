/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "animated_brush_annotation_data.h"

#include <PkStream.h>

#include <string>

int main()
{
    const PkByteArray annotation = captureAnimatedBrushAnnotation([](PkStream *stream) {
        const std::string bytes =
            "3 ncells:3 dim:2 rank0:3 sel0:incremental rank1:1 sel1:random";
        return stream->write(bytes.data(), static_cast<PkStream::pk_int64>(bytes.size())) ==
            static_cast<PkStream::pk_int64>(bytes.size());
    });
    const std::string expected =
        "3 ncells:3 dim:2 rank0:3 sel0:incremental rank1:1 sel1:random";
    const std::string actual(annotation.constData(), static_cast<std::size_t>(annotation.size()));
    return actual == expected ? 0 : 1;
}
