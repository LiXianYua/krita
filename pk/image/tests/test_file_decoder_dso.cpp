#include "../PkImageFileDecoder.h"

#include <cstdint>
#include <iostream>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace
{

using RegisterFunction = bool (*)();

struct RegistrarLibrary
{
#if defined(_WIN32)
    HMODULE handle = nullptr;
#else
    void *handle = nullptr;
#endif

    // Registered std::function closures contain code from this library. Keep it
    // loaded for the registry's process lifetime, like the real plugin owner.
    RegisterFunction open(const char *path)
    {
#if defined(_WIN32)
        handle = LoadLibraryA(path);
        return handle ? reinterpret_cast<RegisterFunction>(
                            GetProcAddress(handle, "pkImageTestRegisterDsoHandler"))
                      : nullptr;
#else
        handle = dlopen(path, RTLD_NOW | RTLD_LOCAL);
        return handle ? reinterpret_cast<RegisterFunction>(
                            dlsym(handle, "pkImageTestRegisterDsoHandler"))
                      : nullptr;
#endif
    }
};

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        std::cerr << "FAIL: expected registrar library path\n";
        return 2;
    }
    RegistrarLibrary registrar;
    RegisterFunction registerHandler = registrar.open(argv[1]);
    if (!registerHandler) {
        std::cerr << "FAIL: could not load registrar function\n";
        return 1;
    }
    if (!registerHandler()) {
        std::cerr << "FAIL: registrar rejected its unique handler\n";
        return 1;
    }

    const uint8_t marker[] = {'D', 'S', 'O', '!'};
    const PkImage image = PkImageFileDecoder::decode(marker, sizeof(marker), "marker.dso-marker");
    if (image.isNull() || image.width() != 1 || image.height() != 1 ||
        image.pixel(0, 0) != 0xFF5A17C3u) {
        std::cerr << "FAIL: consumer cannot observe handler registered in separate DSO\n";
        return 1;
    }

    std::cout << "shared registry DSO test passed\n";
    return 0;
}
