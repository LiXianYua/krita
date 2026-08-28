#ifndef JXL_VALIDATION_H
#define JXL_VALIDATION_H

#include <cstddef>
#include <limits>

inline bool jxlCheckedImageBufferSize(std::size_t width,
                                      std::size_t height,
                                      std::size_t channels,
                                      std::size_t bytesPerChannel,
                                      std::size_t &bytes)
{
    if (width == 0 || height == 0 || channels == 0 || bytesPerChannel == 0 ||
        width > std::numeric_limits<std::size_t>::max() / height) {
        return false;
    }
    std::size_t pixels = width * height;
    if (pixels > std::numeric_limits<std::size_t>::max() / channels) {
        return false;
    }
    pixels *= channels;
    if (pixels > std::numeric_limits<std::size_t>::max() / bytesPerChannel) {
        return false;
    }
    bytes = pixels * bytesPerChannel;
    return true;
}

inline bool jxlInputMayGrow(std::size_t requestedBytes, std::size_t knownInputBytes)
{
    return requestedBytes <= knownInputBytes;
}

#endif
