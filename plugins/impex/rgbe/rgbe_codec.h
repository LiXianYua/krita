#pragma once

#include <array>
#include <cstdint>
#include <string_view>

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

} // namespace RGBE
