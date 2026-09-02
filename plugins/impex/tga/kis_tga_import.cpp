/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisDocument.h>

#include "kis_tga_import.h"

#include "../kis_impex_static_registration.h"
#include <PkImage.h>
#include <PkRgb.h>
#include <PkStream.h>

#include <array>
#include <limits>
#include <vector>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>

#include <kis_paint_device.h>
#include <kis_image.h>
#include <kis_paint_layer.h>
#include <kis_node.h>
#include <kis_group_layer.h>

#include <tga.h>
#include "tga_validation.h"

extern "C" KRITAIMPEX_EXPORT bool registerKisTGAImportFilter()
{
    static bool registered = false;
    return registerKisImpexFilterOnce(
        registered, {PkString("image/x-tga")}, {}, 1,
        []() -> KisImportExportFilter * { return new KisTGAImport(nullptr, PkVariantList()); });
}

KisTGAImport::KisTGAImport(PkObject *parent, const PkVariantList &)
    : KisImportExportFilter(parent)
{
}

KisTGAImport::~KisTGAImport()
{
}

static bool readExact(PkStream *stream, void *destination, std::size_t size)
{
    auto *out = static_cast<char *>(destination);
    std::size_t total = 0;
    while (total < size) {
        const auto count = stream->read(out + total, static_cast<PkStream::pk_int64>(size - total));
        if (count <= 0) {
            return false;
        }
        total += static_cast<std::size_t>(count);
    }
    return true;
}

static bool readHeader(PkStream *stream, TgaHeader &head)
{
    std::array<unsigned char, TgaHeader::SIZE> bytes{};
    if (!readExact(stream, bytes.data(), bytes.size())) {
        return false;
    }
    const auto little16 = [&bytes](std::size_t offset) {
        return static_cast<ushort>(bytes[offset] | (static_cast<ushort>(bytes[offset + 1]) << 8));
    };
    head.id_length = bytes[0];
    head.colormap_type = bytes[1];
    head.image_type = bytes[2];
    head.colormap_index = little16(3);
    head.colormap_length = little16(5);
    head.colormap_size = bytes[7];
    head.x_origin = little16(8);
    head.y_origin = little16(10);
    head.width = little16(12);
    head.height = little16(14);
    head.pixel_size = bytes[16];
    head.flags = bytes[17];
    return true;
}


static bool isSupported(const TgaHeader & head)
{
    return validateTgaHeader(head);
}

static bool loadTGA(PkStream *stream, const TgaHeader & tga, PkImage &img)
{
    // Create image.
    img = PkImage(tga.width, tga.height, PkImage::Format_RGB32);

    TgaHeaderInfo info(tga);

    /**
     * Theoretically, we should check alpha presence via the bits
     * in flags, but there are a lot of files in the wild that
     * have this flag unset. It contradicts TGA specification,
     * but we cannot do anything about it.
     */
    const bool alphaFlag = tga.flags & 0xf;
    if (tga.pixel_size == 32 && !alphaFlag) {
        warnFile << "TGA image with 32-bit pixels reports no alpha channel; decoding alpha bytes anyway";
    }

    if (tga.pixel_size == 32 || tga.pixel_size == 16) {
        img = PkImage(tga.width, tga.height, PkImage::Format_ARGB32);
    }

    const std::size_t pixel_size = tga.pixel_size / 8;
    if (tga.width > std::numeric_limits<std::size_t>::max() / tga.height ||
        static_cast<std::size_t>(tga.width) * tga.height > std::numeric_limits<std::size_t>::max() / pixel_size) {
        return false;
    }
    const std::size_t size = static_cast<std::size_t>(tga.width) * tga.height * pixel_size;

    if (size < 1) {
        dbgFile << "This TGA file is broken with size " << size;
        return false;
    }

    // Read palette.
    std::array<unsigned char, 768> palette{};
    if (info.pal) {
        // @todo Support palettes in other formats!
        if (!readExact(stream, palette.data() + 3 * tga.colormap_index, 3 * tga.colormap_length)) {
            return false;
        }
    }

    // Allocate image.
    std::vector<uchar> storage(size);
    uchar * const image = storage.data();

    if (info.rle) {
        // Decode image.
        char * dst = (char *)image;
        std::size_t num = size;

        while (num > 0) {
            // Get packet header.
            uchar c;
            if (!readExact(stream, &c, 1)) {
                return false;
            }

            std::size_t count = (c & 0x7f) + 1;
            if (count * pixel_size > num) {
                dbgFile << "This TGA file is broken: the number of pixels left to read and the number of RLE pixels do not agree" << ppVar(num) << ppVar(count) << ppVar(pixel_size);
                return false;
            }
            num -= count * pixel_size;

            if (c & 0x80) {
                // RLE pixels.
                KIS_ASSERT(pixel_size <= 8);
                char pixel[8];
                if (!readExact(stream, pixel, pixel_size)) {
                    return false;
                }
                do {
                    memcpy(dst, pixel, pixel_size);
                    dst += pixel_size;
                } while (--count);
            } else {
                // Raw pixels.
                count *= pixel_size;
                if (!readExact(stream, dst, count)) {
                    return false;
                }
                dst += count;
            }
        }
    } else {
        // Read raw image.
        if (!readExact(stream, image, size)) {
            return false;
        }
    }

    // Convert image to internal format.
    int y_start, y_step, y_end;
    if (tga.flags & TGA_ORIGIN_UPPER) {
        y_start = 0;
        y_step = 1;
        y_end = tga.height;
    } else {
        y_start = tga.height - 1;
        y_step = -1;
        y_end = -1;
    }

    uchar* src = image;

    bool hasAlpha = false;
    for (int y = y_start; y != y_end; y += y_step) {
        PkRgb * scanline = (PkRgb *) (void*) img.scanLine(y);

        if (info.pal) {
            // Paletted.
            for (int x = 0; x < tga.width; x++) {
                uchar idx = *src++;
                if (idx < tga.colormap_index || idx >= tga.colormap_index + tga.colormap_length) {
                    return false;
                }
                const int destinationX = tgaDestinationX(tga, x);
                scanline[destinationX] = pkRgb(palette[3 * idx + 2], palette[3 * idx + 1], palette[3 * idx + 0]);
            }
        } else if (info.grey) {
            // Greyscale.
            for (int x = 0; x < tga.width; x++) {
                const int destinationX = tgaDestinationX(tga, x);
                scanline[destinationX] = pkRgb(*src, *src, *src);
                src++;
            }
        } else {
            // True Color.
            if (tga.pixel_size == 16) {
                for (int x = 0; x < tga.width; x++) {
                    const unsigned value = src[0] | (static_cast<unsigned>(src[1]) << 8);
                    const unsigned b = value & 0x1f;
                    const unsigned g = (value >> 5) & 0x1f;
                    const unsigned r = (value >> 10) & 0x1f;
                    const int destinationX = tgaDestinationX(tga, x);
                    scanline[destinationX] = pkRgb((r << 3) | (r >> 2), (g << 3) | (g >> 2), (b << 3) | (b >> 2));
                    src += 2;
                }
            } else if (tga.pixel_size == 24) {
                for (int x = 0; x < tga.width; x++) {
                    const int destinationX = tgaDestinationX(tga, x);
                    scanline[destinationX] = pkRgb(src[2], src[1], src[0]);
                    src += 3;
                }
            } else if (tga.pixel_size == 32) {
                for (int x = 0; x < tga.width; x++) {
                    const uchar alpha = src[3];
                    const int destinationX = tgaDestinationX(tga, x);
                    scanline[destinationX] = pkRgba(src[2], src[1], src[0], alpha);
                    src += 4;
                    hasAlpha |= (alpha > 0);
                }
            }
        }
    }
    /* According to http://www.paulbourke.net/dataformats/tga/
     * Targa 24 images are sometimes stored as Targa 32 images.
     *
     * In case all alpha information is transparent, we convert
     * image to 24 bits.
     */
    if (!hasAlpha && tga.pixel_size == 32) {
        img.convertTo(PkImage::Format_RGB32);
        warnFile << "TGA image has only transparent alpha bytes; importing as RGB";
    }

    return true;
}



KisImportExportErrorCode KisTGAImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    (void)configuration;

    TgaHeader tga;
    if (!readHeader(io, tga) || !io->seek(TgaHeader::SIZE + tga.id_length)) {
        return ImportExportCodes::FileFormatIncorrect;
    }


    // Check image file format.
    if (io->atEnd()) {
        return ImportExportCodes::FileFormatIncorrect;
    }

    // Check supported file types.
    if (!isSupported(tga)) {
        return ImportExportCodes::FormatFeaturesUnsupported;
    }

    PkImage img;
    bool result = loadTGA(io, tga, img);

    if (result == false) {
        return ImportExportCodes::FileFormatIncorrect;
    }

    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(document->createUndoStore(), img.width(), img.height(), colorSpace, "imported from tga");

    KisPaintLayerSP layer = new KisPaintLayer(image, image->nextLayerName(), 255);
    layer->paintDevice()->convertFromQImage(img, 0, 0, 0);
    image->addNode(layer.data(), image->rootLayer().data());

    document->setCurrentImage(image);
    return ImportExportCodes::OK;

}
