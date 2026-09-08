#pragma once

#include "PkImage.h"
#include "PkImageIoExport.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

struct PkImageFileDecoderHandler
{
    std::string name;
    int priority;
    std::vector<std::string> extensions;
    std::function<bool(const uint8_t *, std::size_t, const std::string &)> canDecode;
    std::function<PkImage(const uint8_t *, std::size_t, const std::string &)> decode;
    // Optional format-preserving result for a painter. The normal decoder
    // contract may normalize storage; alpha-channel identity affects sampling.
    std::function<PkImage(const uint8_t *, std::size_t, const std::string &)> decodeForPainting;
};

class PKIMAGEIO_EXPORT PkImageFileDecoder
{
public:
    static bool registerHandler(PkImageFileDecoderHandler handler);
    static PkImage decode(const uint8_t *data, std::size_t size,
                          const std::string &pathHint = {});
    static PkImage load(const std::string &path);
    static PkImage decodeForPainting(const uint8_t *data, std::size_t size,
                                    const std::string &pathHint = {});
    static PkImage loadForPainting(const std::string &path);
    static std::vector<std::string> supportedExtensions();
};
