/*
 *  SPDX-FileCopyrightText: 1999 Matthias Elter <me@kde.org>
 *  SPDX-FileCopyrightText: 2003 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2004 Adrian Page <adrian@pagenet.plus.com>
 *  SPDX-FileCopyrightText: 2005 Bart Coppens <kde@bartcoppens.be>
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include <sys/types.h>
#include <QtEndian>

#include "kis_gbr_brush.h"

#include <PkXmlElement.h>
#include <PkFileStream.h>
#include <PkImage.h>
#include <PkPoint.h>
#include <PkRgb.h>

#include <kis_debug.h>

#include <KoColor.h>
#include <KoColorSpaceRegistry.h>

#include "kis_datamanager.h"
#include "kis_paint_device.h"
#include "kis_global.h"
#include "kis_image.h"
#include "KisBrushPixelUtils.h"
#include "KisBrushStreamUtils.h"

struct GimpBrushV1Header {
    quint32 header_size;  /*  header_size = sizeof (BrushHeader) + brush name  */
    quint32 version;      /*  brush file version #  */
    quint32 width;        /*  width of brush  */
    quint32 height;       /*  height of brush  */
    quint32 bytes;        /*  depth of brush in bytes */
};

/// All fields are in MSB on disk!
struct GimpBrushHeader {
    quint32 header_size;  /*  header_size = sizeof (BrushHeader) + brush name  */
    quint32 version;      /*  brush file version #  */
    quint32 width;        /*  width of brush  */
    quint32 height;       /*  height of brush  */
    quint32 bytes;        /*  depth of brush in bytes */

    /*  The following are only defined in version 2 */
    quint32 magic_number; /*  GIMP brush magic number  */
    quint32 spacing;      /*  brush spacing as % of width & height, 0 - 1000 */
};

// Needed, or the GIMP won't open it!
quint32 const GimpV2BrushMagic = ('G' << 24) + ('I' << 16) + ('M' << 8) + ('P' << 0);


struct KisGbrBrush::Private {

    PkByteArray data;
    quint32 header_size;  /*  header_size = sizeof (BrushHeader) + brush name  */
    quint32 version;      /*  brush file version #  */
    quint32 bytes;        /*  depth of brush in bytes */
    quint32 magic_number; /*  GIMP brush magic number  */
};

#define DEFAULT_SPACING 0.25

KisGbrBrush::KisGbrBrush(const PkString& filename)
    : KisColorfulBrush(filename)
    , d(new Private)
{
    setSpacing(DEFAULT_SPACING);
}

KisGbrBrush::KisGbrBrush(const PkString& filename,
                         const PkByteArray& data,
                         qint32 & dataPos)
    : KisColorfulBrush(filename)
    , d(new Private)
{
    setSpacing(DEFAULT_SPACING);

    d->data = PkByteArray(data.data() + dataPos, data.size() - dataPos);
    init();
    d->data.resize(0);
    dataPos += d->header_size + (width() * height() * d->bytes);
}

KisGbrBrush::KisGbrBrush(KisPaintDeviceSP image, int x, int y, int w, int h)
    : KisColorfulBrush()
    , d(new Private)
{
    setSpacing(DEFAULT_SPACING);
    initFromPaintDev(image, x, y, w, h);
}

KisGbrBrush::KisGbrBrush(const PkImage& image, const PkString& name)
    : KisColorfulBrush()
    , d(new Private)
{
    setSpacing(DEFAULT_SPACING);

    setBrushTipImage(image);
    setName(name);
}

KisGbrBrush::KisGbrBrush(const KisGbrBrush& rhs)
    : KisColorfulBrush(rhs)
    , d(new Private(*rhs.d))
{
    d->data = PkByteArray();
}

KoResourceSP KisGbrBrush::clone() const
{
    return KoResourceSP(new KisGbrBrush(*this));
}

KisGbrBrush::~KisGbrBrush()
{
    delete d;
}

bool KisGbrBrush::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(resourcesInterface);
    d->data = kisBrushReadAll(dev);
    return init();
}

bool KisGbrBrush::init()
{
    GimpBrushHeader bh;

    if (sizeof(GimpBrushHeader) > (uint)d->data.size()) {
        warnKrita << filename() << "GBR could not be loaded: expected header size larger than bytearray size. Header Size:" << sizeof(GimpBrushHeader) << "Byte array size" << d->data.size();
        return false;
    }

    memcpy(&bh, d->data.constData(), sizeof(GimpBrushHeader));
    bh.header_size = qFromBigEndian(bh.header_size);
    d->header_size = bh.header_size;

    bh.version = qFromBigEndian(bh.version);
    d->version = bh.version;

    bh.width = qFromBigEndian(bh.width);
    bh.height = qFromBigEndian(bh.height);

    bh.bytes = qFromBigEndian(bh.bytes);
    d->bytes = bh.bytes;

    bh.magic_number = qFromBigEndian(bh.magic_number);
    d->magic_number = bh.magic_number;

    if (bh.version == 1) {
        // No spacing in version 1 files so use Gimp default
        bh.spacing = static_cast<int>(DEFAULT_SPACING * 100);
    }
    else {
        bh.spacing = qFromBigEndian(bh.spacing);

        if (bh.spacing > 1000) {
            warnKrita << filename()  << "GBR could not be loaded, spacing above 1000. Spacing:" << bh.spacing;
            return false;
        }
    }

    setSpacing(bh.spacing / 100.0);

    if (bh.header_size > (uint)d->data.size() || bh.header_size == 0) {
        warnKrita << "GBR could not be loaded: header size larger than bytearray size. Header Size:" << bh.header_size << "Byte array size" << d->data.size();
        return false;
    }

    PkString name;

    if (bh.version == 1) {
        // Version 1 has no magic number or spacing, so the name
        // is at a different offset. Character encoding is undefined.
        const char *text = d->data.constData() + sizeof(GimpBrushV1Header);
        // GBR v1 文件名编码未定义（原 Qt 用 fromLatin1 逐字节映射 U+00xx）。Pk 无 Latin-1
        // codec：改用 PkFromUtf8。ASCII 名精确等价；非 ASCII 高字节（Latin-1 0x80-0xFF）
        // 在 UTF-8 解码下退化为 U+FFFD 替换符——GBR v1 名实际几乎全 ASCII，可接受。
        name = PkString::PkFromUtf8(text, bh.header_size - sizeof(GimpBrushV1Header) - 1);
    }
    else {
        // ### Version = 3->cinepaint; may be float16 data!
        // Version >=2: UTF-8 encoding is used
        name = PkString::PkFromUtf8(d->data.constData() + sizeof(GimpBrushHeader),
                                 bh.header_size - sizeof(GimpBrushHeader) - 1);
    }

    setName(name);

    if (bh.width == 0 || bh.height == 0) {
        warnKrita << filename()  << "GBR loading failed: width or height is 0" << bh.width << bh.height;
        return false;
    }

    PkImage::Format imageFormat;

    if (bh.bytes == 1) {
        imageFormat = PkImage::Format_Indexed8;
    } else {
        imageFormat = PkImage::Format_ARGB32;
    }

    PkImage image(PkImage(bh.width, bh.height, imageFormat));

    if (image.isNull()) {
        warnKrita << filename()  << "GBR loading failed; image could not be created from following dimensions" << bh.width << bh.height
                   << "PkImage::Format" << imageFormat;
        return false;
    }

    qint32 k = bh.header_size;

    if (bh.bytes == 1) {
        // PkImage::setColorTable 收 std::vector<uint32_t>（PkRgb 即 uint32_t，
        // qRgb 合成值类型一致），用 std::vector 直配。
        std::vector<uint32_t> table;
        for (int i = 0; i < 256; ++i) table.push_back(pkRgb(i, i, i));
        image.setColorTable(table);
        // Grayscale

        if (static_cast<qint32>(k + bh.width * bh.height) > d->data.size()) {
            warnKrita << filename()  << "GBR file dimensions bigger than bytearray size. Header:"<< k << "Width:" << bh.width << "height" << bh.height
                       << "expected byte array size:" << (k + (bh.width * bh.height)) << "actual byte array size" << d->data.size();
            return false;
        }

        setBrushApplication(ALPHAMASK);
        setBrushType(MASK);
        setHasColorAndTransparency(false);

        for (quint32 y = 0; y < bh.height; y++) {
            uchar *pixel = reinterpret_cast<uchar *>(image.scanLine(y));
            for (quint32 x = 0; x < bh.width; x++, k++) {
                qint32 val = 255 - static_cast<uchar>(d->data.constData()[k]);
                *pixel = val;
                ++pixel;
            }
        }
    } else if (bh.bytes == 4) {
        // RGBA

        if (static_cast<qint32>(k + (bh.width * bh.height * 4)) > d->data.size()) {
            warnKrita << filename()  << "GBR file dimensions bigger than bytearray size. Header:"<< k << "Width:" << bh.width << "height" << bh.height
                       << "expected byte array size:" << (k + (bh.width * bh.height * 4)) << "actual byte array size" << d->data.size();
            return false;
        }

        setBrushApplication(LIGHTNESSMAP);
        setBrushType(IMAGE);

        for (quint32 y = 0; y < bh.height; y++) {
            PkRgb *pixel = reinterpret_cast<PkRgb *>(image.scanLine(y));
            for (quint32 x = 0; x < bh.width; x++, k += 4) {
                *pixel = pkRgba(static_cast<uchar>(d->data.constData()[k]),
                                static_cast<uchar>(d->data.constData()[k + 1]),
                                static_cast<uchar>(d->data.constData()[k + 2]),
                                static_cast<uchar>(d->data.constData()[k + 3]));
                ++pixel;
            }
        }

        setHasColorAndTransparency(!image.allGray());
    }
    else {
        warnKrita << filename()  << "WARNING: loading of GBR brushes with" << bh.bytes << "bytes per pixel is not supported";
        return false;
    }

    setWidth(image.width());
    setHeight(image.height());
    if (!d->data.isEmpty()) {
        d->data.resize(0); // Save some memory, we're using enough of it as it is.
    }
    setValid(image.width() != 0 && image.height() != 0);
    setBrushTipImage(image);
    return true;
}

bool KisGbrBrush::initFromPaintDev(KisPaintDeviceSP image, int x, int y, int w, int h)
{
    // Forcefully convert to RGBA8
    // XXX profile and exposure?
    setBrushTipImage(image->convertToQImage(0, x, y, w, h, KoColorConversionTransformation::internalRenderingIntent(), KoColorConversionTransformation::internalConversionFlags()));
    setName(image->objectName());

    setBrushType(IMAGE);
    setBrushApplication(LIGHTNESSMAP);

    return true;
}

bool KisGbrBrush::saveToDevice(PkStream* dev) const
{
    if (!valid() || brushTipImage().isNull()) {
        warnKrita << "this brush is not valid, set a brush tip image" << filename();
        return false;
    }
    GimpBrushHeader bh;
    const std::string utf8Name = name().PkToUtf8(); // Names in v2 brushes are in UTF-8
    char const* name = utf8Name.data();
    int nameLength = qstrlen(name);
    int wrote;

    bh.header_size = qToBigEndian((quint32)sizeof(GimpBrushHeader) + nameLength + 1);
    bh.version = qToBigEndian((quint32)2); // Only RGBA8 data needed atm, no cinepaint stuff
    bh.width = qToBigEndian((quint32)width());
    bh.height = qToBigEndian((quint32)height());
    // Hardcoded, 4 bytes RGBA or 1 byte GREY
    if (!isImageType()) {
        bh.bytes = qToBigEndian((quint32)1);
    }
    else {
        bh.bytes = qToBigEndian((quint32)4);
    }
    bh.magic_number = qToBigEndian((quint32)GimpV2BrushMagic);
    bh.spacing = qToBigEndian(static_cast<quint32>(spacing() * 100.0));

    // Write header: first bh, then the name
    PkByteArray bytes(reinterpret_cast<char*>(&bh), sizeof(GimpBrushHeader));
    wrote = dev->write(bytes.constData(), bytes.size());
    bytes.resize(0);

    if (wrote == -1) {
        return false;
    }

    wrote = dev->write(name, nameLength + 1);

    if (wrote == -1) {
        return false;
    }

    int k = 0;

    PkImage image = brushTipImage();

    if (!isImageType()) {
        bytes.resize(width() * height());
        for (qint32 y = 0; y < height(); y++) {
            for (qint32 x = 0; x < width(); x++) {
                PkRgb c = image.pixel(x, y);
                bytes.data()[k++] = static_cast<char>(255 - pkRed(c)); // red == blue == green
            }
        }
    } else {
        bytes.resize(width() * height() * 4);
        for (qint32 y = 0; y < height(); y++) {
            for (qint32 x = 0; x < width(); x++) {
                // order for gimp brushes, v2 is: RGBA
                PkRgb pixel = image.pixel(x, y);
                bytes.data()[k++] = static_cast<char>(pkRed(pixel));
                bytes.data()[k++] = static_cast<char>(pkGreen(pixel));
                bytes.data()[k++] = static_cast<char>(pkBlue(pixel));
                bytes.data()[k++] = static_cast<char>(pkAlpha(pixel));
            }
        }
    }

    wrote = dev->write(bytes.constData(), bytes.size());
    if (wrote == -1) {
        return false;
    }
    return true;
}

void KisGbrBrush::setBrushTipImage(const PkImage& image)
{
    KisBrush::setBrushTipImage(image);
    setValid(true);
}

void KisGbrBrush::makeMaskImage(bool preserveAlpha)
{
    if (!isImageType()) {
        return;
    }

    PkImage brushTip = brushTipImage();

    if (!preserveAlpha) {
        const int imageWidth = brushTip.width();
        const int imageHeight = brushTip.height();
        PkImage image(imageWidth, imageHeight, PkImage::Format_Indexed8);
        std::vector<uint32_t> table;
        for (int i = 0; i < 256; ++i) {
            table.push_back(pkRgb(i, i, i));
        }
        image.setColorTable(table);

        for (int y = 0; y < imageHeight; y++) {
            PkRgb *pixel = reinterpret_cast<PkRgb *>(brushTip.scanLine(y));
            uchar * dstPixel = image.scanLine(y);
            for (int x = 0; x < imageWidth; x++) {
                PkRgb c = pixel[x];
                float alpha = pkAlpha(c) / 255.0f;
                // linear interpolation with maximum gray value which is transparent in the mask
                //int a = (qGray(c) * alpha) + ((1.0 - alpha) * 255);
                // single multiplication version
                int a = 255 + int(alpha * (kisBrushGray(c) - 255));
                dstPixel[x] = (uchar)a;
            }
        }
        setBrushTipImage(image);
        setBrushType(MASK);
    }
    else {
        setBrushTipImage(brushTip);
        setBrushType(IMAGE);
    }

    setBrushApplication(ALPHAMASK);
    resetOutlineCache();
    clearBrushPyramid();
}

void KisGbrBrush::toXML(PkXmlDocument& d, PkXmlElement& e) const
{
    predefinedBrushToXML("gbr_brush", e);
    KisColorfulBrush::toXML(d, e);
}

PkString KisGbrBrush::defaultFileExtension() const
{
    return PkString(".gbr");
}
