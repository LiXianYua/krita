#include "jp2_validation.h"

#include <limits>

bool validateJp2Image(const opj_image_t &image, Jp2ValidatedImage &result)
{
    result = {};
    if (!image.comps || image.numcomps == 0 || image.x1 <= image.x0 || image.y1 <= image.y0) {
        return false;
    }
    const std::uint64_t width = static_cast<std::uint64_t>(image.x1) - image.x0;
    const std::uint64_t height = static_cast<std::uint64_t>(image.y1) - image.y0;
    if (width > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
        height > static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ||
        width > static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max()) / height) {
        return false;
    }
    const std::size_t pixels = static_cast<std::size_t>(width * height);
    if (pixels > static_cast<std::size_t>(std::numeric_limits<int>::max()) / 8) {
        return false;
    }
    const std::uint32_t precision = image.comps[0].prec;
    const bool isSigned = image.comps[0].sgnd != 0;
    if (precision == 0 || precision > 16) {
        return false;
    }
    for (std::uint32_t i = 0; i < image.numcomps; ++i) {
        const opj_image_comp_t &component = image.comps[i];
        if (!component.data || component.dx != 1 || component.dy != 1 ||
            component.w != width || component.h != height ||
            component.prec != precision || (component.sgnd != 0) != isSigned) {
            return false;
        }
    }
    result.width = static_cast<std::uint32_t>(width);
    result.height = static_cast<std::uint32_t>(height);
    result.pixelCount = pixels;
    result.precision = precision;
    result.isSigned = isSigned;
    return true;
}
