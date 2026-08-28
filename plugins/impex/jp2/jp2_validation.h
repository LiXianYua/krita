#pragma once

#include <openjpeg.h>

#include <cstddef>
#include <cstdint>

struct Jp2ValidatedImage
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
    std::size_t pixelCount = 0;
    std::uint32_t precision = 0;
    bool isSigned = false;
};

bool validateJp2Image(const opj_image_t &image, Jp2ValidatedImage &result);
