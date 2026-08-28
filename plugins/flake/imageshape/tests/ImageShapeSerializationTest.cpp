/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "../ImageShapePngData.h"

int main()
{
    PkImage original(2, 2, PkImage::Format_ARGB32);
    original.setPixel(0, 0, 0xff102030u);
    original.setPixel(1, 0, 0x80405060u);
    original.setPixel(0, 1, 0x00708090u);
    original.setPixel(1, 1, 0xffa0b0c0u);

    const PkString encoded = ImageShapePngData::encodeBase64(original);
    if (encoded.isEmpty()) return 1;

    const PkString dataUri = ImageShapePngData::encodeDataUri(original);
    const PkString dataUriPrefix("data:image/png;base64,");
    if (!dataUri.startsWith(dataUriPrefix)) return 8;
    if (dataUri.mid(dataUriPrefix.size()) != encoded) return 9;

    const PkByteArray png = ImageShapePngData::decodeBase64(encoded);
    if (png.isEmpty()) return 2;

    const PkImage restored = ImageShapePngData::decodePng(png);
    if (restored != original) return 3;

    if (!ImageShapePngData::decodeBase64("not base64").isEmpty()) return 4;
    if (!ImageShapePngData::decodeBase64("AA=A").isEmpty()) return 5;
    if (!ImageShapePngData::decodeBase64("A===").isEmpty()) return 6;
    if (!ImageShapePngData::decodeBase64("AB==").isEmpty()) return 7;
    return 0;
}
