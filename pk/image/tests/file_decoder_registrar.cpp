#include "../PkImageFileDecoder.h"

#include <cstdint>

#if defined(_WIN32)
#define PKIMAGE_TEST_EXPORT __declspec(dllexport)
#else
#define PKIMAGE_TEST_EXPORT __attribute__((visibility("default")))
#endif

namespace
{

PkImage markerImage()
{
    PkImage image(1, 1, PkImage::Format_ARGB32);
    image.setPixel(0, 0, 0xFF5A17C3u);
    return image;
}

} // namespace

extern "C" PKIMAGE_TEST_EXPORT bool pkImageTestRegisterDsoHandler()
{
    return PkImageFileDecoder::registerHandler({
        "test.registry.cross-dso",
        200000,
        {"dso-marker"},
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return data && size == 4 && data[0] == 'D' && data[1] == 'S' &&
                   data[2] == 'O' && data[3] == '!';
        },
        [](const uint8_t *, std::size_t, const std::string &) {
            return markerImage();
        }
    });
}
