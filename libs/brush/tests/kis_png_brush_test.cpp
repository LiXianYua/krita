/*
 *  SPDX-FileCopyrightText: 2007 Boudewijn Rempt boud @valdyas.org
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_png_brush_test.h"

#include <simpletest.h>
#include <KoColorSpaceRegistry.h>
#include "../kis_png_brush.h"
#include <KisGlobalResourcesInterface.h>
#include <PkMemoryStream.h>

void KisPngBrushTest::testLoading()
{
    struct Case { const char *file; int type; int application; bool image; };
    const Case cases[] = {
        {"bw-alpha-transp.png", IMAGE, ALPHAMASK, true},
        {"bw-alpha-solid.png", MASK, ALPHAMASK, false},
        {"bw-no-alpha-solid.png", MASK, ALPHAMASK, false},
        {"color-alpha-solid.png", IMAGE, LIGHTNESSMAP, true},
        {"color-alpha-transp.png", IMAGE, LIGHTNESSMAP, true},
    };
    for (const Case &test : cases) {
        PkScopedPointer<KisPngBrush> brush(
            new KisPngBrush(PkString(FILES_DATA_DIR) + "/" + test.file));
        QVERIFY2(brush->load(KisGlobalResourcesInterface::instance()), test.file);
        QVERIFY(!brush->brushTipImage().isNull());
        QCOMPARE(int(brush->brushType()), test.type);
        QCOMPARE(int(brush->brushApplication()), test.application);
        QCOMPARE(brush->isImageType(), test.image);
        QVERIFY(brush->metadata().contains(KisBrush::brushTypeMetaDataKey));
        QCOMPARE(brush->metadata().value(KisBrush::brushTypeMetaDataKey).toBool(),
                 brush->isImageType());
    }
}

void KisPngBrushTest::testEmbeddedMetadata()
{
    static const unsigned char png[] = {
        0x89,0x50,0x4e,0x47,0x0d,0x0a,0x1a,0x0a,0x00,0x00,0x00,0x0d,
        0x49,0x48,0x44,0x52,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x01,
        0x08,0x06,0x00,0x00,0x00,0xf4,0x22,0x7f,0x8a,0x00,0x00,0x00,
        0x13,0x74,0x45,0x58,0x74,0x62,0x72,0x75,0x73,0x68,0x5f,0x73,
        0x70,0x61,0x63,0x69,0x6e,0x67,0x00,0x30,0x2e,0x33,0x37,0x35,
        0x55,0xbd,0x5b,0x5e,0x00,0x00,0x00,0x14,0x74,0x45,0x58,0x74,
        0x62,0x72,0x75,0x73,0x68,0x5f,0x6e,0x61,0x6d,0x65,0x00,0xe7,
        0x94,0xbb,0xe7,0xac,0x94,0xe5,0x90,0x8d,0x65,0xa5,0xb3,0x64,
        0x00,0x00,0x00,0x0e,0x49,0x44,0x41,0x54,0x78,0x9c,0x63,0x60,
        0x60,0x60,0xf8,0x0f,0x02,0x00,0x0e,0xfa,0x04,0xfc,0x45,0xb0,
        0x1d,0x8b,0x00,0x00,0x00,0x00,0x49,0x45,0x4e,0x44,0xae,0x42,
        0x60,0x82,
    };
    PkMemoryStream stream;
    QVERIFY(stream.open(PkStream::ReadWrite));
    QCOMPARE(stream.write(reinterpret_cast<const char *>(png), sizeof(png)),
             PkStream::pk_int64(sizeof(png)));
    QVERIFY(stream.seek(0));

    KisPngBrush brush("/tmp/fallback.png");
    QVERIFY(brush.loadFromDevice(&stream, KisGlobalResourcesInterface::instance()));
    QCOMPARE(brush.spacing(), 0.375);
    QCOMPARE(brush.name(), PkString::fromUtf8(u8"画笔名"));
}

SIMPLE_TEST_MAIN(KisPngBrushTest)
