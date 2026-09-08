/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_svg_brush.h"

#include <filesystem>
#include <vector>

#include <PkSvgRasterizer.h>

#include "KisBrushStreamUtils.h"

namespace {

PkString pathCompleteBaseName(const PkString &path)
{
    const auto utf8 = std::filesystem::u8path(path.PkToUtf8()).stem().u8string();
    const std::string name(utf8.begin(), utf8.end());
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
    (void)resourcesInterface;

    m_svg = kisBrushReadAll(dev);
    const PkImage image = PkSvgRasterizer::render(m_svg.constData(),
                                                   static_cast<std::size_t>(m_svg.size()),
                                                   1000);
    if (image.isNull()) return false;

    std::vector<uint32_t> table;
    table.reserve(256);
    for (unsigned i = 0; i < 256; ++i) {
        table.push_back(0xff000000u | (i << 16) | (i << 8) | i);
    }
    PkImage indexed(image.size(), PkImage::Format_Indexed8);
    indexed.setColorTable(table);
    // The legacy ARGB32 -> Indexed8 path with an explicit table retains the
    // low byte as the palette index. Brush presets depend on that behavior;
    // the independent 5.15 oracle maps 0xffff8080 to index 128.
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            indexed.setPixel(x, y, image.pixel(x, y) & 0xffu);
        }
    }
    setBrushTipImage(indexed);

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
