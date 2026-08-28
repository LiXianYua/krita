/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "../ImageShapePngData.h"

#ifndef HAVE_JPEG
#define HAVE_JPEG 0
#endif

#if HAVE_JPEG
#include <jpeglib.h>
#endif
#include <tiffio.h>
#include <png.h>
#include <zlib.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <vector>

namespace
{
PkByteArray byteArray(const std::vector<std::uint8_t> &bytes)
{
    return PkByteArray(bytes);
}

#if HAVE_JPEG
std::vector<std::uint8_t> makeJpegFixture(bool progressive = false)
{
    jpeg_compress_struct compressor{};
    jpeg_error_mgr error{};
    compressor.err = jpeg_std_error(&error);
    jpeg_create_compress(&compressor);
    unsigned char *output = nullptr;
    unsigned long outputSize = 0;
    jpeg_mem_dest(&compressor, &output, &outputSize);
    compressor.image_width = 8;
    compressor.image_height = 8;
    compressor.input_components = 3;
    compressor.in_color_space = JCS_RGB;
    jpeg_set_defaults(&compressor);
    if (progressive) jpeg_simple_progression(&compressor);
    jpeg_set_quality(&compressor, 100, TRUE);
    jpeg_start_compress(&compressor, TRUE);
    std::array<JSAMPLE, 8 * 3> row{};
    for (std::size_t i = 0; i < row.size(); i += 3) {
        row[i] = 240;
        row[i + 1] = 16;
        row[i + 2] = 8;
    }
    while (compressor.next_scanline < compressor.image_height) {
        JSAMPROW rows[] = {row.data()};
        jpeg_write_scanlines(&compressor, rows, 1);
    }
    jpeg_finish_compress(&compressor);
    std::vector<std::uint8_t> result(output, output + outputSize);
    std::free(output);
    jpeg_destroy_compress(&compressor);
    return result;
}

std::vector<std::uint8_t> makeCmykJpegFixture(bool ycck = false)
{
    jpeg_compress_struct compressor{};
    jpeg_error_mgr error{};
    compressor.err = jpeg_std_error(&error);
    jpeg_create_compress(&compressor);
    unsigned char *output = nullptr;
    unsigned long outputSize = 0;
    jpeg_mem_dest(&compressor, &output, &outputSize);
    compressor.image_width = 1;
    compressor.image_height = 1;
    compressor.input_components = 4;
    compressor.in_color_space = JCS_CMYK;
    jpeg_set_defaults(&compressor);
    if (ycck) jpeg_set_colorspace(&compressor, JCS_YCCK);
    jpeg_set_quality(&compressor, 100, TRUE);
    jpeg_start_compress(&compressor, TRUE);
    std::array<JSAMPLE, 4> pixel{255, 127, 63, 191};
    JSAMPROW row = pixel.data();
    jpeg_write_scanlines(&compressor, &row, 1);
    jpeg_finish_compress(&compressor);
    std::vector<std::uint8_t> result(output, output + outputSize);
    std::free(output);
    jpeg_destroy_compress(&compressor);
    return result;
}
#endif

struct TiffBuffer
{
    std::vector<std::uint8_t> bytes;
    toff_t offset = 0;
};

tsize_t tiffRead(thandle_t handle, tdata_t data, tsize_t size)
{
    auto &buffer = *static_cast<TiffBuffer *>(handle);
    const std::size_t available = buffer.offset < buffer.bytes.size()
        ? buffer.bytes.size() - static_cast<std::size_t>(buffer.offset) : 0;
    const std::size_t count = std::min<std::size_t>(available, static_cast<std::size_t>(size));
    std::memcpy(data, buffer.bytes.data() + buffer.offset, count);
    buffer.offset += static_cast<toff_t>(count);
    return static_cast<tsize_t>(count);
}

tsize_t tiffWrite(thandle_t handle, tdata_t data, tsize_t size)
{
    auto &buffer = *static_cast<TiffBuffer *>(handle);
    const std::size_t end = static_cast<std::size_t>(buffer.offset) + static_cast<std::size_t>(size);
    if (end > buffer.bytes.size()) buffer.bytes.resize(end);
    std::memcpy(buffer.bytes.data() + buffer.offset, data, static_cast<std::size_t>(size));
    buffer.offset = static_cast<toff_t>(end);
    return size;
}

toff_t tiffSeek(thandle_t handle, toff_t offset, int whence)
{
    auto &buffer = *static_cast<TiffBuffer *>(handle);
    toff_t base = 0;
    if (whence == SEEK_CUR) base = buffer.offset;
    if (whence == SEEK_END) base = static_cast<toff_t>(buffer.bytes.size());
    buffer.offset = base + offset;
    return buffer.offset;
}

toff_t tiffSize(thandle_t handle)
{
    return static_cast<toff_t>(static_cast<TiffBuffer *>(handle)->bytes.size());
}

int tiffClose(thandle_t) { return 0; }
int tiffMap(thandle_t, tdata_t *, toff_t *) { return 0; }
void tiffUnmap(thandle_t, tdata_t, toff_t) {}

std::vector<std::uint8_t> makeTiffFixture(const char *mode = "w")
{
    TiffBuffer buffer;
    TIFF *tiff = TIFFClientOpen("memory", mode, &buffer, tiffRead, tiffWrite,
                                tiffSeek, tiffClose, tiffSize, tiffMap, tiffUnmap);
    if (!tiff) return {};
    TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, 2u);
    TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, 1u);
    TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    std::uint16_t extraSample = EXTRASAMPLE_UNASSALPHA;
    TIFFSetField(tiff, TIFFTAG_EXTRASAMPLES, 1, &extraSample);
    std::array<std::uint8_t, 8> pixels{10, 20, 30, 255, 200, 150, 100, 255};
    if (TIFFWriteScanline(tiff, pixels.data(), 0, 0) < 0) buffer.bytes.clear();
    TIFFClose(tiff);
    return buffer.bytes;
}

std::vector<std::uint8_t> makePngHeader(std::uint32_t width, std::uint32_t height)
{
    std::vector<std::uint8_t> bytes{
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a,
        0, 0, 0, 13, 'I', 'H', 'D', 'R',
        static_cast<std::uint8_t>(width >> 24), static_cast<std::uint8_t>(width >> 16),
        static_cast<std::uint8_t>(width >> 8), static_cast<std::uint8_t>(width),
        static_cast<std::uint8_t>(height >> 24), static_cast<std::uint8_t>(height >> 16),
        static_cast<std::uint8_t>(height >> 8), static_cast<std::uint8_t>(height),
        8, PNG_COLOR_TYPE_RGBA, 0, 0, 0};
    const uLong crc = crc32(0, bytes.data() + 12, 17);
    bytes.push_back(static_cast<std::uint8_t>(crc >> 24));
    bytes.push_back(static_cast<std::uint8_t>(crc >> 16));
    bytes.push_back(static_cast<std::uint8_t>(crc >> 8));
    bytes.push_back(static_cast<std::uint8_t>(crc));
    const std::array<std::uint8_t, 6> idat{'I', 'D', 'A', 'T', 0x78, 0x9c};
    bytes.insert(bytes.end(), {0, 0, 0, 2});
    bytes.insert(bytes.end(), idat.begin(), idat.end());
    const uLong idatCrc = crc32(0, idat.data(), idat.size());
    bytes.push_back(static_cast<std::uint8_t>(idatCrc >> 24));
    bytes.push_back(static_cast<std::uint8_t>(idatCrc >> 16));
    bytes.push_back(static_cast<std::uint8_t>(idatCrc >> 8));
    bytes.push_back(static_cast<std::uint8_t>(idatCrc));
    const std::array<std::uint8_t, 4> iend{'I', 'E', 'N', 'D'};
    bytes.insert(bytes.end(), {0, 0, 0, 0});
    bytes.insert(bytes.end(), iend.begin(), iend.end());
    const uLong iendCrc = crc32(0, iend.data(), iend.size());
    bytes.push_back(static_cast<std::uint8_t>(iendCrc >> 24));
    bytes.push_back(static_cast<std::uint8_t>(iendCrc >> 16));
    bytes.push_back(static_cast<std::uint8_t>(iendCrc >> 8));
    bytes.push_back(static_cast<std::uint8_t>(iendCrc));
    return bytes;
}

int canonicalPngRoundTrip()
{
    PkImage original(2, 2, PkImage::Format_ARGB32);
    original.setPixel(0, 0, 0xff102030u);
    original.setPixel(1, 0, 0x80405060u);
    original.setPixel(0, 1, 0x00708090u);
    original.setPixel(1, 1, 0xffa0b0c0u);
    const PkString encoded = ImageShapePngData::encodeBase64(original);
    if (encoded.isEmpty()) return 1;
    const PkString prefix("data:image/png;base64,");
    const PkString dataUri = ImageShapePngData::encodeDataUri(original);
    if (!dataUri.startsWith(prefix) || dataUri.mid(prefix.size()) != encoded) return 2;
    return ImageShapePngData::decodeImage(ImageShapePngData::decodeBase64(encoded)) == original ? 0 : 3;
}

int dataUriDecodingRejectsMissingMarkerAndMalformedPayload()
{
    PkImage original(1, 1, PkImage::Format_ARGB32);
    original.setPixel(0, 0, 0xff123456u);
    const PkString dataUri = ImageShapePngData::encodeDataUri(original);
    const PkImage restored = ImageShapePngData::decodeImage(
        ImageShapePngData::decodeDataUriBase64(dataUri));
    if (restored.pixel(0, 0) != 0xff123456u) return 4;
    if (!ImageShapePngData::decodeDataUriBase64("data:image/png,AAAA").isEmpty()) return 5;
    if (!ImageShapePngData::decodeDataUriBase64("data:image/png;base64,AB==").isEmpty()) return 6;
    return 0;
}

int sourceFormatsConvertWithoutCorruption()
{
    PkImage premultiplied(1, 1, PkImage::Format_ARGB32_Premultiplied);
    premultiplied.setPixel(0, 0, 0x80800000u);
    if (ImageShapePngData::decodeImage(ImageShapePngData::decodeBase64(
            ImageShapePngData::encodeBase64(premultiplied))).pixel(0, 0) != 0x80ff0000u) return 10;

    PkImage indexed(1, 1, PkImage::Format_Indexed8);
    indexed.setColorTable({0xff000000u, 0xff123456u});
    indexed.setPixel(0, 0, 1);
    if (ImageShapePngData::decodeImage(ImageShapePngData::decodeBase64(
            ImageShapePngData::encodeBase64(indexed))).pixel(0, 0) != 0xff123456u) return 11;

    PkImage gray(1, 1, PkImage::Format_Grayscale8);
    gray.bits()[0] = 91;
    if (ImageShapePngData::decodeImage(ImageShapePngData::decodeBase64(
            ImageShapePngData::encodeBase64(gray))).pixel(0, 0) != 0xff5b5b5bu) return 12;

    PkImage rgb16(1, 1, PkImage::Format_RGB16);
    const std::uint16_t green565 = 0x07e0u;
    std::memcpy(rgb16.bits(), &green565, sizeof(green565));
    if (ImageShapePngData::decodeImage(ImageShapePngData::decodeBase64(
            ImageShapePngData::encodeBase64(rgb16))).pixel(0, 0) != 0xff00ff00u) return 13;

    for (int format = PkImage::Format_Mono; format <= PkImage::Format_BGR888; ++format) {
        PkImage candidate(1, 1, static_cast<PkImage::Format>(format));
        if (candidate.isNull() || ImageShapePngData::encodeBase64(candidate).isEmpty()) return 100 + format;
    }
    return 0;
}

int highDepthPngRemainsHighDepth()
{
    PkImage rgba64(1, 1, PkImage::Format_RGBA64);
    std::array<std::uint16_t, 4> pixel{0x1234u, 0x5678u, 0x9abcu, 0xdef0u};
    std::memcpy(rgba64.bits(), pixel.data(), sizeof(pixel));
    const PkByteArray png = ImageShapePngData::decodeBase64(ImageShapePngData::encodeBase64(rgba64));
    if (png.size() < 25 || static_cast<unsigned char>(png.constData()[24]) != 16u) return 20;

    PkImage gray16(1, 1, PkImage::Format_Grayscale16);
    const std::uint16_t gray = 0x4321u;
    std::memcpy(gray16.bits(), &gray, sizeof(gray));
    const PkByteArray grayPng = ImageShapePngData::decodeBase64(ImageShapePngData::encodeBase64(gray16));
    if (grayPng.size() < 25 || static_cast<unsigned char>(grayPng.constData()[24]) != 16u) return 21;
    return 0;
}

int autoDetectsLegacyFormats()
{
#if HAVE_JPEG
    const PkImage jpeg = ImageShapePngData::decodeImage(byteArray(makeJpegFixture()));
    if (jpeg.width() != 8 || jpeg.height() != 8) return 30;
    const std::uint32_t jpegPixel = jpeg.pixel(3, 3);
    if (((jpegPixel >> 16) & 0xffu) < 200u || ((jpegPixel >> 8) & 0xffu) > 50u ||
        (jpegPixel & 0xffu) > 50u) return 31;

    const PkImage cmyk = ImageShapePngData::decodeImage(byteArray(makeCmykJpegFixture()));
    if (cmyk.width() != 1 || cmyk.height() != 1 || cmyk.pixel(0, 0) != 0xffbf5f2fu) return 32;
    const PkImage ycck = ImageShapePngData::decodeImage(byteArray(makeCmykJpegFixture(true)));
    if (ycck.width() != 1 || ycck.height() != 1) return 36;
    const std::uint32_t ycckPixel = ycck.pixel(0, 0);
    if (std::abs(int((ycckPixel >> 16) & 0xffu) - 191) > 1 ||
        std::abs(int((ycckPixel >> 8) & 0xffu) - 95) > 1 ||
        std::abs(int(ycckPixel & 0xffu) - 46) > 1) return 37;
#else
    const std::vector<std::uint8_t> jpegMarker{0xff, 0xd8, 0xff, 0xe0, 0x00, 0x02};
    if (!ImageShapePngData::decodeImage(byteArray(jpegMarker)).isNull()) return 32;
#endif

    const PkImage tiff = ImageShapePngData::decodeImage(byteArray(makeTiffFixture()));
    if (tiff.width() != 2 || tiff.height() != 1) return 32;
    if (tiff.pixel(0, 0) != 0xff0a141eu || tiff.pixel(1, 0) != 0xffc89664u) return 33;
    const std::vector<std::uint8_t> bigTiffBytes = makeTiffFixture("w8");
    if (bigTiffBytes.size() < 4 || bigTiffBytes[2] != 43u) return 34;
    const PkImage bigTiff = ImageShapePngData::decodeImage(byteArray(bigTiffBytes));
    if (bigTiff.width() != 2 || bigTiff.height() != 1 || bigTiff.pixel(0, 0) != 0xff0a141eu) return 35;
    return 0;
}

int decoderBoundariesAndCorruptInputsAreRejected()
{
    PkImage exact(4, 1, PkImage::Format_ARGB32);
    exact.fill(0xff123456u);
    const PkByteArray exactPng = ImageShapePngData::decodeBase64(ImageShapePngData::encodeBase64(exact));
    ImageShapePngData::DecodeError error = ImageShapePngData::DecodeError::None;
    const PkImage accepted = ImageShapePngData::decodeImageForTesting(exactPng, 16u, &error);
    if (accepted.width() != 4 || error != ImageShapePngData::DecodeError::None) return 50;

    PkImage oneOver(5, 1, PkImage::Format_ARGB32);
    const PkByteArray oneOverPng = ImageShapePngData::decodeBase64(ImageShapePngData::encodeBase64(oneOver));
    if (!ImageShapePngData::decodeImageForTesting(oneOverPng, 16u, &error).isNull() ||
        error != ImageShapePngData::DecodeError::SizeLimit) return 51;

    const PkByteArray strideOverflow = byteArray(makePngHeader(67108864u, 1u));
    if (!ImageShapePngData::decodeImageForTesting(strideOverflow,
            ImageShapePngData::maxDecodedPixelBytes(), &error).isNull() ||
        error != ImageShapePngData::DecodeError::Dimensions) return 52;

    std::vector<std::uint8_t> truncatedPng(
        reinterpret_cast<const std::uint8_t *>(exactPng.constData()),
        reinterpret_cast<const std::uint8_t *>(exactPng.constData()) + exactPng.size() / 2);
    if (!ImageShapePngData::decodeImage(byteArray(truncatedPng)).isNull()) return 53;
    std::vector<std::uint8_t> corruptHeader(
        reinterpret_cast<const std::uint8_t *>(exactPng.constData()),
        reinterpret_cast<const std::uint8_t *>(exactPng.constData()) + exactPng.size());
    corruptHeader[29] ^= 0x01u;
    if (!ImageShapePngData::decodeImage(byteArray(corruptHeader)).isNull()) return 55;

#if HAVE_JPEG
    ImageShapePngData::resetJpegStartCountForTesting();
    ImageShapePngData::DecodeError jpegError = ImageShapePngData::DecodeError::None;
    const PkImage rejectedProgressive = ImageShapePngData::decodeImageForTesting(
        byteArray(makeJpegFixture(true)), 16u, &jpegError);
    if (!rejectedProgressive.isNull() || jpegError != ImageShapePngData::DecodeError::SizeLimit ||
        ImageShapePngData::jpegStartCountForTesting() != 0) return 56;
    if (ImageShapePngData::decodeImage(byteArray(makeJpegFixture(true))).isNull()) return 57;
    std::vector<std::uint8_t> truncatedJpeg = makeJpegFixture();
    truncatedJpeg.resize(truncatedJpeg.size() / 2);
    if (!ImageShapePngData::decodeImage(byteArray(truncatedJpeg)).isNull()) return 54;
#endif
    return 0;
}

int signaturesAndWhitespaceMatchTheLegacyLoader()
{
    const PkByteArray spaced = ImageShapePngData::decodeBase64("T W\nE=");
    if (spaced.size() != 2 || spaced.constData()[0] != 'M' || spaced.constData()[1] != 'a') return 60;
    const std::vector<std::uint8_t> noise{0xff, 0xd8};
    if (ImageShapePngData::hasJpegSignatureForTesting(byteArray(noise))) return 61;
    const std::vector<std::uint8_t> marker{0xff, 0xd8, 0xff, 0xe0, 0x00, 0x02};
    if (!ImageShapePngData::hasJpegSignatureForTesting(byteArray(marker))) return 62;
    const std::vector<std::uint8_t> tem{0xff, 0xd8, 0xff, 0x01, 0xff, 0xe0, 0x00, 0x02};
    if (!ImageShapePngData::hasJpegSignatureForTesting(byteArray(tem))) return 63;
    return 0;
}

int base64LimitsAreCheckedBeforeAllocation()
{
    std::size_t decoded = 0;
    const std::size_t limit = ImageShapePngData::maxDecodedCompressedBytes();
    const std::size_t encodedAtLimit = ((limit + 2u) / 3u) * 4u;
    const std::size_t padding = (3u - limit % 3u) % 3u;
    if (!ImageShapePngData::base64DecodedSizeWithinLimit(encodedAtLimit, padding, decoded)) return 40;
    if (decoded != limit) return 41;
    if (ImageShapePngData::base64DecodedSizeWithinLimit(encodedAtLimit + 4u, 0, decoded)) return 42;
    if (ImageShapePngData::base64DecodedSizeWithinLimit(
            std::numeric_limits<std::size_t>::max() - 2u, 0, decoded)) return 43;
    if (!ImageShapePngData::decodeBase64("not base64").isEmpty()) return 44;
    if (!ImageShapePngData::decodeBase64("AA=A").isEmpty()) return 45;
    if (!ImageShapePngData::decodeBase64("A===").isEmpty()) return 46;
    if (!ImageShapePngData::decodeBase64("AB==").isEmpty()) return 47;
    return 0;
}
} // namespace

int main()
{
    if (const int result = canonicalPngRoundTrip()) return result;
    if (const int result = dataUriDecodingRejectsMissingMarkerAndMalformedPayload()) return result;
    if (const int result = sourceFormatsConvertWithoutCorruption()) return result;
    if (const int result = highDepthPngRemainsHighDepth()) return result;
    if (const int result = autoDetectsLegacyFormats()) return result;
    if (const int result = decoderBoundariesAndCorruptInputsAreRejected()) return result;
    if (const int result = signaturesAndWhitespaceMatchTheLegacyLoader()) return result;
    return base64LimitsAreCheckedBeforeAllocation();
}
