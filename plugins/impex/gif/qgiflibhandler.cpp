/*
 * SPDX-FileCopyrightText: 2009 Shawn T. Rutledge (shawn.t.rutledge@gmail.com)
 * SPDX-FileCopyrightText: 2018 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "qgiflibhandler.h"

#include <PkRgb.h>
#include <gif_lib.h>

#include <cstring>
#include <memory>

namespace
{

int writeCallback(GifFileType *gif, const GifByteType *data, int count)
{
    auto *stream = static_cast<PkStream *>(gif->UserData);
    const auto written = stream->write(reinterpret_cast<const char *>(data), count);
    return written > 0 ? static_cast<int>(written) : 0;
}

int readCallback(GifFileType *gif, GifByteType *data, int count)
{
    auto *stream = static_cast<PkStream *>(gif->UserData);
    const auto read = stream->read(reinterpret_cast<char *>(data), count);
    return read > 0 ? static_cast<int>(read) : 0;
}

struct GifReadCloser
{
    void operator()(GifFileType *gif) const
    {
        if (gif) {
            int error = 0;
            DGifCloseFile(gif, &error);
        }
    }
};

struct GifWriteCloser
{
    void operator()(GifFileType *gif) const
    {
        if (gif) {
            int error = 0;
            EGifCloseFile(gif, &error);
        }
    }
};

constexpr int interlacedOffset[] = {0, 4, 2, 1};
constexpr int interlacedJump[] = {8, 8, 4, 2};

} // namespace

GifLibCodec::GifLibCodec(PkStream *device)
    : m_device(device)
{
}

bool GifLibCodec::canRead() const
{
    return canRead(m_device);
}

bool GifLibCodec::read(PkImage *image)
{
    if (!image || !m_device || !m_device->isReadable()) {
        return false;
    }

    int error = 0;
    std::unique_ptr<GifFileType, GifReadCloser> gif(DGifOpen(m_device, readCallback, &error));
    if (!gif || gif->SWidth <= 0 || gif->SHeight <= 0) {
        return false;
    }

    *image = PkImage(gif->SWidth, gif->SHeight, PkImage::Format_Indexed8);
    image->fill(static_cast<std::uint32_t>(gif->SBackGroundColor));

    int transparentColor = -1;
    ColorMapObject *activeColorMap = gif->SColorMap;
    bool foundImage = false;
    GifRecordType recordType = UNDEFINED_RECORD_TYPE;

    do {
        if (DGifGetRecordType(gif.get(), &recordType) == GIF_ERROR) {
            return false;
        }

        if (recordType == EXTENSION_RECORD_TYPE) {
            int extensionCode = 0;
            GifByteType *extension = nullptr;
            if (DGifGetExtension(gif.get(), &extensionCode, &extension) == GIF_ERROR) {
                return false;
            }
            while (extension) {
                if (extensionCode == GRAPHICS_EXT_FUNC_CODE && extension[0] >= 4 && (extension[1] & 0x01)) {
                    transparentColor = extension[4];
                }
                if (DGifGetExtensionNext(gif.get(), &extension) == GIF_ERROR) {
                    return false;
                }
            }
            continue;
        }

        if (recordType != IMAGE_DESC_RECORD_TYPE) {
            continue;
        }
        if (foundImage || DGifGetImageDesc(gif.get()) == GIF_ERROR) {
            return false;
        }

        const int left = gif->Image.Left;
        const int top = gif->Image.Top;
        const int width = gif->Image.Width;
        const int height = gif->Image.Height;
        if (left < 0 || top < 0 || width <= 0 || height <= 0 ||
            left + width > gif->SWidth || top + height > gif->SHeight) {
            return false;
        }
        activeColorMap = gif->Image.ColorMap ? gif->Image.ColorMap : gif->SColorMap;

        if (gif->Image.Interlace) {
            for (int pass = 0; pass < 4; ++pass) {
                for (int row = interlacedOffset[pass]; row < height; row += interlacedJump[pass]) {
                    if (DGifGetLine(gif.get(), image->scanLine(top + row) + left, width) == GIF_ERROR) {
                        return false;
                    }
                }
            }
        } else {
            for (int row = 0; row < height; ++row) {
                if (DGifGetLine(gif.get(), image->scanLine(top + row) + left, width) == GIF_ERROR) {
                    return false;
                }
            }
        }
        foundImage = true;
    } while (recordType != TERMINATE_RECORD_TYPE);

    if (!foundImage || !activeColorMap) {
        return false;
    }
    image->setColorCount(activeColorMap->ColorCount);
    for (int index = 0; index < activeColorMap->ColorCount; ++index) {
        const GifColorType &color = activeColorMap->Colors[index];
        image->setColor(index, pkRgba(color.Red, color.Green, color.Blue,
                                     index == transparentColor ? 0 : 255));
    }
    return true;
}

bool GifLibCodec::canRead(PkStream *device)
{
    if (!device || !device->isReadable()) {
        return false;
    }
    char header[6] = {};
    return device->peek(header, sizeof(header)) == sizeof(header) &&
        (std::memcmp(header, "GIF87a", sizeof(header)) == 0 ||
         std::memcmp(header, "GIF89a", sizeof(header)) == 0);
}

bool GifLibCodec::write(const PkImage &image)
{
    if (!m_device || !m_device->isWritable() || image.isNull()) {
        return false;
    }

    PkImage indexed;
    if (image.colorCount() > 0 && image.colorCount() <= 256) {
        indexed = image;
    } else {
        indexed = PkImage(image.width(), image.height(), PkImage::Format_Indexed8);
        std::vector<PkRgb> palette(256, 0);
        palette[0] = pkRgba(0, 0, 0, 0);
        palette[1] = pkRgba(0, 0, 0, 255);
        for (int bucket = 2; bucket < 256; ++bucket) {
            const int red = ((bucket >> 5) & 0x07) * 255 / 7;
            const int green = ((bucket >> 2) & 0x07) * 255 / 7;
            const int blue = (bucket & 0x03) * 255 / 3;
            palette[bucket] = pkRgba(red, green, blue, 255);
        }
        indexed.setColorTable(palette);
        for (int y = 0; y < image.height(); ++y) {
            for (int x = 0; x < image.width(); ++x) {
                const PkRgb color = image.pixel(x, y);
                int index = 0;
                if (pkAlpha(color) >= 128) {
                    const int bucket = ((pkRed(color) >> 5) << 5) |
                        ((pkGreen(color) >> 5) << 2) | (pkBlue(color) >> 6);
                    index = bucket == 0 ? 1 : bucket;
                }
                indexed.setPixel(x, y, static_cast<std::uint32_t>(index));
            }
        }
    }
    const auto colorTable = indexed.colorTable();
    if (colorTable.empty() || colorTable.size() > 256) {
        return false;
    }

    int colorCount = 2;
    while (colorCount < static_cast<int>(colorTable.size())) {
        colorCount <<= 1;
    }
    std::unique_ptr<ColorMapObject, decltype(&GifFreeMapObject)> colorMap(
        GifMakeMapObject(colorCount, nullptr), &GifFreeMapObject);
    if (!colorMap) {
        return false;
    }
    for (int index = 0; index < colorCount; ++index) {
        const PkRgb color = index < static_cast<int>(colorTable.size()) ? colorTable[index] : 0;
        colorMap->Colors[index].Red = pkRed(color);
        colorMap->Colors[index].Green = pkGreen(color);
        colorMap->Colors[index].Blue = pkBlue(color);
    }

    int error = 0;
    std::unique_ptr<GifFileType, GifWriteCloser> gif(EGifOpen(m_device, writeCallback, &error));
    if (!gif ||
        EGifPutScreenDesc(gif.get(), indexed.width(), indexed.height(),
                          GifBitSize(colorCount), 0, colorMap.get()) == GIF_ERROR) {
        return false;
    }

    int transparentIndex = NO_TRANSPARENT_COLOR;
    for (int index = 0; index < static_cast<int>(colorTable.size()); ++index) {
        if (pkAlpha(colorTable[index]) < 128) {
            transparentIndex = index;
            break;
        }
    }
    if (transparentIndex != NO_TRANSPARENT_COLOR) {
        GraphicsControlBlock control = {DISPOSAL_UNSPECIFIED, false, 0, transparentIndex};
        GifByteType extension[4] = {};
        EGifGCBToExtension(&control, extension);
        if (EGifPutExtension(gif.get(), GRAPHICS_EXT_FUNC_CODE, sizeof(extension), extension) == GIF_ERROR) {
            return false;
        }
    }
    if (EGifPutImageDesc(gif.get(), 0, 0, indexed.width(), indexed.height(),
                         false, nullptr) == GIF_ERROR) {
        return false;
    }

    for (int row = 0; row < indexed.height(); ++row) {
        auto *line = const_cast<GifPixelType *>(
            reinterpret_cast<const GifPixelType *>(indexed.constScanLine(row)));
        if (EGifPutLine(gif.get(), line, indexed.width()) == GIF_ERROR) {
            return false;
        }
    }
    return true;
}
