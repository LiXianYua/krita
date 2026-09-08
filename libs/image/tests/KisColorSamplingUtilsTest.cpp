/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkGlobal.h>
#include <simpletest.h>

#include <KoColor.h>
#include <KoColorSpaceRegistry.h>

#include <KisColorSamplingUtils.h>
#include <KisColorSamplerConfig.h>
#include <kis_paint_device.h>

class KisColorSamplingUtilsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void samplesAndRestoresWraparoundMode();
    void blendsWithThePreviousColor();
    void colorSamplerConfigPersistsPkXmlPayload();
};

void KisColorSamplingUtilsTest::samplesAndRestoresWraparoundMode()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP device(new KisPaintDevice(colorSpace));
    device->fill(PkRect(0, 0, 5, 5), KoColor(Pk::red, colorSpace));
    device->setSupportsWraparoundMode(false);

    KoColor sampled(colorSpace);
    QVERIFY(KisColorSamplingUtils::sampleColor(sampled, device, PkPoint(2, 2),
                                               nullptr, 3, 100, false));
    QCOMPARE(sampled, KoColor(Pk::red, colorSpace));
    QVERIFY(!device->supportsWraproundMode());

    const KoColor unchanged(Pk::green, colorSpace);
    sampled = unchanged;
    QVERIFY(!KisColorSamplingUtils::sampleColor(sampled, device, PkPoint(20, 20),
                                                nullptr, 1, 100, true));
    QCOMPARE(sampled, unchanged);
    QVERIFY(!device->supportsWraproundMode());
}

void KisColorSamplingUtilsTest::blendsWithThePreviousColor()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP device(new KisPaintDevice(colorSpace));
    device->fill(PkRect(0, 0, 3, 3), KoColor(Pk::red, colorSpace));

    const KoColor previous(Pk::blue, colorSpace);
    KoColor sampled(colorSpace);
    QVERIFY(KisColorSamplingUtils::sampleColor(sampled, device, PkPoint(1, 1),
                                               &previous, 1, 50, false));

    const PkColor result = sampled.toQColor();
    QVERIFY(pkAbs(result.red() - 127) <= 1);
    QCOMPARE(result.green(), 0);
    QVERIFY(pkAbs(result.blue() - 128) <= 1);
}

void KisColorSamplingUtilsTest::colorSamplerConfigPersistsPkXmlPayload()
{
    KisColorSamplerConfig original;
    original.load();
    struct RestoreConfig {
        KisColorSamplerConfig config;
        ~RestoreConfig() { config.save(); }
    } restore {original};

    KisColorSamplerConfig saved;
    saved.toForegroundColor = false;
    saved.updateColor = false;
    saved.addColorToCurrentPalette = true;
    saved.normaliseValues = true;
    saved.sampleMerged = false;
    saved.radius = 17;
    saved.blend = 63;
    saved.save();

    KisColorSamplerConfig loaded;
    loaded.load();
    QCOMPARE(loaded.toForegroundColor, saved.toForegroundColor);
    QCOMPARE(loaded.updateColor, saved.updateColor);
    QCOMPARE(loaded.addColorToCurrentPalette, saved.addColorToCurrentPalette);
    QCOMPARE(loaded.normaliseValues, saved.normaliseValues);
    QCOMPARE(loaded.sampleMerged, saved.sampleMerged);
    QCOMPARE(loaded.radius, saved.radius);
    QCOMPARE(loaded.blend, saved.blend);
}

SIMPLE_TEST_MAIN(KisColorSamplingUtilsTest)

#include "KisColorSamplingUtilsTest.moc"
