/*  This file is part of the KDE project

    SPDX-FileCopyrightText: 2000 Matthias Elter <elter@kde.org>
    SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>

    SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <PkXmlCompat.h>
#include <png.h>

#include <resources/KoPattern.h>

#include <sys/types.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <limits>
#include <limits.h>
#include <stdlib.h>
#include <vector>

#include <PkMimeDatabase.h>
#include <PkMemoryStream.h>
#include <PkRgb.h>

#include <DebugPigment.h>
#include <kis_pointer_utils.h>

namespace
{
struct GimpPatternHeader {
    quint32 header_size;  /*  header_size = sizeof (PatternHeader) + brush name  */
    quint32 version;      /*  pattern file version #  */
    quint32 width;        /*  width of pattern */
    quint32 height;       /*  height of pattern  */
    quint32 bytes;        /*  depth of pattern in bytes : 1, 2, 3 or 4*/
    quint32 magic_number; /*  GIMP brush magic number  */
};

// Yes! This is _NOT_ what my pat.txt file says. It's really not 'GIMP', but 'GPAT'
quint32 const GimpPatternMagic = (('G' << 24) + ('P' << 16) + ('A' << 8) + ('T' << 0));

// QtEndian 的 qFromBigEndian/qToBigEndian 等价物（本机为小端，直接字节交换）。
inline quint32 qFromBigEndian(quint32 v) { return __builtin_bswap32(v); }
inline quint32 qToBigEndian(quint32 v) { return __builtin_bswap32(v); }

// 已知图像 mime 白名单（对齐 Qt 位图读入器支持的常见格式）。
bool isSupportedImageMime(const PkString &mime)
{
    return mime == "image/png" || mime == "image/jpeg" || mime == "image/gif"
        || mime == "image/bmp" || mime == "image/tiff" || mime == "image/webp";
}

// 从文件名取后缀并大写（去点），对齐 Qt 的文件信息类 suffix().toUpper()。
PkString fileExtensionUpper(const PkString &filename)
{
    std::filesystem::path p(filename.PkToUtf8());
    std::string ext = p.extension().u8string();
    if (!ext.empty() && ext[0] == '.') {
        ext = ext.substr(1);
    }
    return PkString::PkFromUtf8(ext.data(), static_cast<int>(ext.size())).toUpper();
}

// readAllFromStream：PkStream::readAll()（PkByteArray 形态）在 pk/port 刻意声明
// 不定义（R-12），这里用 char* read() 循环读全——与 PkTextStream.cpp 构造器同款
// 模式，字节等价。
PkByteArray readAllFromStream(PkStream *dev)
{
    std::vector<uint8_t> buf;
    char chunk[8192];
    PkStream::pk_int64 n = 0;
    while ((n = dev->read(chunk, static_cast<PkStream::pk_int64>(sizeof(chunk)))) > 0) {
        buf.insert(buf.end(), chunk, chunk + n);
    }
    return PkByteArray(buf);
}

// R-15 图像编解码：PkImage 没有 load()/save()（pk/image 已知缺口，岔路 A），非
// GPAT 图像（PNG 等）的 load/save 用 libpng 生产/消费 PkImage 裸 buffer。转换
// 逻辑照 libs/resources/KisResourceThumbnailCodec.cpp（同一份 RGBA↔ARGB32 语义，
// 改那边时同步这里）。GPAT 路径完整保留；JPEG 等其它 mime 是 R-15 已知缺口，
// 登记不实现（未引入 libjpeg），消费方保持 loadFromDevice/saveToDevice 不变。

constexpr std::size_t kMaxDecodedRgbaBytes = 256u * 1024u * 1024u;

struct RgbaPixel {
    png_byte red = 0;
    png_byte green = 0;
    png_byte blue = 0;
    png_byte alpha = 0;
};

uint16_t read16(const uint8_t *source)
{
    uint16_t value = 0;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

uint32_t read32(const uint8_t *source)
{
    uint32_t value = 0;
    std::memcpy(&value, source, sizeof(value));
    return value;
}

png_byte expand4(uint32_t value)
{
    return static_cast<png_byte>((value << 4) | value);
}

png_byte expand5(uint32_t value)
{
    return static_cast<png_byte>((value << 3) | (value >> 2));
}

png_byte expand6(uint32_t value)
{
    return static_cast<png_byte>((value << 2) | (value >> 4));
}

png_byte expand10(uint32_t value)
{
    return static_cast<png_byte>((value * 255u + 511u) / 1023u);
}

png_byte normalize16To8(uint32_t value)
{
    return static_cast<png_byte>((value * 255u + 32767u) / 65535u);
}

png_byte unpremultiply(png_byte component, png_byte alpha)
{
    if (alpha == 0) {
        return 0;
    }
    return static_cast<png_byte>(std::min(
        255u, (static_cast<unsigned int>(component) * 255u + alpha / 2u) / alpha));
}

uint32_t unpremultiplyNative(uint32_t component,
                             uint32_t componentMaximum,
                             uint32_t alpha,
                             uint32_t alphaMaximum)
{
    if (alpha == 0) {
        return 0;
    }
    const uint64_t straight =
        (static_cast<uint64_t>(component) * alphaMaximum + alpha / 2u) / alpha;
    return static_cast<uint32_t>(std::min<uint64_t>(componentMaximum, straight));
}

RgbaPixel straightArgb(uint32_t argb)
{
    return {static_cast<png_byte>((argb >> 16) & 0xffu),
            static_cast<png_byte>((argb >> 8) & 0xffu),
            static_cast<png_byte>(argb & 0xffu),
            static_cast<png_byte>((argb >> 24) & 0xffu)};
}

RgbaPixel premultiplied(png_byte red, png_byte green, png_byte blue, png_byte alpha)
{
    return {unpremultiply(red, alpha),
            unpremultiply(green, alpha),
            unpremultiply(blue, alpha),
            alpha};
}

bool readPixel(const PkImage &image, int x, int y, RgbaPixel &result)
{
    const uint8_t *row = image.constScanLine(y);
    if (!row) {
        return false;
    }

    switch (image.format()) {
    case PkImage::Format_Mono:
    case PkImage::Format_MonoLSB:
    case PkImage::Format_Indexed8:
        result = straightArgb(image.color(image.pixelIndex(x, y)));
        return true;
    case PkImage::Format_RGB32: {
        const uint32_t rgb = read32(row + static_cast<std::size_t>(x) * 4u);
        result = straightArgb(rgb | 0xff000000u);
        return true;
    }
    case PkImage::Format_ARGB32:
        result = straightArgb(read32(row + static_cast<std::size_t>(x) * 4u));
        return true;
    case PkImage::Format_ARGB32_Premultiplied: {
        const uint32_t argb = read32(row + static_cast<std::size_t>(x) * 4u);
        result = premultiplied(static_cast<png_byte>((argb >> 16) & 0xffu),
                               static_cast<png_byte>((argb >> 8) & 0xffu),
                               static_cast<png_byte>(argb & 0xffu),
                               static_cast<png_byte>((argb >> 24) & 0xffu));
        return true;
    }
    case PkImage::Format_RGB16: {
        const uint16_t rgb = read16(row + static_cast<std::size_t>(x) * 2u);
        result = {expand5((rgb >> 11) & 0x1fu),
                  expand6((rgb >> 5) & 0x3fu),
                  expand5(rgb & 0x1fu), 0xff};
        return true;
    }
    case PkImage::Format_ARGB8565_Premultiplied: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const uint16_t rgb = static_cast<uint16_t>((pixel[0] << 8) | pixel[1]);
        result = premultiplied(expand5((rgb >> 11) & 0x1fu),
                               expand6((rgb >> 5) & 0x3fu),
                               expand5(rgb & 0x1fu), pixel[2]);
        return true;
    }
    case PkImage::Format_RGB666: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const uint32_t rgb = (static_cast<uint32_t>(pixel[0]) << 16) |
                             (static_cast<uint32_t>(pixel[1]) << 8) | pixel[2];
        result = {expand6((rgb >> 12) & 0x3fu),
                  expand6((rgb >> 6) & 0x3fu),
                  expand6(rgb & 0x3fu), 0xff};
        return true;
    }
    case PkImage::Format_ARGB6666_Premultiplied: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const uint32_t argb = (static_cast<uint32_t>(pixel[0]) << 16) |
                              (static_cast<uint32_t>(pixel[1]) << 8) | pixel[2];
        result = premultiplied(expand6((argb >> 12) & 0x3fu),
                               expand6((argb >> 6) & 0x3fu),
                               expand6(argb & 0x3fu),
                               expand6((argb >> 18) & 0x3fu));
        return true;
    }
    case PkImage::Format_RGB555: {
        const uint16_t rgb = read16(row + static_cast<std::size_t>(x) * 2u);
        result = {expand5((rgb >> 10) & 0x1fu),
                  expand5((rgb >> 5) & 0x1fu),
                  expand5(rgb & 0x1fu), 0xff};
        return true;
    }
    case PkImage::Format_ARGB8555_Premultiplied: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        const uint16_t rgb = static_cast<uint16_t>((pixel[0] << 8) | pixel[1]);
        result = premultiplied(expand5((rgb >> 10) & 0x1fu),
                               expand5((rgb >> 5) & 0x1fu),
                               expand5(rgb & 0x1fu), pixel[2]);
        return true;
    }
    case PkImage::Format_RGB888: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        result = {pixel[0], pixel[1], pixel[2], 0xff};
        return true;
    }
    case PkImage::Format_RGB444: {
        const uint16_t rgb = read16(row + static_cast<std::size_t>(x) * 2u);
        result = {expand4((rgb >> 8) & 0x0fu),
                  expand4((rgb >> 4) & 0x0fu),
                  expand4(rgb & 0x0fu), 0xff};
        return true;
    }
    case PkImage::Format_ARGB4444_Premultiplied: {
        const uint16_t argb = read16(row + static_cast<std::size_t>(x) * 2u);
        result = premultiplied(expand4((argb >> 8) & 0x0fu),
                               expand4((argb >> 4) & 0x0fu),
                               expand4(argb & 0x0fu),
                               expand4((argb >> 12) & 0x0fu));
        return true;
    }
    case PkImage::Format_RGBX8888: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 4u;
        result = {pixel[0], pixel[1], pixel[2], 0xff};
        return true;
    }
    case PkImage::Format_RGBA8888: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 4u;
        result = {pixel[0], pixel[1], pixel[2], pixel[3]};
        return true;
    }
    case PkImage::Format_RGBA8888_Premultiplied: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 4u;
        result = premultiplied(pixel[0], pixel[1], pixel[2], pixel[3]);
        return true;
    }
    case PkImage::Format_BGR30:
    case PkImage::Format_A2BGR30_Premultiplied:
    case PkImage::Format_RGB30:
    case PkImage::Format_A2RGB30_Premultiplied: {
        const uint32_t packed = read32(row + static_cast<std::size_t>(x) * 4u);
        const bool bgr = image.format() == PkImage::Format_BGR30 ||
                         image.format() == PkImage::Format_A2BGR30_Premultiplied;
        const bool hasAlpha = image.format() == PkImage::Format_A2BGR30_Premultiplied ||
                              image.format() == PkImage::Format_A2RGB30_Premultiplied;
        uint32_t first = (packed >> 20) & 0x3ffu;
        uint32_t green = (packed >> 10) & 0x3ffu;
        uint32_t last = packed & 0x3ffu;
        const uint32_t alpha = (packed >> 30) & 0x3u;
        if (hasAlpha) {
            first = unpremultiplyNative(first, 0x3ffu, alpha, 0x3u);
            green = unpremultiplyNative(green, 0x3ffu, alpha, 0x3u);
            last = unpremultiplyNative(last, 0x3ffu, alpha, 0x3u);
        }
        const png_byte red = expand10(bgr ? last : first);
        const png_byte blue = expand10(bgr ? first : last);
        result = {red, expand10(green), blue,
                  hasAlpha ? static_cast<png_byte>(alpha * 85u) : static_cast<png_byte>(0xff)};
        return true;
    }
    case PkImage::Format_Alpha8:
        result = {0, 0, 0, row[x]};
        return true;
    case PkImage::Format_Grayscale8:
        result = {row[x], row[x], row[x], 0xff};
        return true;
    case PkImage::Format_RGBX64:
    case PkImage::Format_RGBA64:
    case PkImage::Format_RGBA64_Premultiplied: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 8u;
        uint32_t red = read16(pixel);
        uint32_t green = read16(pixel + 2);
        uint32_t blue = read16(pixel + 4);
        const uint32_t alpha16 = image.format() == PkImage::Format_RGBX64
            ? 0xffffu : read16(pixel + 6);
        if (image.format() == PkImage::Format_RGBA64_Premultiplied) {
            red = unpremultiplyNative(red, 0xffffu, alpha16, 0xffffu);
            green = unpremultiplyNative(green, 0xffffu, alpha16, 0xffffu);
            blue = unpremultiplyNative(blue, 0xffffu, alpha16, 0xffffu);
        }
        result = {normalize16To8(red),
                  normalize16To8(green),
                  normalize16To8(blue),
                  normalize16To8(alpha16)};
        return true;
    }
    case PkImage::Format_Grayscale16: {
        const png_byte gray = normalize16To8(
            read16(row + static_cast<std::size_t>(x) * 2u));
        result = {gray, gray, gray, 0xff};
        return true;
    }
    case PkImage::Format_BGR888: {
        const uint8_t *pixel = row + static_cast<std::size_t>(x) * 3u;
        result = {pixel[2], pixel[1], pixel[0], 0xff};
        return true;
    }
    case PkImage::Format_Invalid:
        return false;
    }
    return false;
}

bool convertToRgba(const PkImage &image, std::vector<png_byte> &pixels)
{
    if (image.isNull() || image.width() > std::numeric_limits<int>::max() / 4) {
        return false;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(image.width()) * 4u;
    if (static_cast<std::size_t>(image.height()) >
        std::numeric_limits<std::size_t>::max() / rowBytes) {
        return false;
    }
    pixels.resize(rowBytes * static_cast<std::size_t>(image.height()));
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            RgbaPixel pixel;
            if (!readPixel(image, x, y, pixel)) {
                return false;
            }
            const std::size_t offset = static_cast<std::size_t>(y) * rowBytes +
                                       static_cast<std::size_t>(x) * 4u;
            pixels[offset] = pixel.red;
            pixels[offset + 1] = pixel.green;
            pixels[offset + 2] = pixel.blue;
            pixels[offset + 3] = pixel.alpha;
        }
    }
    return true;
}

bool decodedRgbaSize(const png_image &image, std::size_t &byteCount)
{
    if (image.width == 0 || image.height == 0 ||
        image.width > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
        image.height > static_cast<png_uint_32>(std::numeric_limits<int>::max()) ||
        image.width > std::numeric_limits<std::size_t>::max() / 4u) {
        return false;
    }
    const std::size_t rowBytes = static_cast<std::size_t>(image.width) * 4u;
    if (image.height > std::numeric_limits<std::size_t>::max() / rowBytes) {
        return false;
    }
    byteCount = rowBytes * static_cast<std::size_t>(image.height);
    return byteCount <= kMaxDecodedRgbaBytes;
}

bool pkImageLoad(PkImage &image, PkStream *dev, const PkString &format)
{
    // 消费方（loadFromDevice）已用 PkMimeDatabase 判过 mime；这里按 format 派发。
    // PNG 用 libpng 完整实现；JPEG/TIFF/WebP 等其它图像格式是 R-15 已知缺口
    // （未引入 libjpeg），显式失败并登记，等编解码选型落地后补。
    if (format.toUpper() != PkString("PNG")) {
        dbgPigment << "KoPattern: 非 PNG 图像格式 " << format << " 未实现（R-15 缺口）";
        return false;
    }

    PkByteArray data = readAllFromStream(dev);
    if (data.isEmpty()) {
        return false;
    }

    png_image pngImage{};
    pngImage.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_memory(&pngImage, data.constData(),
                                          static_cast<std::size_t>(data.size()))) {
        png_image_free(&pngImage);
        return false;
    }
    std::size_t byteCount = 0;
    if (!decodedRgbaSize(pngImage, byteCount)) {
        png_image_free(&pngImage);
        return false;
    }

    pngImage.format = PNG_FORMAT_RGBA;
    std::vector<png_byte> pixels(byteCount);
    if (!png_image_finish_read(&pngImage, nullptr, pixels.data(), 0, nullptr)) {
        png_image_free(&pngImage);
        return false;
    }

    PkImage result(static_cast<int>(pngImage.width), static_cast<int>(pngImage.height),
                   PkImage::Format_ARGB32);
    if (result.isNull()) {
        png_image_free(&pngImage);
        return false;
    }
    for (png_uint_32 y = 0; y < pngImage.height; ++y) {
        for (png_uint_32 x = 0; x < pngImage.width; ++x) {
            const std::size_t offset =
                (static_cast<std::size_t>(y) * pngImage.width + x) * 4u;
            result.setPixel(static_cast<int>(x), static_cast<int>(y),
                           (static_cast<uint32_t>(pixels[offset + 3]) << 24) |
                           (static_cast<uint32_t>(pixels[offset]) << 16) |
                           (static_cast<uint32_t>(pixels[offset + 1]) << 8) |
                           pixels[offset + 2]);
        }
    }
    png_image_free(&pngImage);
    image = result;
    return true;
}

bool pkImageSave(const PkImage &image, PkStream *dev, const PkString &format)
{
    if (format.toUpper() != PkString("PNG")) {
        dbgPigment << "KoPattern: 非 PNG 图像格式 " << format << " 未实现（R-15 缺口）";
        return false;
    }

    std::vector<png_byte> pixels;
    if (!convertToRgba(image, pixels)) {
        return false;
    }

    png_image pngImage{};
    pngImage.version = PNG_IMAGE_VERSION;
    pngImage.width = static_cast<png_uint_32>(image.width());
    pngImage.height = static_cast<png_uint_32>(image.height());
    pngImage.format = PNG_FORMAT_RGBA;
    png_alloc_size_t byteCount = 0;
    if (!png_image_write_to_memory(&pngImage, nullptr, &byteCount, 0,
                                   pixels.data(), 0, nullptr)) {
        png_image_free(&pngImage);
        return false;
    }
    std::vector<uint8_t> encoded(static_cast<std::size_t>(byteCount));
    if (!png_image_write_to_memory(&pngImage, encoded.data(), &byteCount, 0,
                                   pixels.data(), 0, nullptr)) {
        png_image_free(&pngImage);
        return false;
    }
    png_image_free(&pngImage);
    encoded.resize(static_cast<std::size_t>(byteCount));

    const PkStream::pk_int64 wrote =
        dev->write(reinterpret_cast<const char*>(encoded.data()),
                   static_cast<PkStream::pk_int64>(encoded.size()));
    return wrote == static_cast<PkStream::pk_int64>(encoded.size());
}
}


KoPattern::KoPattern(const PkString& file)
    : KoResource(file)
{
}

KoPattern::KoPattern(const PkImage &image, const PkString &name, const PkString &filename)
    : KoResource(PkString())
{
    setPatternImage(image);
    setName(name);
    setFilename(filename);
}


KoPattern::~KoPattern()
{
}

KoPattern::KoPattern(const KoPattern &rhs)
    : KoResource(rhs)
    , m_pattern(rhs.m_pattern)
{
}

KoResourceSP KoPattern::clone() const
{
    return KoResourceSP(new KoPattern(*this));
}

bool KoPattern::loadPatFromDevice(PkStream *dev)
{
    PkByteArray bytes = readAllFromStream(dev);
    int dataSize = bytes.size();
    const char* data = bytes.constData();

    // load Gimp patterns
    GimpPatternHeader bh;
    qint32 k;
    char* name;

    if ((int)sizeof(GimpPatternHeader) > dataSize) {
        return false;
    }

    memcpy(&bh, data, sizeof(GimpPatternHeader));
    bh.header_size = qFromBigEndian(bh.header_size);
    bh.version = qFromBigEndian(bh.version);
    bh.width = qFromBigEndian(bh.width);
    bh.height = qFromBigEndian(bh.height);
    bh.bytes = qFromBigEndian(bh.bytes);
    bh.magic_number = qFromBigEndian(bh.magic_number);

    if (std::memcmp(bytes.constData() + 20, "GPAT", 4) != 0) {
        dbgPigment << filename() << "is not a .pat pattern file";
        return false;
    }

    if ((int)bh.header_size > dataSize || bh.header_size == 0) {
        return false;
    }
    int size = bh.header_size - sizeof(GimpPatternHeader);
    name = new char[size];
    memcpy(name, data + sizeof(GimpPatternHeader), size);

    if (name[size - 1]) {
        delete[] name;
        return false;
    }

    // size -1 so we don't add the end 0 to the PkString...
    PkString newName = PkString::PkFromUtf8(name, size - 1);
    if (!newName.isEmpty()) { // if it's empty, it's better to leave the name that was there before (based on filename)
        setName(newName);
    }
    delete[] name;

    if (bh.width == 0 || bh.height == 0) {
        return false;
    }

    PkImage::Format imageFormat;

    if (bh.bytes == 1 || bh.bytes == 3) {
        imageFormat = PkImage::Format_RGB32;
    } else {
        imageFormat = PkImage::Format_ARGB32;
    }

    PkImage pattern(static_cast<int>(bh.width), static_cast<int>(bh.height), imageFormat);
    if (pattern.isNull()) {
        return false;
    }
    k = static_cast<qint32>(bh.header_size);

    if (bh.bytes == 1) {
        // Grayscale
        qint32 val;
        for (quint32 y = 0; y < bh.height; ++y) {
            PkRgb* pixels = reinterpret_cast<PkRgb*>(pattern.scanLine(static_cast<int>(y)));
            for (quint32 x = 0; x < bh.width; ++x, ++k) {
                if (k > dataSize) {
                    dbgPigment << "failed to load grayscale pattern" << filename();
                    return false;
                }

                val = data[k];
                pixels[x] = pkRgb(val, val, val);
            }
        }
        // It was grayscale, so make the pattern as small as possible
        // by converting it to Indexed8
        pattern.convertTo(PkImage::Format_Indexed8);
    }
    else if (bh.bytes == 2) {
        // Grayscale + A
        qint32 val;
        qint32 alpha;
        for (quint32 y = 0; y < bh.height; ++y) {
            PkRgb* pixels = reinterpret_cast<PkRgb*>(pattern.scanLine(static_cast<int>(y)));
            for (quint32 x = 0; x < bh.width; ++x, ++k) {
                if (k + 2 > dataSize) {
                    dbgPigment << "failed to load grayscale +_ alpha pattern" << filename();
                    return false;
                }

                val = data[k];
                alpha = data[k++];
                pixels[x] = pkRgba(val, val, val, alpha);
            }
        }
    }
    else if (bh.bytes == 3) {
        // RGB without alpha
        for (quint32 y = 0; y < bh.height; ++y) {
            PkRgb* pixels = reinterpret_cast<PkRgb*>(pattern.scanLine(static_cast<int>(y)));
            for (quint32 x = 0; x < bh.width; ++x) {
                if (k + 3 > dataSize) {
                    dbgPigment << "failed to load RGB pattern" << filename();
                    return false;
                }
                pixels[x] = pkRgb(data[k],
                                 data[k + 1],
                                 data[k + 2]);
                k += 3;
            }
        }
    } else if (bh.bytes == 4) {
        // Has alpha
        for (quint32 y = 0; y < bh.height; ++y) {
            PkRgb* pixels = reinterpret_cast<PkRgb*>(pattern.scanLine(static_cast<int>(y)));
            for (quint32 x = 0; x < bh.width; ++x) {
                if (k + 4 > dataSize) {
                    dbgPigment << "failed to load RGB + Alpha pattern" << filename();
                    return false;
                }

                pixels[x] = pkRgba(data[k],
                                  data[k + 1],
                                  data[k + 2],
                                  data[k + 3]);
                k += 4;
            }
        }
    } else {
        return false;
    }

    if (pattern.isNull()) {
        return false;
    }

    setPatternImage(pattern);
    setValid(true);

    return true;

}

bool KoPattern::savePatToDevice(PkStream* dev) const
{
    // Header: header_size (24+name length),version,width,height,colordepth of brush,magic,name
    // depth: 1 = greyscale, 2 = greyscale + A, 3 = RGB, 4 = RGBA
    // magic = "GPAT", as a single uint32, the docs are wrong here!
    // name is UTF-8 (\0-terminated! The docs say nothing about this!)
    // _All_ data in network order, it seems! (not mentioned in gimp-2.2.8/devel-docs/pat.txt!!)
    // We only save RGBA at the moment
    // Version is 1 for now...



    GimpPatternHeader ph;
    std::string utf8Name = name().PkToUtf8();
    char const* name = utf8Name.c_str();
    int nameLength = static_cast<int>(std::strlen(name));

    ph.header_size = qToBigEndian((quint32)sizeof(GimpPatternHeader) + nameLength + 1); // trailing 0
    ph.version = qToBigEndian((quint32)1);
    ph.width = qToBigEndian((quint32)width());
    ph.height = qToBigEndian((quint32)height());
    ph.bytes = qToBigEndian((quint32)4);
    ph.magic_number = qToBigEndian((quint32)GimpPatternMagic);

    PkStream::pk_int64 wrote = dev->write(reinterpret_cast<char*>(&ph), sizeof(GimpPatternHeader));

    if (wrote == -1)
        return false;

    wrote = dev->write(name, nameLength + 1); // Trailing 0 apparently!
    if (wrote == -1)
        return false;

    int k = 0;
    PkByteArray bytes;
    bytes.resize(width() * height() * 4);
    char* bdata = bytes.data();
    for (qint32 y = 0; y < height(); ++y) {
        for (qint32 x = 0; x < width(); ++x) {
            // RGBA only
            PkRgb pixel = m_pattern.pixel(x, y);
            bdata[k++] = static_cast<char>(pkRed(pixel));
            bdata[k++] = static_cast<char>(pkGreen(pixel));
            bdata[k++] = static_cast<char>(pkBlue(pixel));
            bdata[k++] = static_cast<char>(pkAlpha(pixel));
        }
    }

    wrote = dev->write(bdata, bytes.size());
    if (wrote == -1)
        return false;

    return true;
}

bool KoPattern::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(resourcesInterface);

    PkByteArray ba = readAllFromStream(dev);

    PkMemoryStream buf;
    buf.open(PkStream::ReadWrite);
    buf.write(ba.constData(), ba.size());
    buf.seek(0);

    bool result = false;

    if (isSupportedImageMime(PkMimeDatabase::mimeTypeForData(ba))) {
        PkImage image;
        result = pkImageLoad(image, &buf, fileExtensionUpper(filename()));
        if (result) {
            setPatternImage(image);
        }
    }
    else {
        result = loadPatFromDevice(&buf);
    }

    return result;

}

bool KoPattern::saveToDevice(PkStream *dev) const
{
    PkString fileExtension = fileExtensionUpper(filename());

    bool result = false;

    if (fileExtension == "PAT") {
        result = savePatToDevice(dev);
    }
    else {
        if (fileExtension.isEmpty()) {
            fileExtension = "PNG";
        }
        result = pkImageSave(m_pattern, dev, fileExtension);
    }

    return result;
}


qint32 KoPattern::width() const
{
    return m_pattern.width();
}

qint32 KoPattern::height() const
{
    return m_pattern.height();
}

void KoPattern::setPatternImage(const PkImage& image)
{
    m_pattern = image;
    checkForAlpha(image);
    setImage(image);
    setValid(true);
}


PkString KoPattern::defaultFileExtension() const
{
    return PkString(".pat");
}


PkImage KoPattern::pattern() const
{
    return m_pattern;
}

void KoPattern::checkForAlpha(const PkImage& image) {
    m_hasAlpha = false;
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            if (pkAlpha(image.pixel(x, y)) != 255) {
                m_hasAlpha = true;
                break;
            }
        }
    }
}

bool KoPattern::hasAlpha() const
{
    return m_hasAlpha;
}

KoPatternSP KoPattern::cloneWithoutAlpha() const
{
    if (!hasAlpha()) return clone().dynamicCast<KoPattern>();

    PkImage image = this->image();

    for (int y = 0; y < image.height(); ++y) {
        PkRgb *ptr = reinterpret_cast<PkRgb*>(image.scanLine(y));

        for (int x = 0; x < image.width(); ++x) {
            const qreal coeff = pkAlpha(*ptr) / 255.0;
            *ptr = pkRgba(qRound(coeff * pkRed(*ptr)), qRound(coeff * pkGreen(*ptr)), qRound(coeff * pkBlue(*ptr)), 255);
            ptr++;
        }
    }

    KoPatternSP flattenedPattern =
        toQShared(new KoPattern(image, this->name(), this->filename()));

    return flattenedPattern;
}
