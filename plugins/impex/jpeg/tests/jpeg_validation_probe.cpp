#include "../jpeg_validation.h"

#include <cstdlib>
#include <iostream>
#include <limits>

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
    std::size_t bytes = 0;
    require(jpegCheckedBufferSize(640, 480, 4, bytes) && bytes == 1228800,
            "valid JPEG output dimensions must produce the exact byte count");
    require(!jpegCheckedBufferSize(std::numeric_limits<std::size_t>::max(), 2, 4, bytes),
            "JPEG output allocation multiplication must reject overflow");
    require(jpegMarkerPayloadAvailable(29, 29),
            "JPEG marker parser must accept an exactly sized fixed header");
    require(!jpegMarkerPayloadAvailable(28, 29),
            "truncated JPEG marker headers must be rejected before subtraction");
    return 0;
}
