/*
 *  SPDX-FileCopyrightText: 2017 Victor Wåhlström <victor.wahlstrom@initiali.se>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_heightmap_utils.h"

#include <KoColorModelStandardIds.h>
#include <PkAuxTypes.h>

#include <cmath>
#include <limits>

KoID KisHeightmapUtils::mimeTypeToKoID(const PkByteArray& mimeType)
{
    if (mimeType == PkByteArray("image/x-r8", sizeof("image/x-r8") - 1)) {
        return Integer8BitsColorDepthID;
    }
    else if (mimeType == PkByteArray("image/x-r16", sizeof("image/x-r16") - 1)) {
        return Integer16BitsColorDepthID;
    }
    else if (mimeType == PkByteArray("image/x-r32", sizeof("image/x-r32") - 1)) {
        return Float32BitsColorDepthID;
    }
    return KoID();
}

bool KisHeightmapUtils::resolveDimensions(std::uint64_t byteSize,
                                          int pixelSize,
                                          int configuredWidth,
                                          int configuredHeight,
                                          int &width,
                                          int &height)
{
    width = 0;
    height = 0;
    if (pixelSize <= 0 || byteSize == 0 || byteSize % static_cast<unsigned>(pixelSize) != 0) {
        return false;
    }
    const std::uint64_t pixels = byteSize / static_cast<unsigned>(pixelSize);
    if (pixels > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    if (configuredWidth > 0 || configuredHeight > 0) {
        if (configuredWidth <= 0 || configuredHeight <= 0 ||
            static_cast<std::uint64_t>(configuredWidth) > pixels / static_cast<unsigned>(configuredHeight) ||
            static_cast<std::uint64_t>(configuredWidth) * static_cast<unsigned>(configuredHeight) != pixels) {
            return false;
        }
        width = configuredWidth;
        height = configuredHeight;
        return true;
    }

    const auto side = static_cast<std::uint64_t>(std::sqrt(static_cast<long double>(pixels)));
    if (side == 0 || side * side != pixels || side > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    width = static_cast<int>(side);
    height = static_cast<int>(side);
    return true;
}
