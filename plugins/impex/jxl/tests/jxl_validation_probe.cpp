#include "../jxl_validation.h"

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
    require(jxlCheckedImageBufferSize(17, 13, 4, 2, bytes) && bytes == 1768,
            "valid JXL dimensions must produce the exact interleaved byte count");
    require(!jxlCheckedImageBufferSize(0, 13, 4, 2, bytes),
            "zero-width JXL images must be rejected");
    require(!jxlCheckedImageBufferSize(std::numeric_limits<std::size_t>::max(), 2, 4, 2, bytes),
            "JXL decoded image allocation multiplication must reject overflow");
    require(!jxlInputMayGrow(64, 63),
            "a JXL reader must not wait for bytes beyond a known truncated input");
    return 0;
}
