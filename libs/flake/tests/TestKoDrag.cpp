/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TestKoDrag.h"

#include <KoDrag.h>
#include <KoSvgPaste.h>
#include <PkFlakeBridge.h>
#include <PkFileStream.h>

#include <kis_debug.h>
#include <kis_global.h>
#include <svg/SvgParser.h>
#include <KoDocumentResourceManager.h>
#include <KoShapeGroup.h>
#include <QApplication>
#include <QClipboard>
#include <QMimeData>

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

    KoDrag drag;
    drag.setSvg(shapes);
    drag.addToClipboard();

    const QMimeData *mimeData = QApplication::clipboard()->mimeData();
    const bool hasSvg = mimeData->hasFormat(QStringLiteral("image/svg+xml"));
    const QByteArray svg = mimeData->data(QStringLiteral("image/svg+xml"));
    KoSvgPaste paste(PkByteArray(svg.constData(), svg.size()), hasSvg);
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
