#include "PkSvgRasterizer.h"

#include <algorithm>
#include <cstring>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#define NANOSVG_IMPLEMENTATION
#include "thirdparty/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "thirdparty/nanosvgrast.h"

namespace {

struct SvgDeleter {
    void operator()(NSVGimage *image) const { nsvgDelete(image); }
};

struct RasterizerDeleter {
    void operator()(NSVGrasterizer *rasterizer) const { nsvgDeleteRasterizer(rasterizer); }
};

}

PkImage PkSvgRasterizer::render(const char *svg, std::size_t size, int outputWidth)
try
{
    // Same encoded/decoded ceiling as PkImageFileDecoder / native codecs.
    constexpr std::size_t maximumBytes = 512u * 1024u * 1024u;
    if (!svg || size == 0 || size > maximumBytes || outputWidth <= 0 ||
        outputWidth > std::numeric_limits<int>::max() / 4) {
        return {};
    }

    // NanoSVG parses in place and requires a terminating null byte.
    std::vector<char> document(size + 1);
    std::memcpy(document.data(), svg, size);
    document[size] = '\0';

    std::unique_ptr<NSVGimage, SvgDeleter> image(
        nsvgParse(document.data(), "px", 96.0f));
    if (!image || !std::isfinite(image->width) || !std::isfinite(image->height) ||
        !(image->width > 0.0f) || !(image->height > 0.0f)) {
        return {};
    }

    const double scaledHeight = static_cast<double>(outputWidth) * image->height / image->width;
    if (!std::isfinite(scaledHeight) || scaledHeight > std::numeric_limits<int>::max()) return {};
    const int outputHeight = std::max(1, static_cast<int>(scaledHeight));
    const std::size_t stride = static_cast<std::size_t>(outputWidth) * 4u;
    if (static_cast<std::size_t>(outputHeight) > maximumBytes / stride) return {};
    const float scale = static_cast<float>(outputWidth) / image->width;

    std::unique_ptr<NSVGrasterizer, RasterizerDeleter> rasterizer(nsvgCreateRasterizer());
    if (!rasterizer) {
        return {};
    }

    std::vector<unsigned char> rgba(static_cast<std::size_t>(outputWidth) *
                                    static_cast<std::size_t>(outputHeight) * 4u, 0u);
    nsvgRasterize(rasterizer.get(), image.get(), 0.0f, 0.0f, scale,
                  rgba.data(), outputWidth, outputHeight, outputWidth * 4);

    PkImage result(outputWidth, outputHeight, PkImage::Format_ARGB32);
    if (result.isNull()) return {};
    for (int y = 0; y < outputHeight; ++y) {
        for (int x = 0; x < outputWidth; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * static_cast<std::size_t>(outputWidth) +
                 static_cast<std::size_t>(x)) * 4u;
            const unsigned alpha = rgba[offset + 3];
            const auto overWhite = [alpha](unsigned component) {
                return static_cast<unsigned char>((component * alpha + 255u * (255u - alpha)) / 255u);
            };
            result.setPixel(x, y,
                            0xff000000u |
                            (static_cast<unsigned>(overWhite(rgba[offset])) << 16) |
                            (static_cast<unsigned>(overWhite(rgba[offset + 1])) << 8) |
                            static_cast<unsigned>(overWhite(rgba[offset + 2])));
        }
    }
    return result;
}
catch (const std::bad_alloc &) { return {}; }
catch (const std::length_error &) { return {}; }
