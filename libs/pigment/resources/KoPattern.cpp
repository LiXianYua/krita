/*  This file is part of the KDE project

    SPDX-FileCopyrightText: 2000 Matthias Elter <elter@kde.org>
    SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>

    SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <PkXmlCompat.h>

#include <resources/KoPattern.h>

#include <sys/types.h>

#include <cstring>
#include <filesystem>
#include <limits.h>
#include <stdlib.h>

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

// 图像文件编解码是 pk/image 的已知缺口（岔路 A，pk/image/README.md 待认领缺口①）：
// PkImage 没有 load()/save()，本批次不引入外部编解码库。GPAT 路径完整保留；非
// GPAT 图像（PNG/JPEG 等）的 load/save 在此显式失败，等 impex / libs/resources
// 各自的编解码落地后替换为真实实现（消费方保持 loadFromDevice/saveToDevice 不变）。
bool pkImageLoad(PkImage &image, PkStream *dev, const PkString &format)
{
    Q_UNUSED(image);
    Q_UNUSED(dev);
    Q_UNUSED(format);
    return false;
}

bool pkImageSave(const PkImage &image, PkStream *dev, const PkString &format)
{
    Q_UNUSED(image);
    Q_UNUSED(dev);
    Q_UNUSED(format);
    return false;
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
    PkByteArray bytes = dev->readAll();
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

    PkByteArray ba = dev->readAll();

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
