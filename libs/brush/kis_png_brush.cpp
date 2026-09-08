/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_png_brush.h"

#include <PkAuxTypes.h>
#include <PkPngReader.h>
#include <PkStream.h>
#include <PkXmlElement.h>
#include <filesystem>

#include <kis_dom_utils.h>
#include "KisBrushStreamUtils.h"

// Legacy completeBaseName behavior: remove only the final extension.
// 复刻 S-02-b PkResourceStorageDesktop 的 std::filesystem 处理模式
// （Task 1 修复轮 pathFileName 同款）。
static PkString pathCompleteBaseName(const PkString &path)
{
    const std::string name = std::filesystem::u8path(path.PkToUtf8()).stem().string();
    return PkString::PkFromUtf8(name.c_str(), static_cast<int>(name.size()));
}

KisPngBrush::KisPngBrush(const PkString& filename)
    : KisColorfulBrush(filename)
{
    setBrushType(INVALID);
    setSpacing(0.25);
}

KisPngBrush::KisPngBrush(const KisPngBrush &rhs)
    : KisColorfulBrush(rhs)
{
}

KoResourceSP KisPngBrush::clone() const
{
    return KoResourceSP(new KisPngBrush(*this));
}

bool KisPngBrush::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    (void)resourcesInterface;

    // Workaround for some OS (Debian, Ubuntu), where loading directly from the PkStream
    // fails with "libpng error: IDAT: CRC error"
    const PkByteArray data = kisBrushReadAll(dev);
    const PkPngReadResult decoded = PkPngReader::read(
        reinterpret_cast<const uint8_t *>(data.constData()),
        static_cast<std::size_t>(data.size()));

    const auto spacing = decoded.text.find("brush_spacing");
    if (spacing != decoded.text.end()) {
        setSpacing(KisDomUtils::toDouble(PkString::fromUtf8(spacing->second.c_str())));
    }

    const auto name = decoded.text.find("brush_name");
    if (name != decoded.text.end()) {
        setName(PkString::PkFromUtf8(name->second.data(), static_cast<int>(name->second.size())));
    }
    else {
        setName(pathCompleteBaseName(filename()));
    }

    PkImage image = decoded.image;

    if (image.isNull()) {
        setValid(false);
        return false;
    }

    setValid(true);

    bool hasAlpha = false;
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            if ((image.pixel(x, y) >> 24) != 255) {
                hasAlpha = true;
                break;
            }
        }
    }

    const bool isAllGray = image.allGray();

    if (isAllGray && !hasAlpha) {
        // Make sure brush tips all have a white background
        // NOTE: drawing it over white background can probably be skipped now...
        //       Any images with an Alpha channel should be loaded as RGBA so
        //       they can have the lightness and gradient options available
        setBrushTipImage(image.convertToFormat(PkImage::Format_Grayscale8));
        setBrushType(MASK);
        setBrushApplication(ALPHAMASK);
        setHasColorAndTransparency(false);
    }
    else {
        // see bug https://bugs.kde.org/show_bug.cgi?id=484115 if you want to edit this condition
        // keep it in sync with KisColorfulBrush code
        if (image.format() != PkImage::Format_ARGB32) {
            image.convertTo(PkImage::Format_ARGB32);
        }

        setBrushTipImage(image);
        setBrushType(IMAGE);
        setBrushApplication(isAllGray ? ALPHAMASK : LIGHTNESSMAP);
        setHasColorAndTransparency(!isAllGray);
    }


    setWidth(brushTipImage().width());
    setHeight(brushTipImage().height());

    return valid();
}

bool KisPngBrush::saveToDevice(PkStream *dev) const
{
    (void)dev;
    // GAP: PkImage::save 未交付（R-15/S-03-e libpng 通道）。PNG 编码待图像编解码任务。
    return false;
}

PkString KisPngBrush::defaultFileExtension() const
{
    return PkString(".png");
}

void KisPngBrush::toXML(PkXmlDocument& d, PkXmlElement& e) const
{
    predefinedBrushToXML("png_brush", e);
    KisColorfulBrush::toXML(d, e);
}
