/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "TestKoMarkerCollection.h"

#include <simpletest.h>
#include <QFileInfo>
#include <QPainter>
#include <PkPainterPath.h>
#include <KoMarker.h>
#include <KoMarkerCollection.h>
#include <KoPathShape.h>
#include <KoColorBackground.h>

#include "kis_debug.h"
#include <qimage_test_util.h>
#include <testflake.h>

#include <cmath>


void initMarkerCollection(KoMarkerCollection *collection)
{
    QCOMPARE(collection->markers().size(), 1);

    const PkString fileName = TestUtil::fetchDataFileLazy("test_markers.svg");
    QVERIFY(QFileInfo(fileName).exists());

    collection->loadMarkersFromFile(fileName);
    QCOMPARE(collection->markers().size(), 10);
}

void TestKoMarkerCollection::testLoadMarkersFromFile()
{
    KoMarkerCollection collection;
    initMarkerCollection(&collection);
}

void TestKoMarkerCollection::testDeduplication()
{
    PkPainterPath path1;
    path1.addRect(PkRect(5,5,15,15));

    KoPathShape *shape1(KoPathShape::createShapeFromPainterPath(path1));
    shape1->setBackground(PkSharedPointer<KoColorBackground>(new KoColorBackground(Qt::blue)));

    KoMarker *marker(new KoMarker());
    marker->setAutoOrientation(true);
    marker->setShapes({shape1});

    KoMarkerCollection collection;
    QCOMPARE(collection.markers().size(), 1);

    KoMarker *clonedMarker = new KoMarker(*marker);

    collection.addMarker(marker);
    QCOMPARE(collection.markers().size(), 2);

    collection.addMarker(marker);
    QCOMPARE(collection.markers().size(), 2);

    collection.addMarker(clonedMarker);
    QCOMPARE(collection.markers().size(), 2);
}

void testOneMarkerPosition(KoMarker *marker, KoFlake::MarkerPosition position, const PkString &testName)
{
    PkImage image(30,30, PkImage::Format_ARGB32);
    image.fill(0);
    QPainter painter(&image);

    PkPen pen(Pk::black, 2);
    marker->drawPreview(&painter, image.rect(), pen, position);

    QVERIFY(TestUtil::checkQImage(image, "marker_collection", "preview", testName));
}

void TestKoMarkerCollection::testMarkerBounds()
{
    KoMarkerCollection collection;
    initMarkerCollection(&collection);

    PkList<KoMarker*> allMarkers = collection.markers();
    KoMarker *marker = allMarkers[3];

    QCOMPARE(marker->boundingRect(1, 0).toAlignedRect(), PkRect(-7,-3,9,6));
    QCOMPARE(marker->boundingRect(1, M_PI).toAlignedRect(), PkRect(-2,-3,9,6));

    QCOMPARE(marker->outline(1, 0).boundingRect().toAlignedRect(), PkRect(-6,-2,7,4));
    QCOMPARE(marker->outline(1, M_PI).boundingRect().toAlignedRect(), PkRect(-1,-2,7,4));

    testOneMarkerPosition(marker, KoFlake::StartMarker, "start_marker");
    testOneMarkerPosition(marker, KoFlake::MidMarker, "mid_marker");
    testOneMarkerPosition(marker, KoFlake::EndMarker, "end_marker");
}

KISTEST_MAIN(TestKoMarkerCollection)
