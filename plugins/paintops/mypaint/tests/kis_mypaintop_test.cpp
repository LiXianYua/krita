/*
 *  SPDX-FileCopyrightText: 2020 Ashwin Dhakaita <ashwingpdhakaita@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <KisGlobalResourcesInterface.h>
#include <PkImageFileDecoder.h>
#include <kis_image.h>

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <string>

#include "kis_mypaintop_test.h"
#include "MyPaintSurface.h"
#include "MyPaintPaintOpPreset.h"

namespace
{

PkImage loadFixture(const char *name)
{
    return PkImageFileDecoder::load((std::filesystem::path(FILES_DATA_DIR) / name).string());
}

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

PK_TEST_MAIN(KisMyPaintOpTest)
