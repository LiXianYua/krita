#ifndef HEIF_VALIDATION_H
#define HEIF_VALIDATION_H

#include <cstddef>
#include <limits>

enum class HeifReaderTargetStatus {
    Reached,
    BeyondEof
};

inline HeifReaderTargetStatus heifReaderTargetStatus(std::size_t target,
                                                     std::size_t knownLength)
{
    return target <= knownLength ? HeifReaderTargetStatus::Reached
                                 : HeifReaderTargetStatus::BeyondEof;
}

inline bool heifCheckedPlaneSize(std::size_t minimumRowBytes,
                                 std::size_t height,
                                 std::size_t stride,
                                 std::size_t &bytes)
{
    if (minimumRowBytes == 0 || height == 0 || stride < minimumRowBytes ||
        height > std::numeric_limits<std::size_t>::max() / stride) {
        return false;
    }
    bytes = height * stride;
    return true;
}

#endif
