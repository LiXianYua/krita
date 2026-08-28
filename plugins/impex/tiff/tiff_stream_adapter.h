#ifndef TIFF_STREAM_ADAPTER_H
#define TIFF_STREAM_ADAPTER_H

#include <PkStream.h>

#include <cstdio>
#include <cstring>
#include <cstdint>
#include <limits>
#include <new>
#include <vector>

#include <tiffio.h>

inline bool kisTiffWriteExact(PkStream &stream, const char *data, std::size_t size)
{
    return data && size <= static_cast<std::size_t>(std::numeric_limits<PkStream::pk_int64>::max()) &&
           stream.write(data, static_cast<PkStream::pk_int64>(size)) ==
               static_cast<PkStream::pk_int64>(size);
}

constexpr std::uint64_t MaxMetadataAdapterBytes = 512ull * 1024ull * 1024ull;

enum class KisTiffMetadataSnapshotResult {
    Available,
    Skipped,
    Failed,
};

inline KisTiffMetadataSnapshotResult kisTiffSnapshotMetadata(
    PkStream &stream,
    std::vector<std::uint8_t> &bytes)
{
    const PkStream::pk_int64 size = stream.size();
    bytes.clear();
    if (!stream.isReadable() || size <= 0 ||
        static_cast<std::uint64_t>(size) > std::numeric_limits<std::size_t>::max()) {
        return KisTiffMetadataSnapshotResult::Failed;
    }
    if (static_cast<std::uint64_t>(size) > MaxMetadataAdapterBytes) {
        return KisTiffMetadataSnapshotResult::Skipped;
    }
    if (!stream.seek(0)) {
        return KisTiffMetadataSnapshotResult::Failed;
    }
    try {
        bytes.resize(static_cast<std::size_t>(size));
    } catch (const std::bad_alloc &) {
        return KisTiffMetadataSnapshotResult::Failed;
    }
    std::size_t offset = 0;
    while (offset < bytes.size()) {
        const auto count = stream.read(reinterpret_cast<char *>(bytes.data() + offset),
                                       static_cast<PkStream::pk_int64>(bytes.size() - offset));
        if (count <= 0) {
            bytes.clear();
            return KisTiffMetadataSnapshotResult::Failed;
        }
        offset += static_cast<std::size_t>(count);
    }
    return KisTiffMetadataSnapshotResult::Available;
}

inline tmsize_t kisTiffStreamRead(thandle_t handle, void *data, tmsize_t size)
{
    if (!handle || size < 0) return -1;
    return static_cast<PkStream *>(handle)->read(static_cast<char *>(data), size);
}

inline tmsize_t kisTiffStreamWrite(thandle_t handle, void *data, tmsize_t size)
{
    if (!handle || size < 0) return -1;
    return static_cast<PkStream *>(handle)->write(static_cast<const char *>(data), size);
}

inline toff_t kisTiffStreamSeek(thandle_t handle, toff_t offset, int whence)
{
    auto *stream = static_cast<PkStream *>(handle);
    if (!stream || sizeof(offset) != sizeof(PkStream::pk_int64)) {
        return static_cast<toff_t>(-1);
    }
    PkStream::pk_int64 signedOffset = 0;
    std::memcpy(&signedOffset, &offset, sizeof(signedOffset));
    PkStream::pk_int64 base = 0;
    if (whence == SEEK_CUR) base = stream->pos();
    else if (whence == SEEK_END) base = stream->size();
    else if (whence != SEEK_SET) return static_cast<toff_t>(-1);
    if (base < 0 ||
        (signedOffset > 0 && base > std::numeric_limits<PkStream::pk_int64>::max() - signedOffset)) {
        return static_cast<toff_t>(-1);
    }
    const auto position = base + signedOffset;
    return stream->seek(position) ? static_cast<toff_t>(position) : static_cast<toff_t>(-1);
}

inline int kisTiffStreamClose(thandle_t) { return 0; }
inline toff_t kisTiffStreamSize(thandle_t handle)
{
    const auto size = handle ? static_cast<PkStream *>(handle)->size() : -1;
    return size >= 0 ? static_cast<toff_t>(size) : 0;
}
inline int kisTiffStreamMap(thandle_t, void **, toff_t *) { return 0; }
inline void kisTiffStreamUnmap(thandle_t, void *, toff_t) {}

inline TIFF *kisTiffOpenStream(PkStream *stream, const char *mode)
{
    if (!stream || !stream->seek(0)) return nullptr;
    return TIFFClientOpen("PkStream", mode, stream,
                          kisTiffStreamRead, kisTiffStreamWrite,
                          kisTiffStreamSeek, kisTiffStreamClose,
                          kisTiffStreamSize, kisTiffStreamMap,
                          kisTiffStreamUnmap);
}

#endif
