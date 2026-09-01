/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QByteArray>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>

#include "kis_svg_brush.h"

#include <cstring>
#include <filesystem>
#include <vector>

namespace {

PkImage toPkImage(const QImage &image)
{
    PkImage result(image.width(), image.height(), static_cast<PkImage::Format>(image.format()));

    if (image.colorCount() > 0) {
        std::vector<uint32_t> colors;
        colors.reserve(static_cast<size_t>(image.colorCount()));
        for (int i = 0; i < image.colorCount(); ++i) {
            colors.push_back(image.color(i));
        }
        result.setColorTable(colors);
    }

    for (int y = 0; y < image.height(); ++y) {
        std::memcpy(result.scanLine(y), image.constScanLine(y), static_cast<size_t>(image.bytesPerLine()));
    }

    return result;
}

PkString pathCompleteBaseName(const PkString &path)
{
    const std::string name = std::filesystem::u8path(path.PkToUtf8()).stem().string();
    return PkString::PkFromUtf8(name.c_str(), static_cast<int>(name.size()));
}

} // namespace

KisSvgBrush::KisSvgBrush(const PkString& filename)
    : KisScalingSizeBrush(filename)
{
    setBrushType(INVALID);
    setSpacing(0.25);
}

KisSvgBrush::KisSvgBrush(const KisSvgBrush& rhs)
    : KisScalingSizeBrush(rhs)
    , m_svg(rhs.m_svg)
{
}

KoResourceSP KisSvgBrush::clone() const
{
    return KoResourceSP(new KisSvgBrush(*this));
}

bool KisSvgBrush::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(resourcesInterface);

    m_svg = dev->readAll();

    const QByteArray svgBytes(m_svg.constData(), m_svg.size());
    QSvgRenderer renderer(svgBytes);

    QRect box = renderer.viewBox();
    if (box.isEmpty()) return false;

    QImage image_(1000, (1000 * box.height()) / box.width(), QImage::Format_ARGB32);
    {
        QPainter p(&image_);
        p.fillRect(0, 0, image_.width(), image_.height(), Qt::white);
        renderer.render(&p);
    }

    QVector<QRgb> table;
    for (int i = 0; i < 256; ++i) table.push_back(qRgb(i, i, i));
    image_ = image_.convertToFormat(QImage::Format_Indexed8, table);

    setBrushTipImage(toPkImage(image_));

    setValid(true);

    setBrushType(MASK);

    setWidth(brushTipImage().width());
    setHeight(brushTipImage().height());

    setName(pathCompleteBaseName(filename()));

    return !brushTipImage().isNull() && valid();
}

bool KisSvgBrush::saveToDevice(PkStream *dev) const
{
    return dev->write(m_svg.constData(), m_svg.size()) == m_svg.size();
}

PkString KisSvgBrush::defaultFileExtension() const
{
    return PkString(".svg");
}

void KisSvgBrush::toXML(PkXmlDocument& d, PkXmlElement& e) const
{
    predefinedBrushToXML("svg_brush", e);
    KisBrush::toXML(d, e);
}
