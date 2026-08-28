#include "../kis_heightmap_utils.h"

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
    int width = 0;
    int height = 0;
    require(KisHeightmapUtils::resolveDimensions(64, 2, 8, 4, width, height) &&
                width == 8 && height == 4,
            "configured dimensions must be preserved when byte size matches");
    require(!KisHeightmapUtils::resolveDimensions(64, 2, 8, 3, width, height),
            "configured dimensions with mismatched byte size must be rejected");
    require(KisHeightmapUtils::resolveDimensions(128, 2, 0, 0, width, height) &&
                width == 8 && height == 8,
            "missing dimensions may infer an exact square");
    require(!KisHeightmapUtils::resolveDimensions(96, 2, 0, 0, width, height),
            "missing dimensions must not invent an arbitrary factorization");
    require(!KisHeightmapUtils::resolveDimensions(65, 2, 0, 0, width, height),
            "partial pixels must be rejected");
    return 0;
}
