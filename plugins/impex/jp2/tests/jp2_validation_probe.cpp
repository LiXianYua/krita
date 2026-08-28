#include "../jp2_validation.h"

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
    int32_t pixels[4] = {};
    opj_image_comp_t components[3]{};
    for (auto &component : components) {
        component.w = 2;
        component.h = 2;
        component.dx = 1;
        component.dy = 1;
        component.prec = 8;
        component.sgnd = 0;
        component.data = pixels;
    }
    opj_image_t image{};
    image.x1 = 2;
    image.y1 = 2;
    image.numcomps = 3;
    image.comps = components;
    Jp2ValidatedImage result{};
    require(validateJp2Image(image, result) && result.pixelCount == 4,
            "valid decoded JP2 layout must pass");
    image.x1 = 0;
    require(!validateJp2Image(image, result), "zero-width JP2 bounds must fail");
    image.x1 = 2;
    components[1].h = 1;
    require(!validateJp2Image(image, result), "component dimensions must match the canvas");
    components[1].h = 2;
    components[2].data = nullptr;
    require(!validateJp2Image(image, result), "null component data must fail");
    components[2].data = pixels;
    components[2].sgnd = 1;
    require(!validateJp2Image(image, result), "mixed signedness must fail");
    components[2].sgnd = 0;
    components[0].prec = 0;
    require(!validateJp2Image(image, result), "zero precision must fail");
    return 0;
}
