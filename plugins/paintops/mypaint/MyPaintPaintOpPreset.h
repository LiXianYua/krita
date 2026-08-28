/*
 * SPDX-FileCopyrightText: 2020 Ashwin Dhakaita <ashwingpdhakaita@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_MYPAINT_BRUSH_H
#define KIS_MYPAINT_BRUSH_H

#include <libmypaint/mypaint-brush.h>
#include <KoColor.h>
#include <brushengine/kis_paintop_settings.h>
#include <kis_painter.h>
#include <KoResource.h>
#include <KisResourceTypes.h>
#include <kis_paintop_preset.h>
#include <PkAuxTypes.h>
#include <PkStream.h>

class KisMyPaintPaintOpPreset : public KisPaintOpPreset
{
public:

    KisMyPaintPaintOpPreset(const PkString &fileName="");
    KisMyPaintPaintOpPreset(const KisMyPaintPaintOpPreset &rhs);
    virtual ~KisMyPaintPaintOpPreset();

    KoResourceSP clone() const override;

    void setColor(const KoColor color, const KoColorSpace *colorSpace);
    void apply(KisPaintOpSettingsSP settings);
    MyPaintBrush* brush();

    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;

    std::pair<PkString, PkString> resourceType() const override {
        return std::pair<PkString, PkString>(ResourceType::PaintOpPresets, ResourceSubType::MyPaintPaintOpPresets);
    }

    void updateThumbnail() override;
    PkString thumbnailPath() const override;

    PkByteArray getJsonData();

private:
    class Private;
    Private* const d;
};

#endif // KIS_MYPAINT_BRUSH_H
