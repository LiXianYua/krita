/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_qimage_pyramid_test.h"

#include <cmath>

#include <PkRgb.h>
#include <simpletest.h>

#include "../kis_qimage_pyramid.h"

void KisQImagePyramidTest::testSmoothRotationUsesTransparentBorder()
{
    PkImage source(10, 10, PkImage::Format_ARGB32);
    source.fill(pkRgba(0, 0, 0, 255));

    KisQImagePyramid pyramid(source);
    const KisDabShape shape(1.0, 1.0, 15.0 * M_PI / 180.0);
    const PkImage transformed = pyramid.createImage(shape, 0.0, 0.0);

    QVERIFY(!transformed.isNull());
    QCOMPARE(transformed.size(), KisQImagePyramid::imageSize(source.size(), shape, 0.0, 0.0));
    QCOMPARE(transformed.format(), PkImage::Format_ARGB32);

    bool foundTransparentPixel = false;
    bool foundOpaquePixel = false;
    bool foundSmoothedEdgePixel = false;
    for (int y = 0; y < transformed.height(); ++y) {
        for (int x = 0; x < transformed.width(); ++x) {
            const int alpha = pkAlpha(transformed.pixel(x, y));
            foundTransparentPixel |= alpha == 0;
            foundOpaquePixel |= alpha == 255;
            foundSmoothedEdgePixel |= alpha > 0 && alpha < 255;
        }
    }

    QVERIFY(foundTransparentPixel);
    QVERIFY(foundOpaquePixel);
    QVERIFY(foundSmoothedEdgePixel);
}

SIMPLE_TEST_MAIN(KisQImagePyramidTest)
