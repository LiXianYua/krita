#pragma once

#include <PkStream.h>

#include <string>

namespace CsvPrivate
{
inline bool writeAll(PkStream *stream, const std::string &bytes)
{
    if (!stream) {
        return false;
    }
    PkStream::pk_int64 offset = 0;
    const auto size = static_cast<PkStream::pk_int64>(bytes.size());
    while (offset < size) {
        const PkStream::pk_int64 written = stream->write(bytes.data() + offset, size - offset);
        if (written <= 0) {
            return false;
        }
        offset += written;
    }
    return true;
}
}
