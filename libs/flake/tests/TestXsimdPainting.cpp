/*
 *  SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "TestXsimdPainting.h"

#include <QPainter>

#include "KoClipMaskApplicatorBase.h"
#include "KoClipMaskPainter.h"

#include "kistest.h"
#include "SvgParserTestingUtils.h"

void TestXsimdPainting::testKoClipMaskPainting_data()
{
    QTest::addColumn<PkColor>("colorSource");
    QTest::addColumn<PkColor>("colorMask");
    QTest::addColumn<PkColor>("colorFinal");

    QTest::addRow("visibleWhite") << PkColor(255, 255, 255, 255) << PkColor(255, 255, 255, 255)<< PkColor(255, 255, 255, 255);
    QTest::addRow("completelyMasked") << PkColor(255, 255, 255, 255) << PkColor(0, 0, 0, 255) << PkColor(0, 0, 0, 0);
    QTest::addRow("greyMask") << PkColor(255, 255, 255, 255) << PkColor(128, 128, 128, 255) << PkColor(255, 255, 255, 128);
    QTest::addRow("semiTransparent") << PkColor(255, 255, 255, 255) << PkColor(255, 255, 255, 128) << PkColor(255, 255, 255, 128);
    QTest::addRow("semiCyan") << PkColor(255, 255, 255, 255) << PkColor(128, 255, 255, 128) << PkColor(255, 255, 255, 114);
    QTest::addRow("semiMagenta") << PkColor(255, 255, 255, 255) << PkColor(255, 128, 255, 128) << PkColor(255, 255, 255, 82);
    QTest::addRow("semiYellow") << PkColor(255, 255, 255, 255) << PkColor(255, 255, 128, 128) << PkColor(255, 255, 255, 123);
    QTest::addRow("color1") << PkColor(255, 0, 0, 255) << PkColor(64, 128, 255, 128) << PkColor(255, 0, 0, 62);
    QTest::addRow("color2") << PkColor(0, 255, 0, 255) << PkColor(255, 128, 64, 128) << PkColor(0, 255, 0, 75);
    QTest::addRow("color3") << PkColor(0, 0, 255, 255) << PkColor(128, 64, 255, 128) << PkColor(0, 0, 255, 46);
}

void TestXsimdPainting::testKoClipMaskPainting()
{
    QFETCH(PkColor, colorSource);
    QFETCH(PkColor, colorMask);
    QFETCH(PkColor, colorFinal);

    const PkRect imgRect(0, 0, 5, 3);

    PkImage compareImg = PkImage(imgRect.size(), PkImage::Format_ARGB32);
    compareImg.fill(colorFinal);
    PkImage img = PkImage(imgRect.size(), PkImage::Format_ARGB32);
    QPainter p(&img);
    p.save();
    p.setCompositionMode(QPainter::CompositionMode_Source);
    p.fillRect(imgRect, Qt::transparent);
    p.restore();

    KoClipMaskPainter clip (&p, imgRect);
    clip.shapePainter()->fillRect(imgRect, colorSource);
    clip.maskPainter()->fillRect(imgRect, colorMask);

    clip.renderOnGlobalPainter();

    PkPoint errpoint;
    if (!TestUtil::compareQImages(errpoint, img, compareImg)) {
        QFAIL(PkString("XSimd painting test failed, first different pixel: %1,%2 \n").arg(errpoint.x()).arg(errpoint.y()).toLatin1());
    }
}

KISTEST_MAIN(TestXsimdPainting)
