/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_png_brush.h"

#include <PkXmlElement.h>
#include <QFileInfo>
#include <QImageReader>
#include <PkAuxTypes.h>
#include <PkMemoryStream.h>
#include <QPainter>

#include <kis_dom_utils.h>

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
    Q_UNUSED(resourcesInterface);

    // Workaround for some OS (Debian, Ubuntu), where loading directly from the PkStream
    // fails with "libpng error: IDAT: CRC error"
    PkByteArray data = dev->readAll();
    PkMemoryStream buf(&data);
    buf.open(PkStream::ReadOnly);
    QImageReader reader(&buf, "PNG");

    if (!reader.canRead()) {
        dbgKrita << "Could not read brush" << filename() << ". Error:" << reader.errorString();
        setValid(false);
        return false;
    }

    if (reader.textKeys().contains("brush_spacing")) {
        setSpacing(KisDomUtils::toDouble(reader.text("brush_spacing")));
    }

    if (reader.textKeys().contains("brush_name")) {
        setName(reader.text("brush_name"));
    }
    else {
        QFileInfo info(filename());
        setName(info.completeBaseName());
    }

    PkImage image = reader.read();

    if (image.isNull()) {
        dbgKrita << "Could not create image for" << filename() << ". Error:" << reader.errorString();
        setValid(false);
        return false;
    }

    setValid(true);

    bool hasAlpha = false;
    for (int y = 0; y < image.height(); y++) {
        for (int x = 0; x < image.width(); x++) {
            if (qAlpha(image.pixel(x, y)) != 255) {
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
        PkImage base(image.size(), image.format());
        if ((int)base.format() < (int)PkImage::Format_RGB32) {
            base.convertTo(PkImage::Format_ARGB32);
        }
        QPainter gc(&base);
        gc.fillRect(base.rect(), Qt::white);
        gc.drawImage(0, 0, image);
        gc.end();
        PkImage converted = base.convertToFormat(PkImage::Format_Grayscale8);
        setBrushTipImage(converted);
        setBrushType(MASK);
        setBrushApplication(ALPHAMASK);
        setHasColorAndTransparency(false);
    }
    else {
        // see bug https://bugs.kde.org/show_bug.cgi?id=484115 if you want to edit this condition
        // keep it in sync with KisColorfulBrush code
        if ((int)image.format() != (int)PkImage::Format_ARGB32) {
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
    return brushTipImage().save(dev, "PNG");
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
