#include "PkFont.h"
#include "PkFontRasterizer.h"

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
