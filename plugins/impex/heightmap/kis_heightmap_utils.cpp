/*
 *  SPDX-FileCopyrightText: 2017 Victor Wåhlström <victor.wahlstrom@initiali.se>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_heightmap_utils.h"

#include <KoColorModelStandardIds.h>
#include <PkAuxTypes.h>

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
