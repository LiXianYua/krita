#pragma once

#include "PkImage.h"
#include "PkImageIoExport.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <string>

struct PKIMAGEIO_EXPORT PkPngReadResult
{
    PkImage image;
    std::map<std::string, std::string> text;
};

class PKIMAGEIO_EXPORT PkPngReader
{
public:
    static PkPngReadResult read(const uint8_t *data, std::size_t size);
};
