#include "../tiff_validation.h"

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
    require(tiffCheckedRasterSize(19, 11, 4, 2, bytes) && bytes == 1672,
            "valid TIFF raster dimensions must produce the exact byte count");
    require(!tiffCheckedRasterSize(19, 11, 0, 2, bytes),
            "zero-channel TIFF rasters must be rejected");
    require(!tiffCheckedRasterSize(std::numeric_limits<std::size_t>::max(), 2, 4, 2, bytes),
            "TIFF raster allocation multiplication must reject overflow");
    require(tiffTagPayloadAvailable(12, 12) && !tiffTagPayloadAvailable(11, 12),
            "truncated TIFF/PSD tag payloads must be rejected before reading");
    return 0;
}
