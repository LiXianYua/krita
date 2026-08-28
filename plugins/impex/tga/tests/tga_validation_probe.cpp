#include "../tga_validation.h"

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
}

int main()
{
    TgaHeader header{};
    header.image_type = TGA_TYPE_INDEXED;
    header.colormap_type = 1;
    header.colormap_index = 250;
    header.colormap_length = 7;
    header.colormap_size = 24;
    header.width = 2;
    header.height = 1;
    header.pixel_size = 8;
    require(!validateTgaHeader(header), "palette index plus length must fit the 8-bit index range");
    header.colormap_length = 6;
    require(validateTgaHeader(header), "valid nonzero palette offset must be accepted");
    header.flags = TGA_ORIGIN_RIGHT;
    require(tgaDestinationX(header, 0) == 1 && tgaDestinationX(header, 1) == 0,
            "right-origin rows must be mirrored into display coordinates");
    header.image_type = TGA_TYPE_GREY;
    header.colormap_type = 0;
    header.pixel_size = 16;
    require(!validateTgaHeader(header), "unsupported 16-bit grayscale must be rejected truthfully");
    require(!validateTgaExportDimensions(65536, 1), "TGA export dimensions must fit uint16");
    return 0;
}
