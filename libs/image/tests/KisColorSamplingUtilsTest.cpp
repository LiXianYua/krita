/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <KoColor.h>
#include <KoColorSpaceRegistry.h>

#include <KisColorSamplingUtils.h>
#include <kis_paint_device.h>

class KisColorSamplingUtilsTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void samplesAndRestoresWraparoundMode();
    void blendsWithThePreviousColor();
};

void KisColorSamplingUtilsTest::samplesAndRestoresWraparoundMode()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP device(new KisPaintDevice(colorSpace));
    device->fill(QRect(0, 0, 5, 5), KoColor(Qt::red, colorSpace));
    device->setSupportsWraparoundMode(false);

    KoColor sampled(colorSpace);
    QVERIFY(KisColorSamplingUtils::sampleColor(sampled, device, QPoint(2, 2),
                                               nullptr, 3, 100, false));
    QCOMPARE(sampled, KoColor(Qt::red, colorSpace));
    QVERIFY(!device->supportsWraproundMode());

    const KoColor unchanged(Qt::green, colorSpace);
    sampled = unchanged;
    QVERIFY(!KisColorSamplingUtils::sampleColor(sampled, device, QPoint(20, 20),
                                                nullptr, 1, 100, true));
    QCOMPARE(sampled, unchanged);
    QVERIFY(!device->supportsWraproundMode());
}

void KisColorSamplingUtilsTest::blendsWithThePreviousColor()
{
    const KoColorSpace *colorSpace = KoColorSpaceRegistry::instance()->rgb8();
    KisPaintDeviceSP device(new KisPaintDevice(colorSpace));
    device->fill(QRect(0, 0, 3, 3), KoColor(Qt::red, colorSpace));

    const KoColor previous(Qt::blue, colorSpace);
    KoColor sampled(colorSpace);
    QVERIFY(KisColorSamplingUtils::sampleColor(sampled, device, QPoint(1, 1),
                                               &previous, 1, 50, false));

    const QColor result = sampled.toQColor();
    QVERIFY(qAbs(result.red() - 127) <= 1);
    QCOMPARE(result.green(), 0);
    QVERIFY(qAbs(result.blue() - 128) <= 1);
}

SIMPLE_TEST_MAIN(KisColorSamplingUtilsTest)

#include "KisColorSamplingUtilsTest.moc"
