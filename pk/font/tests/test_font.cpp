#include "PkFont.h"
#include "PkFontRasterizer.h"
#include <PkTransform.h>
#include <PkPoint.h>

#include <cmath>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

namespace {

std::uint64_t imageHash(const PkImage &image)
{
    std::uint64_t hash = 1469598103934665603ull;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            hash ^= image.pixel(x, y);
            hash *= 1099511628211ull;
        }
    }
    return hash;
}

// Ink centre of mass in device pixels, using the reported offset so that masks
// of different widths are comparable.
double inkCentreX(const PkFontRasterizer::TextCoverage &coverage)
{
    double weighted = 0;
    double ink = 0;
    for (int y = 0; y < coverage.height; ++y) {
        for (int x = 0; x < coverage.width; ++x) {
            const double weight = coverage.mask[static_cast<std::size_t>(y) * coverage.width + x];
            weighted += weight * (coverage.offsetX + x);
            ink += weight;
        }
    }
    return ink > 0 ? weighted / ink : 0;
}

}

int main()
{
    // QFont 5.15 preserves the style enum in field 6, including Oblique.
    for (int style = 0; style != 3; ++style) {
        const PkString styledWire(("DejaVu Sans,-1,24,5,50," + std::to_string(style) + ",0,0,0,0").c_str());
        PkFont styled;
        if (!styled.fromString(styledWire) || styled.style() != style ||
            styled.italic() != (style != 0) || styled.toString() != styledWire) {
            std::cerr << "FAIL: QFont style wire round trip differs for " << style << '\n';
            return 1;
        }
    }
    const PkString wire("DejaVu Sans,-1,24,5,50,0,0,0,0,0");
    PkFont font;
    if (!font.fromString(wire) || font.family() != "DejaVu Sans" ||
        font.pointSize() != -1 || font.pixelSize() != 24 || font.weight() != 50 ||
        font.italic() || font.toString() != wire) {
        std::cerr << "FAIL: QFont wire format round trip differs\n";
        return 1;
    }

    const PkImage image = PkFontRasterizer::render(PkString("A"), font);
    if (image.size() != PkSize(17, 29) || image.format() != PkImage::Format_ARGB32) {
        std::cerr << "FAIL: native font metrics differ: "
                  << image.width() << 'x' << image.height() << '\n';
        return 1;
    }

    int nonWhite = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const std::uint32_t pixel = image.pixel(x, y);
            if (pixel != 0xffffffffu) ++nonWhite;
        }
    }
    const std::uint64_t hash = imageHash(image);
    if (nonWhite != 137 || hash != 16695919004516461988ull) {
        std::cerr << "FAIL: native glyph pixels differ: nonWhite=" << nonWhite
                  << " hash=" << hash << '\n';
        return 1;
    }

    // coverage(): the unit transform must reproduce render() exactly.  Its
    // offset is the ink box's (minimumX, -ascent) relative to the baseline
    // origin, and mask[i] == 255 - <render()'s grayscale> at the same pixel.
    const PkFontRasterizer::TextCoverage cov =
        PkFontRasterizer::coverage(PkString("A"), font, PkPointF(0, 0), PkTransform());
    PkFontRasterizer::Metrics outlineMetrics;
    PkFontRasterizer::outline(PkString("A"), font, nullptr, &outlineMetrics);
    if (cov.isEmpty() || cov.width != image.width() || cov.height != image.height()) {
        std::cerr << "FAIL: coverage box differs from render: "
                  << cov.width << 'x' << cov.height << " vs "
                  << image.width() << 'x' << image.height() << '\n';
        return 1;
    }
    if (cov.offsetX != 0 || cov.offsetY != -static_cast<int>(outlineMetrics.ascent)) {
        std::cerr << "FAIL: coverage offset is not (minimumX, -ascent): ("
                  << cov.offsetX << ',' << cov.offsetY << ") vs (0, -"
                  << outlineMetrics.ascent << ")\n";
        return 1;
    }
    int maskedInk = 0;
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const unsigned gray = image.pixel(x, y) & 0xffu;
            const unsigned mask = cov.mask[static_cast<std::size_t>(y) * cov.width + x];
            if (mask != 255u - gray) {
                std::cerr << "FAIL: coverage mask differs from render at ("
                          << x << ',' << y << "): mask=" << mask
                          << " gray=" << gray << '\n';
                return 1;
            }
            if (mask != 0) ++maskedInk;
        }
    }
    if (maskedInk != nonWhite) {
        std::cerr << "FAIL: coverage ink count differs: " << maskedInk
                  << " vs " << nonWhite << '\n';
        return 1;
    }
    // The per-glyph FT_Set_Transform path is not cross-checkable on a host whose
    // Qt has no FreeType engine (see pk/font/README.md), so what is pinned here
    // is the anchor and the sub-pixel contract that *are* checked against Qt's
    // sources: the mask anchors on Qt's own blit rounding -- floor for x, round
    // for y (qpaintengine_raster.cpp:2892-2893) -- and the x fraction reaches the
    // rasterizer as the FT_Set_Transform delta (qfontengine_ft.cpp:970), never
    // snapping to the pixel.
    {
        PkTransform scaled;
        scaled.scale(1.75, 1.75);
        const PkFontRasterizer::TextCoverage base =
            PkFontRasterizer::coverage(PkString("A"), font, PkPointF(1.3, 2.75), scaled);
        if (base.isEmpty() || base.width < image.width() || base.height < image.height()) {
            std::cerr << "FAIL: transformed coverage is malformed: "
                      << base.width << 'x' << base.height << '\n';
            return 1;
        }
        // An integer shift of the baseline origin must leave the mask and its
        // offsets byte-identical: they are relative to the snapped origin.
        const PkFontRasterizer::TextCoverage moved =
            PkFontRasterizer::coverage(PkString("A"), font, PkPointF(4.3, 4.75), scaled);
        if (moved.mask != base.mask || moved.width != base.width ||
            moved.height != base.height || moved.offsetX != base.offsetX ||
            moved.offsetY != base.offsetY) {
            std::cerr << "FAIL: transformed coverage is not anchored on the snapped origin\n";
            return 1;
        }
        // A fractional x shift must land at that fraction instead of snapping:
        // the ink's centre of mass follows it.
        const PkFontRasterizer::TextCoverage subpixel =
            PkFontRasterizer::coverage(PkString("A"), font, PkPointF(1.7, 2.75), scaled);
        const double shift = inkCentreX(subpixel) - inkCentreX(base);
        if (std::abs(shift - 0.4) > 0.02) {
            std::cerr << "FAIL: sub-pixel x did not reach the glyph rasterizer: shift="
                      << shift << '\n';
            return 1;
        }
    }

    std::vector<std::thread> workers;
    std::vector<std::uint64_t> hashes(8, 0);
    for (std::size_t i = 0; i < hashes.size(); ++i) {
        workers.emplace_back([&, i] {
            hashes[i] = imageHash(PkFontRasterizer::render(PkString("A"), font));
        });
    }
    for (std::thread &worker : workers) worker.join();
    for (const std::uint64_t threadedHash : hashes) {
        if (threadedHash != hash) {
            std::cerr << "FAIL: concurrent font rasterization differs\n";
            return 1;
        }
    }
    std::cout << "font rasterizer matches Qt 5.15 brush oracle\n";
    return 0;
}
