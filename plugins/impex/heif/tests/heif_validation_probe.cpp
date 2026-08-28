#include "../heif_validation.h"
#include "../heif_chroma_dispatch.h"

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
    require(heifCheckedPlaneSize(32, 24, 128, bytes) && bytes == 3072,
            "valid HEIF plane stride must produce the exact byte count");
    require(!heifCheckedPlaneSize(128, 24, 127, bytes),
            "HEIF plane stride smaller than the row payload must be rejected");
    require(!heifCheckedPlaneSize(32, std::numeric_limits<std::size_t>::max(), 128, bytes),
            "HEIF plane allocation multiplication must reject overflow");
    require(heifReaderTargetStatus(128, 128) == HeifReaderTargetStatus::Reached &&
                heifReaderTargetStatus(129, 128) == HeifReaderTargetStatus::BeyondEof,
            "HEIF truncated readers must report beyond EOF without waiting");
    require(heifInterleavedLayout(heif_chroma_interleaved_RRGGBBAA_BE) ==
                HeifInterleavedLayout::HdrRgba,
            "HEIF big-endian HDR RGBA must dispatch to the HDR RGBA reader");
    require(heifInterleavedLayout(heif_chroma_undefined) ==
                HeifInterleavedLayout::Unsupported,
            "unknown HEIF chroma must be rejected instead of producing a blank layer");
    return 0;
}
