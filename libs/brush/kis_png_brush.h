/*
 *  SPDX-FileCopyrightText: 2010 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PNG_BRUSH_
#define KIS_PNG_BRUSH_

#include "KisColorfulBrush.h"

class BRUSH_EXPORT  KisPngBrush : public KisColorfulBrush
{
public:
    /// Construct brush to load filename later as brush
    KisPngBrush(const PkString& filename);
    KisPngBrush(const KisPngBrush &rhs);
    KoResourceSP clone() const override;
    KisPngBrush &operator=(const KisPngBrush &rhs) = delete;

    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream *dev) const override;

    PkString defaultFileExtension() const override;
    void toXML(PkXmlDocument& d, PkXmlElement& e) const override;

    std::pair<PkString, PkString> resourceType() const override {
        return std::pair<PkString, PkString>(ResourceType::Brushes, ResourceSubType::PngBrushes);
    }

};

#endif
