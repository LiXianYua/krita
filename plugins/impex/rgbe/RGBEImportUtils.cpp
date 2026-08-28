/*
 * SPDX-FileCopyrightText: 2023 Rasyuqa A. H. <qampidh@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 *
 * Based on KImageFormats Radiance HDR loader
 *
 * SPDX-FileCopyrightText: 2005 Christoph Hormann <chris_hormann@gmx.de>
 * SPDX-FileCopyrightText: 2005 Ignacio Castaño <castanyo@yahoo.es>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <cmath>

 #include <PkDataStream.h>
#include <PkStream.h>
#include <kis_sequential_iterator.h>
#include "rgbe_codec.h"

namespace RGBEIMPORT
{
#define MINELEN 8 // minimum scanline length for encoding
#define MAXELEN 0x7fff // maximum scanline length for encoding

// read an old style line from the hdr image file
// if 'first' is true the first byte is already read
bool ReadOldLine(quint8 *image, int width, PkDataStream &s)
{
    unsigned rshift = 0;
    std::size_t produced = 0;
    int i;

    while (width > 0) {
        s >> image[0];
        s >> image[1];
        s >> image[2];
        s >> image[3];
        if (s.status() != PkDataStream::Ok) {
            return false;
        }

        if ((image[0] == 1) && (image[1] == 1) && (image[2] == 1)) {
            std::size_t length = 0;
            if (!RGBE::decodeOldRepeat(image[3], produced, static_cast<std::size_t>(width), rshift, length)) {
                dbgFile << "Broken file detected: cannot duplicate pixels past image bounds!";
                return false;
            }
            for (i = static_cast<int>(length); i > 0; i--) {
                memcpy(image, image-4, 4);
                image += 4;
                width--;
                ++produced;
            }
        } else {
            image += 4;
            width--;
            ++produced;
            rshift = 0;
        }
    }
    return true;
}

void RGBEToPaintDevice(quint8 *image, int width, KisSequentialIterator &it)
{
    for (int j = 0; j < width; j++) {
        it.nextPixel();
        auto *dst = reinterpret_cast<float *>(it.rawData());

        const auto pixelData = RGBE::decodePixel(image[0], image[1], image[2], image[3]);
        memcpy(dst, pixelData.data(), pixelData.size() * sizeof(float));

        image += 4;
    }
}

// Load the HDR image.
bool LoadHDR(PkDataStream &s, PkStream *device, const int width, const int height, KisSequentialIterator &it)
{
    if (!device) {
        return false;
    }
    quint8 val;
    quint8 code;

    PkByteArray lineArray;
    lineArray.resize(4 * width);
    quint8 *image = (quint8 *)lineArray.data();

    for (int cline = 0; cline < height; cline++) {
        // determine scanline type
        if ((width < MINELEN) || (MAXELEN < width)) {
            if (!ReadOldLine(image, width, s)) {
                return false;
            }
            RGBEToPaintDevice(image, width, it);
            continue;
        }

        s >> val;
        if (s.status() != PkDataStream::Ok) {
            return false;
        }

        if (val != 2) {
            device->ungetChar(static_cast<char>(val));
            if (!ReadOldLine(image, width, s)) {
                return false;
            }
            RGBEToPaintDevice(image, width, it);
            continue;
        }

        s >> image[1];
        s >> image[2];
        s >> image[3];
        if (s.status() != PkDataStream::Ok) {
            return false;
        }

        if ((image[1] != 2) || (image[2] & 128)) {
            image[0] = 2;
            if (!ReadOldLine(image + 4, width - 1, s)) {
                return false;
            }
            RGBEToPaintDevice(image, width, it);
            continue;
        }

        if ((image[2] << 8 | image[3]) != width) {
            dbgFile << "Line of pixels had width" << (image[2] << 8 | image[3]) << "instead of" << width;
            return false;
        }

        // read each component
        for (int i = 0; i < 4; i++) {
            for (int j = 0; j < width;) {
                s >> code;
                if (s.status() != PkDataStream::Ok) {
                    dbgFile << "Truncated HDR file";
                    return false;
                }
                bool isRun = false;
                std::size_t packetLength = 0;
                if (!RGBE::decodeRlePacket(code, static_cast<std::size_t>(width - j), isRun, packetLength)) {
                    dbgFile << "Broken file detected: empty or overlong RLE packet";
                    return false;
                }
                if (isRun) {
                    s >> val;
                    if (s.status() != PkDataStream::Ok) {
                        dbgFile << "Truncated HDR run packet";
                        return false;
                    }
                    while (packetLength-- != 0) {
                        image[i + j * 4] = val;
                        j++;
                    }
                } else {
                    while (packetLength-- != 0) {
                        s >> image[i + j * 4];
                        if (s.status() != PkDataStream::Ok) {
                            dbgFile << "Truncated HDR file";
                            return false;
                        }
                        j++;
                    }
                }
            }
        }

        RGBEToPaintDevice(image, width, it);
    }

    return true;
}

} // namespace RGBEIMPORT
