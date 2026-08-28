#ifndef TIFF_VALIDATION_H
#define TIFF_VALIDATION_H

#include <cstddef>
#include <cstdint>
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

inline bool tiffValidDirectoryShape(std::uint32_t width,
                                    std::uint32_t height,
                                    std::uint16_t samples,
                                    std::uint16_t extraSamples)
{
    return width > 0 && height > 0 &&
           width <= static_cast<std::uint32_t>(std::numeric_limits<int>::max()) &&
           height <= static_cast<std::uint32_t>(std::numeric_limits<int>::max()) &&
           samples > 0 && extraSamples <= samples;
}

inline bool tiffValidChunkGeometry(std::uint32_t width,
                                   std::uint32_t height,
                                   std::size_t encodedSize)
{
    return width > 0 && height > 0 && encodedSize > 0;
}

#endif
