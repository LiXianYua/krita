/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <memory>

#include <KoColorSpaceRegistry.h>
#include <KisReferenceImage.h>
#include <kis_coordinates_converter.h>
#include <kis_paint_device.h>

class PkReferenceImageNativeTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void convertsPaintDevicePixels();
    void computesSaturationWithoutExternalImageHelpers();
};

void PkReferenceImageNativeTest::convertsPaintDevicePixels()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8(
        KoColorSpaceRegistry::instance()->p709SRGBProfile());
    KisPaintDeviceSP device = new KisPaintDevice(colorSpace);
    const quint8 pixels[] = {
        0x20, 0x40, 0x80, 0xa0,
        0xe0, 0x60, 0x10, 0xff,
    };
    device->writeBytes(pixels, 0, 0, 2, 1);

    KisCoordinatesConverter converter;
    std::unique_ptr<KisReferenceImage> reference(
        KisReferenceImage::fromPaintDevice(device, converter));

    PK_VERIFY(reference);
    PK_COMPARE(reference->getImage().size(), PkSize(2, 1));
    PK_COMPARE(reference->getImage().pixel(0, 0), quint32(0xa0804020));
    PK_COMPARE(reference->getImage().pixel(1, 0), quint32(0xff1060e0));
}

void PkReferenceImageNativeTest::computesSaturationWithoutExternalImageHelpers()
{
    PkImage source(1, 1, PkImage::Format_ARGB32);
    source.setPixel(0, 0, 0x80406020u);
    KisCoordinatesConverter converter;
    std::unique_ptr<KisReferenceImage> reference(
        KisReferenceImage::fromQImage(converter, source));

    PK_VERIFY(reference);
    reference->setSaturation(0.0);
    PK_COMPARE(reference->getPixel(PkPointF()).rgba(), quint32(0x804b4b4b));
}

SIMPLE_TEST_MAIN(PkReferenceImageNativeTest)

#include "PkReferenceImageNativeTest.moc"
