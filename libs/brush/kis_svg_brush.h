/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_SVG_BRUSH_
#define KIS_SVG_BRUSH_

#include <PkAuxTypes.h>
#include <PkStream.h>
#include <PkString.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>

#include <utility>

#include "kis_scaling_size_brush.h"

class BRUSH_EXPORT KisSvgBrush : public KisScalingSizeBrush
{
public:
    /// Construct brush to load filename later as brush
    KisSvgBrush(const PkString &filename);
    KisSvgBrush(const KisSvgBrush &rhs);
    KisSvgBrush &operator=(const KisSvgBrush &rhs) = delete;

    KoResourceSP clone() const override;

    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream *dev) const override;

    std::pair<PkString, PkString> resourceType() const override {
        return std::pair<PkString, PkString>(ResourceType::Brushes, ResourceSubType::SvgBrushes);
    }

    PkString defaultFileExtension() const override;
    void toXML(PkXmlDocument& d, PkXmlElement& e) const override;
private:
    PkByteArray m_svg;
};

#endif
