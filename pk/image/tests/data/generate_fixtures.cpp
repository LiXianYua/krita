// Regenerates the native decoder fixtures using the same low-level libraries
// as the production handlers. This utility is not part of the pkimage build.

#include <cstddef>
#include <cstdio>

#include <gif_lib.h>
#include <jpeglib.h>
#include <png.h>
#include <tiffio.h>
#include <webp/encode.h>
#include <zlib.h>

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

namespace
{

bool writeBytes(const std::string &path, const uint8_t *data, std::size_t size)
{
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char *>(data), static_cast<std::streamsize>(size));
    return output.good();
}

std::vector<uint8_t> readBytes(const std::string &path)
{
    std::ifstream input(path, std::ios::binary);
    return std::vector<uint8_t>(std::istreambuf_iterator<char>(input), {});
}

bool writePng(const std::string &path)
{
    const uint8_t rgba[] = {255, 0, 0, 255, 0, 255, 0, 64};
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    image.width = 2;
    image.height = 1;
    image.format = PNG_FORMAT_RGBA;
    return png_image_write_to_file(&image, path.c_str(), 0, rgba, 0, nullptr) != 0;
}

bool writeJpeg(const std::string &path)
{
    FILE *output = std::fopen(path.c_str(), "wb");
    if (!output) {
        return false;
    }

    jpeg_compress_struct codec{};
    jpeg_error_mgr errors{};
    codec.err = jpeg_std_error(&errors);
    jpeg_create_compress(&codec);
    jpeg_stdio_dest(&codec, output);
    codec.image_width = 2;
    codec.image_height = 1;
    codec.input_components = 1;
    codec.in_color_space = JCS_GRAYSCALE;
    jpeg_set_defaults(&codec);
    jpeg_set_quality(&codec, 100, TRUE);
    jpeg_start_compress(&codec, TRUE);
    uint8_t pixels[] = {32, 224};
    JSAMPROW row[] = {pixels};
    jpeg_write_scanlines(&codec, row, 1);
    jpeg_finish_compress(&codec);
    jpeg_destroy_compress(&codec);
    return std::fclose(output) == 0;
}

bool writeTiff(const std::string &path)
{
    TIFF *tiff = TIFFOpen(path.c_str(), "w");
    if (!tiff) {
        return false;
    }
    uint16_t extraSample = EXTRASAMPLE_UNASSALPHA;
    TIFFSetField(tiff, TIFFTAG_IMAGEWIDTH, 2u);
    TIFFSetField(tiff, TIFFTAG_IMAGELENGTH, 1u);
    TIFFSetField(tiff, TIFFTAG_SAMPLESPERPIXEL, 4);
    TIFFSetField(tiff, TIFFTAG_BITSPERSAMPLE, 8);
    TIFFSetField(tiff, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(tiff, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
    TIFFSetField(tiff, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
    TIFFSetField(tiff, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(tiff, TIFFTAG_EXTRASAMPLES, 1, &extraSample);
    uint8_t rgba[] = {255, 0, 0, 255, 0, 0, 255, 128};
    const bool ok = TIFFWriteScanline(tiff, rgba, 0, 0) == 1;
    TIFFClose(tiff);
    return ok;
}

bool writeGif(const std::string &path)
{
    ColorMapObject *colors = GifMakeMapObject(2, nullptr);
    if (!colors) {
        return false;
    }
    colors->Colors[0] = {255, 0, 0};
    colors->Colors[1] = {0, 255, 0};

    int error = 0;
    GifFileType *gif = EGifOpenFileName(path.c_str(), false, &error);
    if (!gif) {
        GifFreeMapObject(colors);
        return false;
    }

    bool ok = EGifPutScreenDesc(gif, 2, 1, 1, 0, colors) != GIF_ERROR;
    GraphicsControlBlock control{};
    control.DisposalMode = DISPOSAL_UNSPECIFIED;
    control.DelayTime = 0;
    control.TransparentColor = 1;
    GifByteType extension[4]{};
    EGifGCBToExtension(&control, extension);
    ok = ok && EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, 4, extension) != GIF_ERROR;
    ok = ok && EGifPutImageDesc(gif, 0, 0, 2, 1, false, nullptr) != GIF_ERROR;
    GifPixelType row[] = {0, 1};
    ok = ok && EGifPutLine(gif, row, 2) != GIF_ERROR;
    ok = EGifCloseFile(gif, &error) != GIF_ERROR && ok;
    GifFreeMapObject(colors);
    return ok;
}

bool writePartialGif(const std::string &path, bool transparent)
{
    ColorMapObject *colors = GifMakeMapObject(4, nullptr);
    if (!colors) {
        return false;
    }
    colors->Colors[0] = {0x12, 0x34, 0x56};
    colors->Colors[1] = {0x44, 0x66, 0x88};
    colors->Colors[2] = {0xEF, 0x10, 0x20};
    colors->Colors[3] = {0, 0, 0};

    int error = 0;
    GifFileType *gif = EGifOpenFileName(path.c_str(), false, &error);
    if (!gif) {
        GifFreeMapObject(colors);
        return false;
    }

    bool ok = EGifPutScreenDesc(gif, 3, 2, 2, 0, colors) != GIF_ERROR;
    if (transparent) {
        GraphicsControlBlock control{};
        control.DisposalMode = DISPOSAL_UNSPECIFIED;
        control.TransparentColor = 1;
        GifByteType extension[4]{};
        EGifGCBToExtension(&control, extension);
        ok = ok && EGifPutExtension(gif, GRAPHICS_EXT_FUNC_CODE, 4, extension) != GIF_ERROR;
    }
    ok = ok && EGifPutImageDesc(gif, 1, 0, 1, 1, false, nullptr) != GIF_ERROR;
    GifPixelType row[] = {2};
    ok = ok && EGifPutLine(gif, row, 1) != GIF_ERROR;
    ok = EGifCloseFile(gif, &error) != GIF_ERROR && ok;
    GifFreeMapObject(colors);
    return ok;
}

bool writeWebP(const std::string &path)
{
    const uint8_t rgba[] = {255, 0, 0, 255, 0, 0, 255, 128};
    uint8_t *encoded = nullptr;
    const std::size_t encodedSize = WebPEncodeLosslessRGBA(rgba, 2, 1, 8, &encoded);
    const bool ok = encodedSize > 0 && writeBytes(path, encoded, encodedSize);
    WebPFree(encoded);
    return ok;
}

bool writeCorruptAndOversize(const std::string &directory, const std::string &stem)
{
    const std::vector<uint8_t> bytes = readBytes(directory + "/valid." + stem);
    if (bytes.empty()) {
        return false;
    }
    const std::size_t truncatedSize = std::max<std::size_t>(1, bytes.size() / 2);
    return writeBytes(directory + "/corrupt." + stem, bytes.data(), truncatedSize);
}

bool writeOversizePng(const std::string &directory)
{
    std::vector<uint8_t> bytes = readBytes(directory + "/valid.png");
    if (bytes.size() < 33) {
        return false;
    }
    const uint32_t huge = 1000000u;
    for (int byte = 0; byte < 4; ++byte) {
        const uint8_t value = static_cast<uint8_t>(huge >> (24 - byte * 8));
        bytes[16 + byte] = value;
        bytes[20 + byte] = value;
    }
    const uint32_t checksum = static_cast<uint32_t>(crc32(0, bytes.data() + 12, 17));
    for (int byte = 0; byte < 4; ++byte) {
        bytes[29 + byte] = static_cast<uint8_t>(checksum >> (24 - byte * 8));
    }
    return writeBytes(directory + "/oversize.png", bytes.data(), bytes.size());
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2) {
        return 2;
    }
    const std::string directory = argv[1];
    bool ok = writePng(directory + "/valid.png");
    ok = writeJpeg(directory + "/valid.jpg") && ok;
    ok = writeTiff(directory + "/valid.tiff") && ok;
    ok = writeGif(directory + "/valid.gif") && ok;
    ok = writePartialGif(directory + "/partial-opaque.gif", false) && ok;
    ok = writePartialGif(directory + "/partial-transparent.gif", true) && ok;
    ok = writeWebP(directory + "/valid.webp") && ok;
    for (const char *extension : {"png", "jpg", "tiff", "gif", "webp"}) {
        ok = writeCorruptAndOversize(directory, extension) && ok;
    }
    ok = writeOversizePng(directory) && ok;
    return ok ? 0 : 1;
}
