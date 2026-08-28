#ifndef JPEG_VALIDATION_H
#define JPEG_VALIDATION_H

#include <cstddef>
#include <limits>

inline bool jpegCheckedBufferSize(std::size_t width,
                                  std::size_t height,
                                  std::size_t channels,
                                  std::size_t &bytes)
{
    if (width == 0 || height == 0 || channels == 0 ||
        width > std::numeric_limits<std::size_t>::max() / channels) {
        return false;
    }
    const std::size_t rowBytes = width * channels;
    if (height > std::numeric_limits<std::size_t>::max() / rowBytes) {
        return false;
    }
    bytes = rowBytes * height;
    return true;
}

inline bool jpegMarkerPayloadAvailable(std::size_t markerSize, std::size_t headerSize)
{
    return markerSize >= headerSize;
}

#endif
