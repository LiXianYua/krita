#pragma once

#include <array>
#include <cstdint>
#include <string_view>
#include <cstddef>

namespace RGBE
{

struct Resolution
{
    int width = 0;
    int height = 0;
    bool xIncreasing = true;
    bool yIncreasing = false;
    bool rowMajor = true;
};

bool parseResolution(std::string_view line, Resolution &resolution);

std::array<float, 4> decodePixel(std::uint8_t red,
                                 std::uint8_t green,
                                 std::uint8_t blue,
                                 std::uint8_t exponent);

bool decodeOldRepeat(std::uint8_t marker,
                     std::size_t produced,
                     std::size_t remaining,
                     unsigned &shift,
                     std::size_t &length);

bool decodeRlePacket(std::uint8_t code,
                     std::size_t remaining,
                     bool &run,
                     std::size_t &length);

} // namespace RGBE
