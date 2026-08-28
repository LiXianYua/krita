#ifndef TIFF_VALIDATION_H
#define TIFF_VALIDATION_H

#include <cstddef>
#include <cstdint>
#include <limits>

#include <tiffio.h>

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
    return width > 0 && height > 0 && encodedSize > 0 &&
           width <= static_cast<std::uint32_t>(std::numeric_limits<int>::max()) &&
           height <= static_cast<std::uint32_t>(std::numeric_limits<int>::max()) &&
           encodedSize <= static_cast<std::size_t>(std::numeric_limits<int>::max());
}

inline std::uint16_t tiffMinimumBaseSamples(std::uint16_t photometric)
{
    switch (photometric) {
    case PHOTOMETRIC_MINISWHITE:
    case PHOTOMETRIC_MINISBLACK:
        return 1;
    case PHOTOMETRIC_RGB:
    case PHOTOMETRIC_YCBCR:
    case PHOTOMETRIC_CIELAB:
    case PHOTOMETRIC_ICCLAB:
        return 3;
    case PHOTOMETRIC_SEPARATED:
        return 4;
    case PHOTOMETRIC_PALETTE:
        return 2;
    default:
        return 0;
    }
}

inline bool tiffHasMinimumBaseSamples(std::uint16_t photometric,
                                      std::uint16_t samples)
{
    const std::uint16_t minimum = tiffMinimumBaseSamples(photometric);
    return minimum == 0 || samples >= minimum;
}

inline bool tiffValidSubsampling(std::uint16_t horizontal,
                                 std::uint16_t vertical)
{
    return horizontal > 0 && vertical > 0;
}

inline bool tiffActualReadSize(tmsize_t result,
                               std::size_t capacity,
                               std::size_t &actualSize)
{
    if (result <= 0 || static_cast<std::uintmax_t>(result) > capacity) {
        return false;
    }
    actualSize = static_cast<std::size_t>(result);
    return true;
}

#endif
