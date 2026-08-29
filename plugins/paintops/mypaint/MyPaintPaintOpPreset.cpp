/*
 * SPDX-FileCopyrightText: 2020 Ashwin Dhakaita <ashwingpdhakaita@gmail.com>
 * SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "MyPaintPaintOpPreset.h"

#include <PkAuxTypes.h>
#include <PkColor.h>
#include <PkImage.h>
#include <array>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <libmypaint/mypaint-brush.h>
#include <png.h>

#include <KisResourceLocator.h>
#include <KoColorConversions.h>
#include <KoColorModelStandardIds.h>
#include <kis_debug.h>

#include "MyPaintPaintOpSettings.h"
#include "MyPaintBrushUtils.h"
#include "MyPaintSensorPack.h"
#include "MyPaintStandardOptionData.h"

namespace
{
PkString pathCompleteBaseName(const PkString &path)
{
    const std::string name = std::filesystem::u8path(path.PkToUtf8()).stem().string();
    return PkString::PkFromUtf8(name.data(), static_cast<int>(name.size()));
}

PkString pathBaseName(const PkString &path)
{
    std::string name = std::filesystem::u8path(path.PkToUtf8()).filename().string();
    const std::size_t firstSuffix = name.find('.');
    if (firstSuffix != std::string::npos) {
        name.erase(firstSuffix);
    }
    return PkString::PkFromUtf8(name.data(), static_cast<int>(name.size()));
}

bool hasMyPaintSuffix(const PkString &path)
{
    std::string suffix = std::filesystem::u8path(path.PkToUtf8()).extension().string();
    std::transform(suffix.begin(), suffix.end(), suffix.begin(),
                   [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
    return suffix == ".myb";
}
}

class KisMyPaintPaintOpPreset::Private {

public:
    MyPaintBrush *brush;
    PkImage icon;
    PkByteArray json;
};

KisMyPaintPaintOpPreset::KisMyPaintPaintOpPreset(const PkString &fileName)
    : KisPaintOpPreset(fileName)
    , d(new Private)
{
    d->brush = mypaint_brush_new();
    mypaint_brush_from_defaults(d->brush);
}

KisMyPaintPaintOpPreset::KisMyPaintPaintOpPreset(const KisMyPaintPaintOpPreset &rhs)
    : KisPaintOpPreset(rhs)
    , d(new Private(*rhs.d))
{
    d->brush = mypaint_brush_new();

    if (d->json.isEmpty()) {
        mypaint_brush_from_defaults(d->brush);
    } else {
        MyPaintBrushUtils::parseBrush(d->brush, d->json);
    }
}

KisMyPaintPaintOpPreset::~KisMyPaintPaintOpPreset() {

    mypaint_brush_unref(d->brush);
    delete d;
}

KoResourceSP KisMyPaintPaintOpPreset::clone() const
{
    return toQShared(new KisMyPaintPaintOpPreset(*this));
}

void KisMyPaintPaintOpPreset::setColor(const KoColor color, const KoColorSpace *colorSpace) {

    float hue, saturation, value;
    float r = 0, g = 0, b = 0;
    PkColor dstColor;

    if (colorSpace->colorModelId() == RGBAColorModelID) {
        colorSpace->toQColor(color.data(), &dstColor);
        r = static_cast<float>(dstColor.redF());
        g = static_cast<float>(dstColor.greenF());
        b = static_cast<float>(dstColor.blueF());
    }

    RGBToHSV(r, g, b, &hue, &saturation, &value);

    mypaint_brush_set_base_value(d->brush, MYPAINT_BRUSH_SETTING_COLOR_H, (hue)/360);
    mypaint_brush_set_base_value(d->brush, MYPAINT_BRUSH_SETTING_COLOR_S, (saturation));
    mypaint_brush_set_base_value(d->brush, MYPAINT_BRUSH_SETTING_COLOR_V, (value));
}

void KisMyPaintPaintOpPreset::apply(KisPaintOpSettingsSP settings) {

    if(settings->getProperty(MYPAINT_JSON).isNull()) {
        mypaint_brush_from_defaults(d->brush);
    }
    else {
        PkByteArray ba = settings->getProperty(MYPAINT_JSON).toByteArray();
        MyPaintBrushUtils::parseBrush(d->brush, ba);
    }

    mypaint_brush_new_stroke(d->brush);
}

MyPaintBrush* KisMyPaintPaintOpPreset::brush() {

    return d->brush;
}

bool KisMyPaintPaintOpPreset::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    if (!dev->isSequential())
        dev->seek(0); // ensure we do read *all* the bytes

    std::array<png_byte, 8> signature;
    dev->peek(reinterpret_cast<char*>(signature.data()), 8);

#if PNG_LIBPNG_VER < 10400
    if (png_check_sig(signature, 8)) {
#else
    if (png_sig_cmp(signature.data(), 0, 8) == 0) {
#endif
        // this is a koresource
        if (KisPaintOpPreset::loadFromDevice(dev, resourcesInterface)) {
            apply(settings());
            // correct filename
            const PkString f = filename();
            if (hasMyPaintSuffix(f)) {
                setFilename(pathCompleteBaseName(f).append(KisPaintOpPreset::defaultFileExtension()));
            }
            return true;
        } else {
            warnPlugins << "Failed loading MyPaint preset from KoResource serialization";
            return false;
        }
    }
    
    const PkByteArray ba(dev->readAll());
    d->json = ba;
    // mypaint can handle invalid json files too, so this is the only way to find out if it was correct mypaint file or not...
    // if the json is incorrect, the brush will get the default mypaint brush settings
    // which looks like a round brush with low opacity and high spacing
    bool success = MyPaintBrushUtils::parseBrush(d->brush, ba);
    const float isEraser = mypaint_brush_get_base_value(d->brush, MYPAINT_BRUSH_SETTING_ERASER);

    KisPaintOpSettingsSP s = new KisMyPaintOpSettings(resourcesInterface);
    s->setProperty("paintop", "mypaintbrush");
    s->setProperty("filename", this->filename());
    s->setProperty(MYPAINT_JSON, this->getJsonData());
    s->setProperty("EraserMode", static_cast<int>(std::lround(isEraser)));


    {
        /**
         * See a comment in `namespace deprecated_remove_after_krita6` in
         * MyPaintStandardOptionData.cpp
         */

        auto recoverDeprecatedProperty = [] (auto data, KisPaintOpSettingsSP settings) {
            /// we just round-trip the save operation to save the property
            /// out to the json object

            data.read(settings.data());
            data.write(settings.data());
        };

        recoverDeprecatedProperty(MyPaintRadiusLogarithmicData(), s);
        recoverDeprecatedProperty(MyPaintOpacityData(), s);
        recoverDeprecatedProperty(MyPaintHardnessData(), s);
    }


    if (!metadata().contains("paintopid")) {
        addMetaData("paintopid", "mypaintbrush");
    }

    this->setSettings(s);
    setName(pathBaseName(filename()));
    setValid(success);

    return success;
}

void KisMyPaintPaintOpPreset::updateThumbnail()
{
    d->icon = thumbnail();
}

PkString KisMyPaintPaintOpPreset::thumbnailPath() const
{
    return pathBaseName(filename()) + "_prev.png";
}

PkByteArray KisMyPaintPaintOpPreset::getJsonData() {

    return d->json;
}
