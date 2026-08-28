#include "rgbe_codec.h"

#include <charconv>
#include <cmath>

namespace RGBE
{
namespace
{

void skipSpaces(std::string_view text, std::size_t &position)
{
    while (position < text.size() && (text[position] == ' ' || text[position] == '\t')) {
        ++position;
    }
}

bool readAxis(std::string_view text,
              std::size_t &position,
              char &axis,
              bool &increasing,
              int &extent)
{
    skipSpaces(text, position);
    if (position + 2 > text.size() || (text[position] != '+' && text[position] != '-')) {
        return false;
    }
    increasing = text[position++] == '+';
    axis = text[position++];
    if (axis != 'X' && axis != 'Y') {
        return false;
    }

    if (position >= text.size() || (text[position] != ' ' && text[position] != '\t')) {
        return false;
    }
    skipSpaces(text, position);

    const char *begin = text.data() + position;
    const char *end = text.data() + text.size();
    const auto result = std::from_chars(begin, end, extent);
    if (result.ec != std::errc() || result.ptr == begin || extent <= 0) {
        return false;
    }
    position = static_cast<std::size_t>(result.ptr - text.data());
    return true;
}

} // namespace

bool parseResolution(std::string_view line, Resolution &resolution)
{
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.remove_suffix(1);
    }

    std::size_t position = 0;
    char firstAxis = 0;
    char secondAxis = 0;
    bool firstIncreasing = false;
    bool secondIncreasing = false;
    int firstExtent = 0;
    int secondExtent = 0;

    if (!readAxis(line, position, firstAxis, firstIncreasing, firstExtent) ||
        !readAxis(line, position, secondAxis, secondIncreasing, secondExtent) ||
        firstAxis == secondAxis) {
        return false;
    }
    skipSpaces(line, position);
    if (position != line.size()) {
        return false;
    }

    Resolution parsed;
    parsed.rowMajor = firstAxis == 'Y';
    if (firstAxis == 'X') {
        parsed.width = firstExtent;
        parsed.xIncreasing = firstIncreasing;
        parsed.height = secondExtent;
        parsed.yIncreasing = secondIncreasing;
    } else {
        parsed.height = firstExtent;
        parsed.yIncreasing = firstIncreasing;
        parsed.width = secondExtent;
        parsed.xIncreasing = secondIncreasing;
    }
    resolution = parsed;
    return true;
}

std::array<float, 4> decodePixel(std::uint8_t red,
                                 std::uint8_t green,
                                 std::uint8_t blue,
                                 std::uint8_t exponent)
{
    if (exponent == 0) {
        return {0.0F, 0.0F, 0.0F, 1.0F};
    }
    const float scale = std::ldexp(1.0F, static_cast<int>(exponent) - (128 + 8));
    return {red * scale, green * scale, blue * scale, 1.0F};
}

} // namespace RGBE
