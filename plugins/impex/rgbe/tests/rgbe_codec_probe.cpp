#include "../rgbe_codec.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

namespace {

void require(bool condition, const char *message)
{
    if (!condition) {
        std::cerr << message << '\n';
        std::exit(1);
    }
}

} // namespace

int main()
{
    RGBE::Resolution resolution;
    require(RGBE::parseResolution("-Y 256 +X 512\n", resolution),
            "canonical Radiance resolution must parse");
    require(resolution.width == 512 && resolution.height == 256,
            "canonical Radiance dimensions must be preserved");
    require(resolution.xIncreasing && !resolution.yIncreasing,
            "canonical Radiance orientation must be preserved");

    require(RGBE::parseResolution("+X 3 -Y 2", resolution),
            "axis order must be accepted");
    require(resolution.width == 3 && resolution.height == 2,
            "axis-order dimensions must be assigned by axis");
    require(!RGBE::parseResolution("-Y 2 +Y 3", resolution),
            "duplicate axes must be rejected");
    require(!RGBE::parseResolution("-Y 0 +X 3", resolution),
            "zero dimensions must be rejected");
    require(!RGBE::parseResolution("-Y 2 +X 3 trailing", resolution),
            "trailing garbage must be rejected");

    const auto white = RGBE::decodePixel(128, 128, 128, 129);
    require(std::fabs(white[0] - 1.0F) < 0.00001F &&
                std::fabs(white[1] - 1.0F) < 0.00001F &&
                std::fabs(white[2] - 1.0F) < 0.00001F,
            "Radiance exponent conversion must preserve 1.0");

    const auto black = RGBE::decodePixel(255, 255, 255, 0);
    require(black[0] == 0.0F && black[1] == 0.0F && black[2] == 0.0F,
            "zero exponent must decode as black");

    std::size_t repeat = 0;
    unsigned shift = 0;
    require(!RGBE::decodeOldRepeat(5, 0, 8, shift, repeat),
            "old-style repeat cannot appear before a produced pixel");
    require(RGBE::decodeOldRepeat(3, 1, 8, shift, repeat) && repeat == 3 && shift == 8,
            "valid old-style repeat must decode and advance the shift");
    shift = 24;
    require(!RGBE::decodeOldRepeat(255, 1, 8, shift, repeat),
            "old-style repeat shift overflow must be rejected");
    bool run = false;
    std::size_t length = 0;
    require(!RGBE::decodeRlePacket(0, 8, run, length) &&
                !RGBE::decodeRlePacket(128, 8, run, length),
            "zero-length RLE packets must be rejected");
    require(RGBE::decodeRlePacket(131, 8, run, length) && run && length == 3,
            "non-empty run packet must decode");

    return 0;
}
