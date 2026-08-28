#include "../PkImageFileDecoder.h"

#include <cstddef>
#include <cstdio>

#include <gif_lib.h>
#include <jpeglib.h>
#include <png.h>
#include <tiffio.h>
#include <webp/decode.h>

#include <algorithm>
#include <cstdarg>
#include <csetjmp>
#include <cstdint>
#include <cstring>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr std::size_t kMaximumDecodedBytes = 512u * 1024u * 1024u;

uint32_t packArgb(uint8_t alpha, uint8_t red, uint8_t green, uint8_t blue)
{
    return (static_cast<uint32_t>(alpha) << 24) |
           (static_cast<uint32_t>(red) << 16) |
           (static_cast<uint32_t>(green) << 8) |
           static_cast<uint32_t>(blue);
}

bool validArgbDimensions(std::uint64_t width, std::uint64_t height,
                         std::size_t *byteCount = nullptr)
{
    if (width == 0 || height == 0 ||
        width > static_cast<std::uint64_t>(std::numeric_limits<int>::max() / 32) ||
        height > static_cast<std::uint64_t>(std::numeric_limits<int>::max())) {
        return false;
    }
    if (width > std::numeric_limits<std::size_t>::max() / 4u) {
        return false;
    }
    const std::size_t stride = static_cast<std::size_t>(width) * 4u;
    if (height > std::numeric_limits<std::size_t>::max() / stride) {
        return false;
    }
    const std::size_t bytes = stride * static_cast<std::size_t>(height);
    if (bytes > kMaximumDecodedBytes) {
        return false;
    }
    if (byteCount) {
        *byteCount = bytes;
    }
    return true;
}

PkImage makeArgbImage(std::uint64_t width, std::uint64_t height)
{
    if (!validArgbDimensions(width, height)) {
        return PkImage();
    }
    try {
        return PkImage(static_cast<int>(width), static_cast<int>(height),
                       PkImage::Format_ARGB32);
    } catch (const std::bad_alloc &) {
        return PkImage();
    } catch (const std::length_error &) {
        return PkImage();
    }
}

PkImage rgbaToImage(const uint8_t *rgba, std::uint64_t width, std::uint64_t height,
                    std::size_t stride)
{
    PkImage result = makeArgbImage(width, height);
    if (result.isNull()) {
        return result;
    }
    for (std::uint64_t y = 0; y < height; ++y) {
        const uint8_t *row = rgba + static_cast<std::size_t>(y) * stride;
        for (std::uint64_t x = 0; x < width; ++x) {
            const uint8_t *pixel = row + static_cast<std::size_t>(x) * 4u;
            result.setPixel(static_cast<int>(x), static_cast<int>(y),
                            packArgb(pixel[3], pixel[0], pixel[1], pixel[2]));
        }
    }
    return result;
}

bool isPng(const uint8_t *data, std::size_t size)
{
    return size >= 8 && png_sig_cmp(data, 0, 8) == 0;
}

PkImage decodePng(const uint8_t *data, std::size_t size)
{
    png_image codec{};
    codec.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&codec, data, size)) {
        return PkImage();
    }

    std::size_t bytes = 0;
    if (!validArgbDimensions(codec.width, codec.height, &bytes)) {
        png_image_free(&codec);
        return PkImage();
    }
    codec.format = PNG_FORMAT_RGBA;
    try {
        std::vector<uint8_t> rgba(bytes);
        if (!png_image_finish_read(&codec, nullptr, rgba.data(),
                                   static_cast<png_int_32>(codec.width * 4u), nullptr)) {
            png_image_free(&codec);
            return PkImage();
        }
        PkImage result = rgbaToImage(rgba.data(), codec.width, codec.height,
                                     static_cast<std::size_t>(codec.width) * 4u);
        png_image_free(&codec);
        return result;
    } catch (const std::bad_alloc &) {
        png_image_free(&codec);
        return PkImage();
    } catch (const std::length_error &) {
        png_image_free(&codec);
        return PkImage();
    }
}

struct JpegErrorManager
{
    jpeg_error_mgr base;
    std::jmp_buf jump;
};

void jpegErrorExit(j_common_ptr codec)
{
    auto *errors = reinterpret_cast<JpegErrorManager *>(codec->err);
    std::longjmp(errors->jump, 1);
}

void jpegDiscardMessage(j_common_ptr)
{
}

bool isJpeg(const uint8_t *data, std::size_t size)
{
    return size >= 3 && data[0] == 0xFFu && data[1] == 0xD8u && data[2] == 0xFFu;
}

PkImage decodeJpeg(const uint8_t *data, std::size_t size)
{
    if (size > std::numeric_limits<unsigned long>::max()) {
        return PkImage();
    }

    jpeg_decompress_struct codec{};
    JpegErrorManager errors{};
    PkImage result;
    std::vector<uint8_t> row;
    volatile bool codecCreated = false;

    codec.err = jpeg_std_error(&errors.base);
    errors.base.error_exit = jpegErrorExit;
    errors.base.output_message = jpegDiscardMessage;
    if (setjmp(errors.jump)) {
        if (codecCreated) {
            jpeg_destroy_decompress(&codec);
        }
        return PkImage();
    }

    jpeg_create_decompress(&codec);
    codecCreated = true;
    jpeg_mem_src(&codec, data, static_cast<unsigned long>(size));
    if (jpeg_read_header(&codec, TRUE) != JPEG_HEADER_OK) {
        jpeg_destroy_decompress(&codec);
        return PkImage();
    }

    const bool cmyk = codec.jpeg_color_space == JCS_CMYK || codec.jpeg_color_space == JCS_YCCK;
    if (cmyk) {
        codec.out_color_space = JCS_CMYK;
    } else if (codec.jpeg_color_space == JCS_GRAYSCALE) {
        codec.out_color_space = JCS_GRAYSCALE;
    } else {
        codec.out_color_space = JCS_RGB;
    }
    jpeg_start_decompress(&codec);

    if (!validArgbDimensions(codec.output_width, codec.output_height) ||
        (codec.output_components != 1 && codec.output_components != 3 &&
         codec.output_components != 4)) {
        jpeg_abort_decompress(&codec);
        jpeg_destroy_decompress(&codec);
        return PkImage();
    }
    result = makeArgbImage(codec.output_width, codec.output_height);
    if (result.isNull()) {
        jpeg_abort_decompress(&codec);
        jpeg_destroy_decompress(&codec);
        return PkImage();
    }

    try {
        if (codec.output_width > std::numeric_limits<std::size_t>::max() /
                                     codec.output_components) {
            jpeg_abort_decompress(&codec);
            jpeg_destroy_decompress(&codec);
            return PkImage();
        }
        row.resize(static_cast<std::size_t>(codec.output_width) * codec.output_components);
    } catch (const std::bad_alloc &) {
        jpeg_abort_decompress(&codec);
        jpeg_destroy_decompress(&codec);
        return PkImage();
    } catch (const std::length_error &) {
        jpeg_abort_decompress(&codec);
        jpeg_destroy_decompress(&codec);
        return PkImage();
    }

    while (codec.output_scanline < codec.output_height) {
        JSAMPROW rows[] = {row.data()};
        if (jpeg_read_scanlines(&codec, rows, 1) != 1) {
            jpeg_abort_decompress(&codec);
            jpeg_destroy_decompress(&codec);
            return PkImage();
        }
        const int y = static_cast<int>(codec.output_scanline - 1);
        for (JDIMENSION x = 0; x < codec.output_width; ++x) {
            uint8_t red = 0;
            uint8_t green = 0;
            uint8_t blue = 0;
            if (codec.output_components == 1) {
                red = green = blue = row[x];
            } else if (codec.output_components == 3) {
                const uint8_t *pixel = row.data() + static_cast<std::size_t>(x) * 3u;
                red = pixel[0];
                green = pixel[1];
                blue = pixel[2];
            } else {
                const uint8_t *pixel = row.data() + static_cast<std::size_t>(x) * 4u;
                const int key = pixel[3];
                if (codec.saw_Adobe_marker) {
                    red = static_cast<uint8_t>((pixel[0] * key + 127) / 255);
                    green = static_cast<uint8_t>((pixel[1] * key + 127) / 255);
                    blue = static_cast<uint8_t>((pixel[2] * key + 127) / 255);
                } else {
                    red = static_cast<uint8_t>(255 - std::min(255, pixel[0] + key));
                    green = static_cast<uint8_t>(255 - std::min(255, pixel[1] + key));
                    blue = static_cast<uint8_t>(255 - std::min(255, pixel[2] + key));
                }
            }
            result.setPixel(static_cast<int>(x), y, packArgb(255, red, green, blue));
        }
    }

    jpeg_finish_decompress(&codec);
    jpeg_destroy_decompress(&codec);
    return result;
}

struct TiffMemory
{
    const uint8_t *data;
    std::size_t size;
    std::size_t position;
};

tmsize_t tiffRead(thandle_t handle, void *output, tmsize_t requested)
{
    auto *memory = static_cast<TiffMemory *>(handle);
    if (requested <= 0 || memory->position >= memory->size) {
        return 0;
    }
    const std::size_t count = std::min<std::size_t>(
        static_cast<std::size_t>(requested), memory->size - memory->position);
    std::memcpy(output, memory->data + memory->position, count);
    memory->position += count;
    return static_cast<tmsize_t>(count);
}

tmsize_t tiffWrite(thandle_t, void *, tmsize_t)
{
    return 0;
}

toff_t tiffSeek(thandle_t handle, toff_t offset, int origin)
{
    auto *memory = static_cast<TiffMemory *>(handle);
    std::uint64_t base = 0;
    if (origin == SEEK_CUR) {
        base = memory->position;
    } else if (origin == SEEK_END) {
        base = memory->size;
    } else if (origin != SEEK_SET) {
        return static_cast<toff_t>(-1);
    }
    if (offset > std::numeric_limits<std::uint64_t>::max() - base) {
        return static_cast<toff_t>(-1);
    }
    const std::uint64_t position = base + offset;
    if (position > memory->size) {
        return static_cast<toff_t>(-1);
    }
    memory->position = static_cast<std::size_t>(position);
    return static_cast<toff_t>(memory->position);
}

int tiffClose(thandle_t)
{
    return 0;
}

toff_t tiffSize(thandle_t handle)
{
    return static_cast<toff_t>(static_cast<TiffMemory *>(handle)->size);
}

int tiffMap(thandle_t, void **, toff_t *)
{
    return 0;
}

void tiffUnmap(thandle_t, void *, toff_t)
{
}

int tiffDiscardDiagnostic(TIFF *, void *, const char *, const char *, va_list)
{
    return 1;
}

uint8_t unassociateTiffChannel(uint8_t channel, uint8_t alpha)
{
    if (alpha == 0 || alpha == 255) {
        return channel;
    }
    return static_cast<uint8_t>(std::min(255u,
        (static_cast<unsigned>(channel) * 255u + alpha / 2u) / alpha));
}

bool isTiff(const uint8_t *data, std::size_t size)
{
    return size >= 4 &&
           ((data[0] == 'I' && data[1] == 'I' && data[2] == 42 && data[3] == 0) ||
            (data[0] == 'M' && data[1] == 'M' && data[2] == 0 && data[3] == 42));
}

PkImage decodeTiff(const uint8_t *data, std::size_t size)
{
    TiffMemory memory{data, size, 0};
    TIFFOpenOptions *options = TIFFOpenOptionsAlloc();
    if (!options) {
        return PkImage();
    }
    TIFFOpenOptionsSetMaxSingleMemAlloc(options, static_cast<tmsize_t>(kMaximumDecodedBytes));
    TIFFOpenOptionsSetMaxCumulatedMemAlloc(options, static_cast<tmsize_t>(kMaximumDecodedBytes));
    TIFFOpenOptionsSetErrorHandlerExtR(options, tiffDiscardDiagnostic, nullptr);
    TIFFOpenOptionsSetWarningHandlerExtR(options, tiffDiscardDiagnostic, nullptr);
    TIFF *codec = TIFFClientOpenExt("PkImageFileDecoder", "rm", &memory,
                                    tiffRead, tiffWrite, tiffSeek, tiffClose,
                                    tiffSize, tiffMap, tiffUnmap, options);
    TIFFOpenOptionsFree(options);
    if (!codec) {
        return PkImage();
    }

    uint32_t width = 0;
    uint32_t height = 0;
    if (!TIFFGetField(codec, TIFFTAG_IMAGEWIDTH, &width) ||
        !TIFFGetField(codec, TIFFTAG_IMAGELENGTH, &height) ||
        !validArgbDimensions(width, height)) {
        TIFFClose(codec);
        return PkImage();
    }

    try {
        std::vector<uint32_t> raster(static_cast<std::size_t>(width) * height);
        if (!TIFFReadRGBAImageOriented(codec, width, height, raster.data(),
                                       ORIENTATION_TOPLEFT, 0)) {
            TIFFClose(codec);
            return PkImage();
        }
        TIFFClose(codec);

        PkImage result = makeArgbImage(width, height);
        if (result.isNull()) {
            return result;
        }
        for (uint32_t y = 0; y < height; ++y) {
            for (uint32_t x = 0; x < width; ++x) {
                const uint32_t pixel = raster[static_cast<std::size_t>(y) * width + x];
                const uint8_t alpha = TIFFGetA(pixel);
                result.setPixel(static_cast<int>(x), static_cast<int>(y),
                                packArgb(alpha,
                                         unassociateTiffChannel(TIFFGetR(pixel), alpha),
                                         unassociateTiffChannel(TIFFGetG(pixel), alpha),
                                         unassociateTiffChannel(TIFFGetB(pixel), alpha)));
            }
        }
        return result;
    } catch (const std::bad_alloc &) {
        TIFFClose(codec);
        return PkImage();
    } catch (const std::length_error &) {
        TIFFClose(codec);
        return PkImage();
    }
}

struct GifMemory
{
    const uint8_t *data;
    std::size_t size;
    std::size_t position;
};

int gifRead(GifFileType *gif, GifByteType *output, int requested)
{
    auto *memory = static_cast<GifMemory *>(gif->UserData);
    if (requested <= 0 || memory->position >= memory->size) {
        return 0;
    }
    const std::size_t count = std::min<std::size_t>(
        static_cast<std::size_t>(requested), memory->size - memory->position);
    std::memcpy(output, memory->data + memory->position, count);
    memory->position += count;
    return static_cast<int>(count);
}

bool isGif(const uint8_t *data, std::size_t size)
{
    return size >= 6 &&
           (std::memcmp(data, "GIF87a", 6) == 0 || std::memcmp(data, "GIF89a", 6) == 0);
}

PkImage decodeGif(const uint8_t *data, std::size_t size)
{
    GifMemory memory{data, size, 0};
    int error = 0;
    GifFileType *codec = DGifOpen(&memory, gifRead, &error);
    if (!codec || !validArgbDimensions(codec->SWidth, codec->SHeight)) {
        if (codec) {
            DGifCloseFile(codec, &error);
        }
        return PkImage();
    }

    int transparentIndex = NO_TRANSPARENT_COLOR;
    GifRecordType record = UNDEFINED_RECORD_TYPE;
    try {
        while (DGifGetRecordType(codec, &record) != GIF_ERROR &&
               record != TERMINATE_RECORD_TYPE) {
            if (record == EXTENSION_RECORD_TYPE) {
                int extensionCode = 0;
                GifByteType *extension = nullptr;
                if (DGifGetExtension(codec, &extensionCode, &extension) == GIF_ERROR) {
                    break;
                }
                if (extensionCode == GRAPHICS_EXT_FUNC_CODE && extension &&
                    extension[0] >= 4 && (extension[1] & 0x01u)) {
                    transparentIndex = extension[4];
                }
                while (extension) {
                    if (DGifGetExtensionNext(codec, &extension) == GIF_ERROR) {
                        extension = nullptr;
                        record = UNDEFINED_RECORD_TYPE;
                        break;
                    }
                }
                continue;
            }
            if (record != IMAGE_DESC_RECORD_TYPE || DGifGetImageDesc(codec) == GIF_ERROR) {
                continue;
            }

            const GifImageDesc &description = codec->Image;
            if (description.Left < 0 || description.Top < 0 || description.Width <= 0 ||
                description.Height <= 0 ||
                description.Left > codec->SWidth - description.Width ||
                description.Top > codec->SHeight - description.Height ||
                !validArgbDimensions(description.Width, description.Height)) {
                break;
            }
            ColorMapObject *colors = description.ColorMap ? description.ColorMap : codec->SColorMap;
            if (!colors || colors->ColorCount <= 0) {
                break;
            }

            PkImage result = makeArgbImage(codec->SWidth, codec->SHeight);
            if (result.isNull()) {
                break;
            }
            if (description.Left != 0 || description.Top != 0 ||
                description.Width != codec->SWidth ||
                description.Height != codec->SHeight) {
                const int fillIndex = transparentIndex != NO_TRANSPARENT_COLOR
                    ? transparentIndex
                    : codec->SBackGroundColor;
                if (fillIndex >= 0 && fillIndex < colors->ColorCount) {
                    const GifColorType fill = colors->Colors[fillIndex];
                    const uint8_t alpha = fillIndex == transparentIndex ? 0 : 255;
                    result.fill(packArgb(alpha, fill.Red, fill.Green, fill.Blue));
                }
            }
            std::vector<GifPixelType> row(static_cast<std::size_t>(description.Width));
            const int passStarts[] = {0, 4, 2, 1};
            const int passSteps[] = {8, 8, 4, 2};
            const int passes = description.Interlace ? 4 : 1;
            for (int pass = 0; pass < passes; ++pass) {
                const int start = description.Interlace ? passStarts[pass] : 0;
                const int step = description.Interlace ? passSteps[pass] : 1;
                for (int localY = start; localY < description.Height; localY += step) {
                    if (DGifGetLine(codec, row.data(), description.Width) == GIF_ERROR) {
                        DGifCloseFile(codec, &error);
                        return PkImage();
                    }
                    for (int localX = 0; localX < description.Width; ++localX) {
                        const int colorIndex = row[static_cast<std::size_t>(localX)];
                        if (colorIndex < 0 || colorIndex >= colors->ColorCount) {
                            DGifCloseFile(codec, &error);
                            return PkImage();
                        }
                        const GifColorType color = colors->Colors[colorIndex];
                        const uint8_t alpha = colorIndex == transparentIndex ? 0 : 255;
                        result.setPixel(description.Left + localX, description.Top + localY,
                                        packArgb(alpha, color.Red, color.Green, color.Blue));
                    }
                }
            }
            DGifCloseFile(codec, &error);
            return result;
        }
    } catch (const std::bad_alloc &) {
        DGifCloseFile(codec, &error);
        return PkImage();
    } catch (const std::length_error &) {
        DGifCloseFile(codec, &error);
        return PkImage();
    }
    DGifCloseFile(codec, &error);
    return PkImage();
}

bool isWebP(const uint8_t *data, std::size_t size)
{
    return size >= 12 && std::memcmp(data, "RIFF", 4) == 0 &&
           std::memcmp(data + 8, "WEBP", 4) == 0;
}

PkImage decodeWebP(const uint8_t *data, std::size_t size)
{
    WebPBitstreamFeatures features{};
    if (WebPGetFeatures(data, size, &features) != VP8_STATUS_OK ||
        !validArgbDimensions(features.width, features.height)) {
        return PkImage();
    }
    std::size_t bytes = 0;
    validArgbDimensions(features.width, features.height, &bytes);
    try {
        std::vector<uint8_t> rgba(bytes);
        if (!WebPDecodeRGBAInto(data, size, rgba.data(), rgba.size(), features.width * 4)) {
            return PkImage();
        }
        return rgbaToImage(rgba.data(), features.width, features.height,
                           static_cast<std::size_t>(features.width) * 4u);
    } catch (const std::bad_alloc &) {
        return PkImage();
    } catch (const std::length_error &) {
        return PkImage();
    }
}

PkImageFileDecoderHandler handler(
    std::string name, int priority, std::vector<std::string> extensions,
    bool (*sniff)(const uint8_t *, std::size_t),
    PkImage (*decode)(const uint8_t *, std::size_t))
{
    return {
        std::move(name),
        priority,
        std::move(extensions),
        [sniff](const uint8_t *data, std::size_t size, const std::string &) {
            return sniff(data, size);
        },
        [decode](const uint8_t *data, std::size_t size, const std::string &) {
            return decode(data, size);
        }
    };
}

} // namespace

std::vector<PkImageFileDecoderHandler> pkNativeImageCodecHandlers()
{
    std::vector<PkImageFileDecoderHandler> result;
    result.reserve(5);
    result.push_back(handler("native.png", 1000, {"png"}, isPng, decodePng));
    result.push_back(handler("native.jpeg", 1000, {"jpg", "jpeg"}, isJpeg, decodeJpeg));
    result.push_back(handler("native.tiff", 1000, {"tif", "tiff"}, isTiff, decodeTiff));
    result.push_back(handler("native.gif", 1000, {"gif"}, isGif, decodeGif));
    result.push_back(handler("native.webp", 1000, {"webp"}, isWebP, decodeWebP));
    return result;
}
