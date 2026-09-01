/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_BRUSH_STREAM_UTILS_H
#define KIS_BRUSH_STREAM_UTILS_H

#include <vector>

#include <PkAuxTypes.h>
#include <PkStream.h>

inline PkByteArray kisBrushReadAll(PkStream *stream)
{
    std::vector<char> bytes;
    char chunk[8192];
    PkStream::pk_int64 count = 0;
    while ((count = stream->read(chunk, sizeof(chunk))) > 0) {
        bytes.insert(bytes.end(), chunk, chunk + count);
    }
    return PkByteArray(bytes.data(), static_cast<int>(bytes.size()));
}

#endif
