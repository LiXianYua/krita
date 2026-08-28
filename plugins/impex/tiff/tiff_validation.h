#ifndef TIFF_VALIDATION_H
#define TIFF_VALIDATION_H

#include <cstddef>
#include <limits>

inline bool tiffCheckedRasterSize(std::size_t width,
                                  std::size_t height,
                                  std::size_t samples,
                                  std::size_t bytesPerSample,
                                  std::size_t &bytes)
{
    if (width == 0 || height == 0 || samples == 0 || bytesPerSample == 0 ||
        width > std::numeric_limits<std::size_t>::max() / height) {
        return false;
    }
    std::size_t pixels = width * height;
    if (pixels > std::numeric_limits<std::size_t>::max() / samples) {
        return false;
    }
    pixels *= samples;
    if (pixels > std::numeric_limits<std::size_t>::max() / bytesPerSample) {
        return false;
    }
    bytes = pixels * bytesPerSample;
    return true;
}

inline bool tiffTagPayloadAvailable(std::size_t payloadSize, std::size_t requiredSize)
{
    return payloadSize >= requiredSize;
}

#endif
