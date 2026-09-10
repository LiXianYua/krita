/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TestKoDrag.h"

#include <KoDrag.h>
#include <KoSvgPaste.h>
#include <PkClipboardData.h>
#include <PkFlakeBridge.h>
#include <PkFileStream.h>

#include <kis_debug.h>
#include <kis_global.h>
#include <svg/SvgParser.h>
#include <KoDocumentResourceManager.h>
#include <KoShapeGroup.h>

#include <qimage_test_util.h>

void TestKoDrag::test()
{
    const PkString fileName = toPkString(TestUtil::fetchDataFileLazy("test_svg_file.svg"));
    QVERIFY(!fileName.isEmpty());

    PkFileStream testShapes(fileName);
    KIS_ASSERT(testShapes.open(PkStream::ReadOnly));

    PkXmlDocument doc = SvgParser::createDocumentFromSvg(&testShapes);

    KoDocumentResourceManager resourceManager;
    SvgParser parser(&resourceManager);
    parser.setResolution(PkRectF(0, 0, 30, 30) /* px */, 72 /* ppi */);

    PkSizeF fragmentSize;
    PkList<KoShape*> shapes = parser.parseSvg(doc.documentElement(), &fragmentSize);
    QCOMPARE(fragmentSize, PkSizeF(30,30));

    {
        QCOMPARE(shapes.size(), 1);

        KoShapeGroup *layer = dynamic_cast<KoShapeGroup*>(shapes.first());
        QVERIFY(layer);
        QCOMPARE(layer->shapeCount(), 2);

        QCOMPARE(KoShape::absoluteOutlineRect(shapes).toAlignedRect(), PkRect(6,6,19,18));
    }

    {
        // A KoDrag that was handed nothing carries an empty payload: every
        // format flag is unset and no field holds bytes.
        KoDrag empty;
        const PkClipboardData payload = empty.takeClipboardData();
        QVERIFY(!payload.hasText);
        QVERIFY(!payload.hasHtml);
        QVERIFY(!payload.hasSvg);
        QVERIFY(payload.text.isEmpty());
        QVERIFY(payload.html.isEmpty());
        QVERIFY(payload.svg.isEmpty());
    }

    {
        // The setData() path fills the same payload the format flags index.
        KoDrag dataDrag;
        dataDrag.setData(PkString("text/plain"), PkString("Hello").toUtf8());
        dataDrag.setData(PkString("text/html"), PkString("<p>Hello</p>").toUtf8());

        const PkClipboardData payload = dataDrag.takeClipboardData();
        QVERIFY(payload.hasText);
        QVERIFY(payload.hasHtml);
        QVERIFY(!payload.hasSvg);
        QCOMPARE(payload.text, PkString("Hello"));
        QCOMPARE(payload.html, PkString("<p>Hello</p>"));
        QVERIFY(payload.svg.isEmpty());
    }

    KoDrag drag;
    QVERIFY(drag.setSvg(shapes));

    const PkClipboardData payload = drag.takeClipboardData();
    QVERIFY(payload.hasSvg);
    QVERIFY(!payload.svg.isEmpty());

    {
        // Taking the payload transfers its ownership: the KoDrag no longer
        // holds it, and nothing was refilled in between.
        const PkClipboardData secondTake = drag.takeClipboardData();
        QVERIFY(!secondTake.hasSvg);
        QVERIFY(secondTake.svg.isEmpty());
    }

    KoSvgPaste paste(payload.svg, payload.hasSvg);
    QVERIFY(paste.hasShapes());

    PkList<KoShape*> newShapes = paste.fetchShapes(PkRectF(0,0,15,15) /* px */, 144 /* ppi */, &fragmentSize);

    {
        QCOMPARE(newShapes.size(), 1);

        KoShapeGroup *layer = dynamic_cast<KoShapeGroup*>(newShapes.first());
        QVERIFY(layer);
        QCOMPARE(layer->shapeCount(), 2);

        QCOMPARE(fragmentSize.toSize(), PkSize(57, 55));
        QCOMPARE(KoShape::absoluteOutlineRect(newShapes).toAlignedRect(), PkRect(6,6,19,18));
    }


    qDeleteAll(shapes);
    qDeleteAll(newShapes);
}


SIMPLE_TEST_MAIN(TestKoDrag)
