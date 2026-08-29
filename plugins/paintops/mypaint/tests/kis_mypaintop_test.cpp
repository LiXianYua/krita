/*
 *  SPDX-FileCopyrightText: 2020 Ashwin Dhakaita <ashwingpdhakaita@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisGlobalResourcesInterface.h>
#include <PkImageFileDecoder.h>
#include <PkStream.h>
#include <kis_properties_configuration.h>
#include <kis_image.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>

#include "kis_mypaintop_test.h"
#include "MyPaintSurface.h"
#include "MyPaintBrushUtils.h"
#include "MyPaintPaintOpPreset.h"

namespace
{

PkImage loadFixture(const char *name)
{
    return PkImageFileDecoder::load((std::filesystem::path(FILES_DATA_DIR) / name).string());
}

class MemoryReadStream : public PkStream
{
public:
    explicit MemoryReadStream(std::string bytes)
        : m_bytes(std::move(bytes))
    {
        open(ReadOnly);
    }

    pk_int64 size() const override
    {
        return static_cast<pk_int64>(m_bytes.size());
    }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        const pk_int64 available = size() - pos();
        if (available <= 0) {
            return 0;
        }
        const pk_int64 count = std::min(available, maxSize);
        std::memcpy(data, m_bytes.data() + pos(), static_cast<std::size_t>(count));
        return count;
    }

    pk_int64 writeData(const char *, pk_int64) override
    {
        return -1;
    }

private:
    std::string m_bytes;
};

bool findFirstDifferentPixel(const PkImage &expected, const PkImage &actual,
                             PkPoint *differentPixel)
{
    if (expected.size() != actual.size()) {
        *differentPixel = PkPoint(-1, -1);
        return true;
    }

    for (int y = 0; y < expected.height(); ++y) {
        for (int x = 0; x < expected.width(); ++x) {
            const std::uint32_t expectedPixel = expected.pixel(x, y);
            const std::uint32_t actualPixel = actual.pixel(x, y);
            const bool bothTransparent =
                (expectedPixel >> 24) == 0 && (actualPixel >> 24) == 0;
            if (!bothTransparent && expectedPixel != actualPixel) {
                *differentPixel = PkPoint(x, y);
                return true;
            }
        }
    }
    return false;
}

} // namespace

void KisMyPaintOpTest::testDab()
{

    KisPaintDeviceSP dst = new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8());
    KisPainter painter(dst);

    mypaint_brush_new();

    PkScopedPointer<KisMyPaintSurface> surface(new KisMyPaintSurface(&painter, dst));

    surface->draw_dab(surface->surface(), 250, 250, 100, 0, 0, 1, 1, 0.8, 1, 1, 90, 0, 0);

    PkImage img = dst->convertToQImage(0, dst->exactBounds().x(), dst->exactBounds().y(), dst->exactBounds().width(), dst->exactBounds().height());
    const PkImage source = loadFixture("draw_dab.png");
    PK_VERIFY2(!source.isNull(), "draw_dab.png must decode");

    PkPoint errpoint;
    if (findFirstDifferentPixel(source, img, &errpoint)) {
        std::string message;
        if (errpoint.x() < 0) {
            message = "Failed to create identical image: size mismatch";
        } else {
            message =
                "Failed to create identical image, first different pixel: " +
                std::to_string(errpoint.x()) + "," + std::to_string(errpoint.y()) +
                ", expected=" + std::to_string(source.pixel(errpoint.x(), errpoint.y())) +
                ", actual=" + std::to_string(img.pixel(errpoint.x(), errpoint.y()));
        }
        PK_FAIL(message.c_str());
    }
}

void KisMyPaintOpTest::testGetColor()
{

    KisPaintDeviceSP dst = new KisPaintDevice(KoColorSpaceRegistry::instance()->rgb8());

    const PkImage source = loadFixture("draw_dab.png");
    PK_VERIFY2(!source.isNull(), "draw_dab.png must decode");
    dst->convertFromQImage(source, 0);

    KisPainter painter(dst);

    PkScopedPointer<KisMyPaintSurface> surface(new KisMyPaintSurface(&painter, dst));

    surface->draw_dab(surface->surface(), 250, 250, 100, 0, 0, 1, 1, 0.8, 1, 1, 90, 0, 0);

    float r = 0.0f;
    float g = 0.0f;
    float b = 0.0f;
    float a = 0.0f;

    surface->get_color(surface->surface(), 250, 250, 100, &r, &g, &b, &a);

    PK_COMPARE(std::lround(r), 0L);
    PK_COMPARE(std::lround(g), 0L);
    PK_COMPARE(std::lround(b), 1L);
    PK_COMPARE(std::lround(a), 1L);
}

void KisMyPaintOpTest::testLoading()
{
    const std::string path =
        (std::filesystem::path(FILES_DATA_DIR) / "basic.myb").string();
    PkScopedPointer<KisMyPaintPaintOpPreset> brush(
        new KisMyPaintPaintOpPreset(PkString(path.c_str())));
    brush->load(KisGlobalResourcesInterface::instance());
    PK_VERIFY(brush->valid());
}

void KisMyPaintOpTest::testParseBufferIsNulTerminatedWithoutChangingRawBytes()
{
    const char source[] = {'{', '}', static_cast<char>(0xff)};
    const PkByteArray raw(source, static_cast<int>(sizeof(source)));
    const MyPaintBrushUtils::ParseBuffer parseBuffer(raw);

    PK_COMPARE(parseBuffer.size(), raw.size() + 1);
    PK_VERIFY(std::memcmp(parseBuffer.data(), raw.constData(), static_cast<std::size_t>(raw.size())) == 0);
    PK_COMPARE(parseBuffer.data()[raw.size()], '\0');
    PK_COMPARE(raw.size(), static_cast<int>(sizeof(source)));
    PK_VERIFY(std::memcmp(raw.constData(), source, sizeof(source)) == 0);
}

void KisMyPaintOpTest::testSlowTrackingPolicyPreservesFreehandValue()
{
    std::unique_ptr<MyPaintBrush, decltype(&mypaint_brush_unref)>
        brush(mypaint_brush_new(), &mypaint_brush_unref);
    KisPropertiesConfiguration settings;
    settings.setProperty(MyPaintBrushUtils::preserveSlowTrackingKey(), true);
    mypaint_brush_set_base_value(brush.get(), MYPAINT_BRUSH_SETTING_SLOW_TRACKING, 7.25f);

    MyPaintBrushUtils::applySlowTrackingPolicy(brush.get(), &settings);

    PK_COMPARE(mypaint_brush_get_base_value(brush.get(), MYPAINT_BRUSH_SETTING_SLOW_TRACKING), 7.25f);
}

void KisMyPaintOpTest::testSlowTrackingPolicyDefaultsToHeadlessClear()
{
    std::unique_ptr<MyPaintBrush, decltype(&mypaint_brush_unref)>
        brush(mypaint_brush_new(), &mypaint_brush_unref);
    KisPropertiesConfiguration settings;
    mypaint_brush_set_base_value(brush.get(), MYPAINT_BRUSH_SETTING_SLOW_TRACKING, 7.25f);

    MyPaintBrushUtils::applySlowTrackingPolicy(brush.get(), &settings);

    PK_COMPARE(mypaint_brush_get_base_value(brush.get(), MYPAINT_BRUSH_SETTING_SLOW_TRACKING), 0.0f);
}

void KisMyPaintOpTest::testSlowTrackingPolicyIsNotPersisted()
{
    KisPropertiesConfiguration settings;
    settings.setProperty(MyPaintBrushUtils::preserveSlowTrackingKey(), true);
    settings.setPropertyNotSaved(MyPaintBrushUtils::preserveSlowTrackingKey());

    PK_VERIFY(settings.getBool(MyPaintBrushUtils::preserveSlowTrackingKey()));
    PK_VERIFY(settings.toXML().PkToUtf8().find(
                  MyPaintBrushUtils::preserveSlowTrackingKey().PkToUtf8()) == std::string::npos);
}

void KisMyPaintOpTest::testInvalidRawPresetFallsBackWithoutChangingRawBytes()
{
    const std::string rawBytes("not-json\xff", 9);
    MemoryReadStream stream(rawBytes);
    KisMyPaintPaintOpPreset preset("invalid.myb");

    const bool loaded = preset.loadFromDevice(&stream, KisGlobalResourcesInterface::instance());

    PK_VERIFY(!loaded);
    PK_VERIFY(!preset.valid());
    const PkByteArray retained = preset.getJsonData();
    PK_COMPARE(retained.size(), static_cast<int>(rawBytes.size()));
    PK_VERIFY(std::memcmp(retained.constData(), rawBytes.data(), rawBytes.size()) == 0);

    std::unique_ptr<MyPaintBrush, decltype(&mypaint_brush_unref)>
        defaults(mypaint_brush_new(), &mypaint_brush_unref);
    mypaint_brush_from_defaults(defaults.get());
    PK_COMPARE(
        mypaint_brush_get_base_value(preset.brush(), MYPAINT_BRUSH_SETTING_OPAQUE),
        mypaint_brush_get_base_value(defaults.get(), MYPAINT_BRUSH_SETTING_OPAQUE));
}

PK_TEST_MAIN(KisMyPaintOpTest)
