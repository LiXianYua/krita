#include "tga_validation.h"

#include <limits>

bool validateTgaHeader(const TgaHeader &head)
{
    const TgaHeaderInfo info(head);
    if (!info.pal && !info.rgb && !info.grey) {
        return false;
    }
    if (info.pal) {
        if (head.colormap_length == 0 || head.colormap_index >= 256 ||
            head.colormap_length > 256 - head.colormap_index ||
            head.colormap_size != 24 || head.colormap_type != 1 || head.pixel_size != 8) {
            return false;
        }
    } else if (head.colormap_type != 0) {
        return false;
    }
    if (head.width == 0 || head.height == 0 ||
        (head.flags & TGA_INTERLEAVE_MASK) != TGA_INTERLEAVE_NONE) {
        return false;
    }
    return (info.grey && head.pixel_size == 8) ||
        (info.rgb && (head.pixel_size == 16 || head.pixel_size == 24 || head.pixel_size == 32)) ||
        info.pal;
}

int tgaDestinationX(const TgaHeader &header, int sourceX)
{
    return (header.flags & TGA_ORIGIN_RIGHT) ? header.width - 1 - sourceX : sourceX;
}

bool validateTgaExportDimensions(int width, int height)
{
    return width > 0 && height > 0 &&
        width <= std::numeric_limits<ushort>::max() &&
        height <= std::numeric_limits<ushort>::max();
}
