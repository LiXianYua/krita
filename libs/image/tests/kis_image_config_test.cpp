/*
 *  SPDX-FileCopyrightText: 2026 Krita contributors
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_image_config_test.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <simpletest.h>

#include <utility>

#include "kis_cubic_curve.h"
#include "kis_global.h"
#include "kis_image_config.h"

void KisImageConfigTest::initTestCase()
{
    m_testKeys = QStringList{
        QStringLiteral("newCursorStyle"),
        QStringLiteral("newOutlineStyle"),
        QStringLiteral("cursorStyleDef"),
        QStringLiteral("tabletPressureCurve"),
        QStringLiteral("ShowOutlineWhilePainting"),
        QStringLiteral("forceAlwaysFullSizedOutline"),
        QStringLiteral("LineSmoothingType"),
        QStringLiteral("compressLayersInKra"),
    };

    KConfigGroup config = KSharedConfig::openConfig()->group(QString());
    const QMap<QString, QString> entries = config.entryMap();
    for (const QString &key : std::as_const(m_testKeys)) {
        if (config.hasKey(key)) {
            m_originalEntries.insert(key, entries.value(key));
        }
    }
    clearTestKeys();
}

void KisImageConfigTest::cleanupTestCase()
{
    clearTestKeys();

    KConfigGroup config = KSharedConfig::openConfig()->group(QString());
    for (auto it = m_originalEntries.cbegin(); it != m_originalEntries.cend(); ++it) {
        config.writeEntry(it.key(), it.value());
    }
    config.sync();
}

void KisImageConfigTest::init()
{
    clearTestKeys();
}

void KisImageConfigTest::cleanup()
{
    clearTestKeys();
}

void KisImageConfigTest::testLegacyCursorStyleMapping_data()
{
    QTest::addColumn<int>("legacyStyle");
    QTest::addColumn<int>("cursorStyle");
    QTest::addColumn<int>("outlineStyle");

    QTest::newRow("tool icon") << int(OLD_CURSOR_STYLE_TOOLICON) << int(CURSOR_STYLE_TOOLICON) << int(OUTLINE_NONE);
    QTest::newRow("crosshair") << int(OLD_CURSOR_STYLE_CROSSHAIR) << int(CURSOR_STYLE_CROSSHAIR) << int(OUTLINE_NONE);
    QTest::newRow("pointer") << int(OLD_CURSOR_STYLE_POINTER) << int(CURSOR_STYLE_POINTER) << int(OUTLINE_NONE);
    QTest::newRow("outline") << int(OLD_CURSOR_STYLE_OUTLINE) << int(CURSOR_STYLE_NO_CURSOR) << int(OUTLINE_FULL);
    QTest::newRow("no cursor") << int(OLD_CURSOR_STYLE_NO_CURSOR) << int(CURSOR_STYLE_NO_CURSOR) << int(OUTLINE_NONE);
    QTest::newRow("small round") << int(OLD_CURSOR_STYLE_SMALL_ROUND) << int(CURSOR_STYLE_SMALL_ROUND) << int(OUTLINE_NONE);
    QTest::newRow("outline dot") << int(OLD_CURSOR_STYLE_OUTLINE_CENTER_DOT) << int(CURSOR_STYLE_SMALL_ROUND) << int(OUTLINE_FULL);
    QTest::newRow("outline cross") << int(OLD_CURSOR_STYLE_OUTLINE_CENTER_CROSS) << int(CURSOR_STYLE_CROSSHAIR) << int(OUTLINE_FULL);
    QTest::newRow("right triangle") << int(OLD_CURSOR_STYLE_TRIANGLE_RIGHTHANDED) << int(CURSOR_STYLE_TRIANGLE_RIGHTHANDED) << int(OUTLINE_NONE);
    QTest::newRow("left triangle") << int(OLD_CURSOR_STYLE_TRIANGLE_LEFTHANDED) << int(CURSOR_STYLE_TRIANGLE_LEFTHANDED) << int(OUTLINE_NONE);
    QTest::newRow("outline right triangle") << int(OLD_CURSOR_STYLE_OUTLINE_TRIANGLE_RIGHTHANDED) << int(CURSOR_STYLE_TRIANGLE_RIGHTHANDED) << int(OUTLINE_FULL);
    QTest::newRow("outline left triangle") << int(OLD_CURSOR_STYLE_OUTLINE_TRIANGLE_LEFTHANDED) << int(CURSOR_STYLE_TRIANGLE_LEFTHANDED) << int(OUTLINE_FULL);
    QTest::newRow("unknown") << 99 << int(CURSOR_STYLE_NO_CURSOR) << int(OUTLINE_FULL);
}

void KisImageConfigTest::testLegacyCursorStyleMapping()
{
    QFETCH(int, legacyStyle);
    QFETCH(int, cursorStyle);
    QFETCH(int, outlineStyle);

    KConfigGroup config = KSharedConfig::openConfig()->group(QString());
    config.writeEntry("cursorStyleDef", legacyStyle);

    KisImageConfig imageConfig(true);
    QCOMPARE(int(imageConfig.newCursorStyle()), cursorStyle);
    QCOMPARE(int(imageConfig.newOutlineStyle()), outlineStyle);
}

void KisImageConfigTest::testModernCursorStyleValidation()
{
    KConfigGroup config = KSharedConfig::openConfig()->group(QString());
    config.writeEntry("newCursorStyle", int(CURSOR_STYLE_BLACK_PIXEL));
    config.writeEntry("newOutlineStyle", int(OUTLINE_TILT));

    KisImageConfig imageConfig(true);
    QCOMPARE(imageConfig.newCursorStyle(), CURSOR_STYLE_BLACK_PIXEL);
    QCOMPARE(imageConfig.newOutlineStyle(), OUTLINE_TILT);
    QCOMPARE(imageConfig.newCursorStyle(true), CURSOR_STYLE_NO_CURSOR);
    QCOMPARE(imageConfig.newOutlineStyle(true), OUTLINE_FULL);

    config.writeEntry("newCursorStyle", int(N_CURSOR_STYLE_SIZE));
    config.writeEntry("newOutlineStyle", int(N_OUTLINE_STYLE_SIZE));
    QCOMPARE(imageConfig.newCursorStyle(), CURSOR_STYLE_NO_CURSOR);
    QCOMPARE(imageConfig.newOutlineStyle(), OUTLINE_FULL);
}

void KisImageConfigTest::testLegacyCursorStyleKeyCleanup()
{
    KConfigGroup config = KSharedConfig::openConfig()->group(QString());
    config.writeEntry("cursorStyleDef", int(OLD_CURSOR_STYLE_OUTLINE));
    config.writeEntry("newCursorStyle", int(CURSOR_STYLE_POINTER));

    KisImageConfig imageConfig(true);
    QCOMPARE(imageConfig.newCursorStyle(), CURSOR_STYLE_POINTER);
    QVERIFY(config.hasKey("cursorStyleDef"));

    config.writeEntry("newOutlineStyle", int(OUTLINE_FULL));
    QCOMPARE(imageConfig.newOutlineStyle(), OUTLINE_FULL);
    QVERIFY(!config.hasKey("cursorStyleDef"));
    QVERIFY(config.hasKey("newCursorStyle"));
    QVERIFY(config.hasKey("newOutlineStyle"));
}

void KisImageConfigTest::testPressureTabletCurve()
{
    KConfigGroup config = KSharedConfig::openConfig()->group(QString());
    KisImageConfig imageConfig(true);

#ifndef Q_OS_ANDROID
    QCOMPARE(imageConfig.pressureTabletCurve(), DEFAULT_CURVE_STRING);
    QCOMPARE(imageConfig.pressureTabletCurve(true), DEFAULT_CURVE_STRING);
#endif

    const QString storedCurve = QStringLiteral("0,0;0.5,1;");
    config.writeEntry("tabletPressureCurve", storedCurve);
    QCOMPARE(imageConfig.pressureTabletCurve(), storedCurve);
#ifndef Q_OS_ANDROID
    QCOMPARE(imageConfig.pressureTabletCurve(true), DEFAULT_CURVE_STRING);
#endif
}

void KisImageConfigTest::testOutlinePaintingFlags()
{
    KConfigGroup config = KSharedConfig::openConfig()->group(QString());
    KisImageConfig imageConfig(true);

    QVERIFY(imageConfig.showOutlineWhilePainting());
    QVERIFY(!imageConfig.forceAlwaysFullSizedOutline());

    config.writeEntry("ShowOutlineWhilePainting", false);
    config.writeEntry("forceAlwaysFullSizedOutline", true);
    QVERIFY(!imageConfig.showOutlineWhilePainting());
    QVERIFY(imageConfig.showOutlineWhilePainting(true));
    QVERIFY(imageConfig.forceAlwaysFullSizedOutline());
    QVERIFY(!imageConfig.forceAlwaysFullSizedOutline(true));
}

void KisImageConfigTest::testLineSmoothingType()
{
    KConfigGroup config = KSharedConfig::openConfig()->group(QString());
    KisImageConfig imageConfig(true);

    QCOMPARE(imageConfig.lineSmoothingType(), 1);
    config.writeEntry("LineSmoothingType", 2);
    QCOMPARE(imageConfig.lineSmoothingType(), 2);
    QCOMPARE(imageConfig.lineSmoothingType(true), 1);
}

void KisImageConfigTest::testCompressKra()
{
    {
        KisImageConfig imageConfig(true);
        QVERIFY(!imageConfig.compressKra());
        QVERIFY(!imageConfig.compressKra(true));
    }

    KisImageConfig imageConfig(true);
    const bool previousCompression = imageConfig.compressKra();
    imageConfig.setCompressKra(true);
    QVERIFY(KisImageConfig(true).compressKra());
    imageConfig.setCompressKra(previousCompression);
    QVERIFY(!KisImageConfig(true).compressKra());

    KConfigGroup config = KSharedConfig::openConfig()->group(QString());
    QVERIFY(config.hasKey("compressLayersInKra"));
}

void KisImageConfigTest::clearTestKeys()
{
    KConfigGroup config = KSharedConfig::openConfig()->group(QString());
    for (const QString &key : std::as_const(m_testKeys)) {
        config.deleteEntry(key);
    }
    config.sync();
}

SIMPLE_TEST_MAIN(KisImageConfigTest)
