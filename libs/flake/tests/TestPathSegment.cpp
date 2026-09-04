/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008-2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "TestPathSegment.h"
#include <KoPathSegment.h>
#include <KoPathPoint.h>
#include <PkPainterPath.h>
#include <simpletest.h>

void TestPathSegment::segmentAssign()
{
    KoPathSegment s1(PkPointF(0, 0), PkPointF(100, 100));
    KoPathSegment s1Copy = s1;
    QVERIFY(s1 == s1Copy);

    KoPathSegment s2(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 0));
    KoPathSegment s2Copy = s2;
    QVERIFY(s2 == s2Copy);

    KoPathSegment s3(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 100), PkPointF(300, 0));
    KoPathSegment s3Copy = s3;
    QVERIFY(s3 == s3Copy);
}

void TestPathSegment::segmentCopy()
{
    KoPathSegment s1(PkPointF(0, 0), PkPointF(100, 100));
    KoPathSegment s1Copy(s1);
    QVERIFY(s1 == s1Copy);

    KoPathSegment s2(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 0));
    KoPathSegment s2Copy(s2);
    QVERIFY(s2 == s2Copy);

    KoPathSegment s3(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 100), PkPointF(300, 0));
    KoPathSegment s3Copy(s3);
    QVERIFY(s3 == s3Copy);
}

void TestPathSegment::segmentDegree()
{
    KoPathSegment s0(0, 0);
    QCOMPARE(s0.degree(), -1);

    KoPathSegment s1(PkPointF(0, 0), PkPointF(100, 100));
    QCOMPARE(s1.degree(), 1);

    KoPathSegment s2(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 0));
    QCOMPARE(s2.degree(), 2);

    KoPathSegment s3(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 100), PkPointF(300, 0));
    QCOMPARE(s3.degree(), 3);
}

void TestPathSegment::segmentConvexHull()
{
    KoPathSegment s1(PkPointF(0, 0), PkPointF(100, 100));
    PkList<PkPointF> hull1 = s1.convexHull();
    QCOMPARE(hull1.count(), 2);
    QCOMPARE(hull1[0], PkPointF(0, 0));
    QCOMPARE(hull1[1], PkPointF(100, 100));

    KoPathSegment s2(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 0));
    PkList<PkPointF> hull2 = s2.convexHull();
    QCOMPARE(hull2.count(), 3);
    QCOMPARE(hull2[0], PkPointF(0, 0));
    QCOMPARE(hull2[1], PkPointF(100, 100));
    QCOMPARE(hull2[2], PkPointF(200, 0));

    KoPathSegment s3(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 100), PkPointF(300, 0));
    PkList<PkPointF> hull3 = s3.convexHull();
    QCOMPARE(hull3.count(), 4);
    QCOMPARE(hull3[0], PkPointF(0, 0));
    QCOMPARE(hull3[1], PkPointF(100, 100));
    QCOMPARE(hull3[2], PkPointF(200, 100));
    QCOMPARE(hull3[3], PkPointF(300, 0));

    KoPathSegment s4(PkPointF(0, 0), PkPointF(150, 100), PkPointF(150, 50), PkPointF(300, 0));
    PkList<PkPointF> hull4 = s4.convexHull();
    QCOMPARE(hull4.count(), 3);
    QCOMPARE(hull4[0], PkPointF(0, 0));
    QCOMPARE(hull4[1], PkPointF(150, 100));
    QCOMPARE(hull4[2], PkPointF(300, 0));
}

void TestPathSegment::segmentPointAt()
{
    KoPathSegment s1(PkPointF(0, 0), PkPointF(100, 0));
    QCOMPARE(s1.pointAt(0.0), PkPointF(0, 0));
    QCOMPARE(s1.pointAt(0.5), PkPointF(50, 0));
    QCOMPARE(s1.pointAt(1.0), PkPointF(100, 0));

    KoPathSegment s2(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 0));
    QCOMPARE(s2.pointAt(0.0), PkPointF(0, 0));
    QCOMPARE(s2.pointAt(0.5), PkPointF(100, 50));
    QCOMPARE(s2.pointAt(1.0), PkPointF(200, 0));

    KoPathSegment s3(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 100), PkPointF(300, 0));
    QCOMPARE(s3.pointAt(0.0), PkPointF(0, 0));
    QCOMPARE(s3.pointAt(1.0 / 3.0), PkPointF(100, 100*2.0 / 3.0));
    QCOMPARE(s3.pointAt(2.0 / 3.0), PkPointF(200, 100*2.0 / 3.0));
    QCOMPARE(s3.pointAt(1.0), PkPointF(300, 0));
}

void TestPathSegment::segmentSplitAt()
{
    KoPathSegment s1(PkPointF(0, 0), PkPointF(100, 0));
    std::pair<KoPathSegment, KoPathSegment> parts1 = s1.splitAt( 0.5 );
    QCOMPARE(parts1.first.first()->point(), PkPointF(0, 0));
    QCOMPARE(parts1.first.second()->point(), PkPointF(50, 0));
    QCOMPARE(parts1.first.degree(), 1);
    QCOMPARE(parts1.second.first()->point(), PkPointF(50, 0));
    QCOMPARE(parts1.second.second()->point(), PkPointF(100, 0));
    QCOMPARE(parts1.second.degree(), 1);

    PkPainterPath p1;
    p1.moveTo( PkPoint(0, 0) );
    p1.lineTo( PkPointF(100, 0) );
    QCOMPARE( parts1.first.second()->point(), p1.pointAtPercent( 0.5 ) );

    KoPathSegment s2(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 0));
    std::pair<KoPathSegment, KoPathSegment> parts2 = s2.splitAt( 0.5 );
    QCOMPARE(parts2.first.first()->point(), PkPointF(0, 0));
    QCOMPARE(parts2.first.second()->point(), PkPointF(100, 50));
    QCOMPARE(parts2.first.degree(), 2);
    QCOMPARE(parts2.second.first()->point(), PkPointF(100, 50));
    QCOMPARE(parts2.second.second()->point(), PkPointF(200, 0));
    QCOMPARE(parts2.second.degree(), 2);

    PkPainterPath p2;
    p2.moveTo( PkPoint(0, 0) );
    p2.quadTo( PkPointF(100, 100), PkPointF(200, 0) );
    QCOMPARE( parts2.first.second()->point(), p2.pointAtPercent( 0.5 ) );

    KoPathSegment s3(PkPointF(0, 0), PkPointF(100, 100), PkPointF(200, 100), PkPointF(300, 0));
    std::pair<KoPathSegment, KoPathSegment> parts3 = s3.splitAt( 0.5 );
    QCOMPARE(parts3.first.first()->point(), PkPointF(0, 0));
    QCOMPARE(parts3.first.second()->point(), PkPointF(150, 75));
    QCOMPARE(parts3.first.degree(), 3);
    QCOMPARE(parts3.second.first()->point(), PkPointF(150, 75));
    QCOMPARE(parts3.second.second()->point(), PkPointF(300, 0));
    QCOMPARE(parts3.second.degree(), 3);

    PkPainterPath p3;
    p3.moveTo( PkPoint(0, 0) );
    p3.cubicTo( PkPointF(100, 100), PkPointF(200, 100), PkPointF(300, 0) );
    QCOMPARE( parts3.first.second()->point(), p3.pointAtPercent( 0.5 ) );
}

void TestPathSegment::segmentIntersections()
{
    // simple line intersections
    {
        KoPathSegment s1(PkPointF(0, 0), PkPointF(100, 0));
        KoPathSegment s2(PkPointF(50, -50), PkPointF(50, 50));
        PkList<PkPointF> isects = s1.intersections(s2);
        QCOMPARE(isects.count(), 1);
    }
    {
        KoPathSegment s1(PkPointF(0, 0), PkPointF(100, 100));
        KoPathSegment s2(PkPointF(25, 100), PkPointF(75, 50));
        PkList<PkPointF> isects = s1.intersections(s2);
        QCOMPARE(isects.count(), 1);
    }
    // curve intersections
    {
        KoPathSegment s1(PkPointF(0, 0), PkPointF(50, 50), PkPointF(100, -50), PkPointF(150, 0));
        KoPathSegment s2(PkPointF(75, 75), PkPointF(125, 25), PkPointF(25, -25), PkPointF(75, -75));
        PkList<PkPointF> isects = s1.intersections(s2);
        QCOMPARE(isects.count(), 1);
    }
    {
        KoPathSegment s1(PkPointF(0, 0), PkPointF(50, 50), PkPointF(100, -50), PkPointF(150, 0));
        KoPathSegment s2(PkPointF(100, 75), PkPointF(150, 25), PkPointF(50, -25), PkPointF(100, -75));
        PkList<PkPointF> isects = s1.intersections(s2);
        QCOMPARE(isects.count(), 1);
    }
    {
        KoPathSegment s1(PkPointF(0, 0), PkPointF(25, 50), PkPointF(75, 50), PkPointF(100, 0));
        KoPathSegment s2(PkPointF(0, 30), PkPointF(25, -20), PkPointF(75, -20), PkPointF(100, 30));
        PkList<PkPointF> isects = s1.intersections(s2);
        QCOMPARE(isects.count(), 2);
    }
}

void TestPathSegment::segmentLength()
{
    {
        // line segment
        KoPathSegment s(PkPointF(0, 0), PkPointF(100, 0));
        QCOMPARE(s.length(), 100.0);
    }
    {
        // quadric curve segment
        KoPathSegment s1(PkPointF(0, 0), PkPointF(50, 0), PkPointF(100, 0));
        QCOMPARE(s1.length(), 100.0);
        KoPathSegment s2(PkPointF(0, 0), PkPointF(50, 50), PkPointF(100, 0));
        PkPainterPath p2;
        p2.moveTo(PkPointF(0, 0));
        p2.quadTo(PkPointF(50, 50), PkPointF(100, 0));
        // verify that difference is less than 0.5 percent of the length
        QVERIFY(s2.length() - p2.length() < 0.005 * s2.length(0.01));
    }
    {
        // cubic curve segment
        KoPathSegment s1(PkPointF(0, 0), PkPointF(25, 0), PkPointF(75, 0), PkPointF(100, 0));
        QCOMPARE(s1.length(), 100.0);
        KoPathSegment s2(PkPointF(0, 0), PkPointF(25, 50), PkPointF(75, 50), PkPointF(100, 0));
        PkPainterPath p2;
        p2.moveTo(PkPointF(0, 0));
        p2.cubicTo(PkPointF(25, 50), PkPointF(75, 50), PkPointF(100, 0));
        // verify that difference is less than 0.5 percent of the length
        QVERIFY(s2.length() - p2.length() < 0.005 * s2.length(0.01));
    }
}

void TestPathSegment::segmentFlatness()
{
    // line segments
    {
        KoPathSegment s1(PkPointF(0, 0), PkPointF(100, 0));
        QVERIFY(s1.isFlat());
        KoPathSegment s2(PkPointF(0, 0), PkPointF(0, 100));
        QVERIFY(s2.isFlat());
        KoPathSegment s3(PkPointF(0, 0), PkPointF(100, 100));
        QVERIFY(s3.isFlat());
    }
    // quadratic segments
    {
        KoPathSegment s1(PkPointF(0, 0), PkPointF(50, 0), PkPointF(100, 0));
        QVERIFY(s1.isFlat());
        KoPathSegment s2(PkPointF(0, 0), PkPointF(0, 50), PkPointF(0, 100));
        QVERIFY(s2.isFlat());
        KoPathSegment s3(PkPointF(0, 0), PkPointF(50, 50), PkPointF(100, 100));
        QVERIFY(s3.isFlat());
        KoPathSegment s4(PkPointF(0, 0), PkPointF(50, 50), PkPointF(100, 0));
        QVERIFY(! s4.isFlat());
        KoPathSegment s5(PkPointF(0, 0), PkPointF(50, -50), PkPointF(100, 0));
        QVERIFY(! s5.isFlat());
        KoPathSegment s6(PkPointF(0, 0), PkPointF(0, 100), PkPointF(100, 100));
        QVERIFY(! s6.isFlat());
    }
    // cubic segments
    {
        KoPathSegment s1(PkPointF(0, 0), PkPointF(25, 0), PkPointF(75, 0), PkPointF(100, 0));
        QVERIFY(s1.isFlat());
        KoPathSegment s2(PkPointF(0, 0), PkPointF(0, 25), PkPointF(0, 75), PkPointF(0, 100));
        QVERIFY(s2.isFlat());
        KoPathSegment s3(PkPointF(0, 0), PkPointF(25, 25), PkPointF(75, 75), PkPointF(100, 100));
        QVERIFY(s3.isFlat());
        KoPathSegment s4(PkPointF(0, 0), PkPointF(25, 50), PkPointF(75, 50), PkPointF(100, 0));
        QVERIFY(! s4.isFlat());
        KoPathSegment s5(PkPointF(0, 0), PkPointF(25, -50), PkPointF(75, -50), PkPointF(100, 0));
        QVERIFY(! s5.isFlat());
        KoPathSegment s6(PkPointF(0, 0), PkPointF(-25, 75), PkPointF(25, 125), PkPointF(100, 100));
        QVERIFY(! s6.isFlat());
    }
}

void TestPathSegment::nearestPoint()
{
    // line segments
    {
        KoPathSegment s1(PkPointF(0, 0), PkPointF(100, 0));
        QCOMPARE( s1.nearestPoint( PkPointF(0,0) ),    0.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(-20,0) ),  0.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(100,0) ),  1.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(120,0) ),  1.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(50,0) ),   0.5 );
        QCOMPARE( s1.nearestPoint( PkPointF(50,-10) ), 0.5 );
        QCOMPARE( s1.nearestPoint( PkPointF(50,10) ),  0.5 );
        QCOMPARE( s1.nearestPoint( PkPointF(63,50) ),  0.63 );
        QCOMPARE( s1.nearestPoint( PkPointF(63,50) ),  0.63 );
        QCOMPARE( s1.nearestPoint( PkPointF(63,50) ),  0.63 );
        QCOMPARE( s1.nearestPoint( s1.pointAt( 0.25 ) ),  0.25 );
        QCOMPARE( s1.nearestPoint( s1.pointAt( 0.75 ) ),  0.75 );
    }
    // quadratic segments
    {
        KoPathSegment s1(PkPointF(0, 0), PkPointF(50, 0), PkPointF(100, 0));
        QCOMPARE( s1.nearestPoint( PkPointF(0,0) ),    0.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(-20,0) ),  0.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(100,0) ),  1.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(120,0) ),  1.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(50,0) ),   0.5 );
        QCOMPARE( s1.nearestPoint( PkPointF(50,-10) ), 0.5 );
        QCOMPARE( s1.nearestPoint( PkPointF(50,10) ),  0.5 );
        KoPathSegment s2(PkPointF(0, 0), PkPointF(50, 50), PkPointF(100, 0));
        QCOMPARE( s2.nearestPoint( PkPointF(0,0) ),    0.0 );
        QCOMPARE( s2.nearestPoint( PkPointF(-20,0) ),  0.0 );
        QCOMPARE( s2.nearestPoint( PkPointF(100,0) ),  1.0 );
        QCOMPARE( s2.nearestPoint( PkPointF(120,0) ),  1.0 );
        QCOMPARE( s2.nearestPoint( PkPointF(50,50) ),   0.5 );

        QCOMPARE( s2.nearestPoint( s2.pointAt( 0.0 ) ), 0.0 );
        QCOMPARE( s2.nearestPoint( s2.pointAt( 0.25 ) ), 0.25 );
        QCOMPARE( s2.nearestPoint( s2.pointAt( 0.5 ) ), 0.5 );
        QCOMPARE( s2.nearestPoint( s2.pointAt( 0.75 ) ), 0.75 );
        QCOMPARE( s2.nearestPoint( s2.pointAt( 1.0 ) ), 1.0 );
    }
    // cubic segments
    {
        // a flat cubic bezier
        KoPathSegment s1(PkPointF(0, 0), PkPointF(25, 0), PkPointF(75, 0), PkPointF(100, 0));
        QCOMPARE( s1.nearestPoint( PkPointF(0,0) ),    0.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(-20,0) ),  0.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(100,0) ),  1.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(120,0) ),  1.0 );
        QCOMPARE( s1.nearestPoint( PkPointF(50,0) ),   0.5 );
        QCOMPARE( s1.nearestPoint( PkPointF(50,-10) ), 0.5 );
        QCOMPARE( s1.nearestPoint( PkPointF(50,10) ),  0.5 );
        KoPathSegment s2(PkPointF(0, 0), PkPointF(25, 50), PkPointF(75, 50), PkPointF(100, 0));
        QCOMPARE( s2.nearestPoint( PkPointF(0,0) ),    0.0 );
        QCOMPARE( s2.nearestPoint( PkPointF(-20,0) ),  0.0 );
        QCOMPARE( s2.nearestPoint( PkPointF(100,0) ),  1.0 );
        QCOMPARE( s2.nearestPoint( PkPointF(120,0) ),  1.0 );
        QCOMPARE( s2.nearestPoint( PkPointF(50,50) ),   0.5 );

        QCOMPARE( s2.nearestPoint( s2.pointAt( 0.0 ) ), 0.0 );
        QCOMPARE( s2.nearestPoint( s2.pointAt( 0.25 ) ), 0.25 );
        QCOMPARE( s2.nearestPoint( s2.pointAt( 0.5 ) ), 0.5 );
        QCOMPARE( s2.nearestPoint( s2.pointAt( 0.75 ) ), 0.75 );
        QCOMPARE( s2.nearestPoint( s2.pointAt( 1.0 ) ), 1.0 );
    }
}

void TestPathSegment::paramAtLength()
{
    // line segment
    {
        KoPathSegment s1(PkPointF(0,0), PkPointF(100,0));
        QCOMPARE(s1.paramAtLength(0), 0.0);
        QCOMPARE(s1.paramAtLength(100.0), 1.0);
        QCOMPARE(s1.paramAtLength(50.0), 0.5);
        QCOMPARE(s1.paramAtLength(120.0), 1.0);
    }
    // quadratic segments
    {
        // a flat quadratic bezier
        KoPathSegment s1(PkPointF(0, 0), PkPointF(50, 0), PkPointF(100, 0));
        QCOMPARE(s1.paramAtLength(0), 0.0);
        QCOMPARE(s1.paramAtLength(100.0), 1.0);
        QCOMPARE(s1.paramAtLength(120.0), 1.0);
    }
    // cubic segments
    {
        // a flat cubic bezier
        KoPathSegment s1(PkPointF(0, 0), PkPointF(25, 0), PkPointF(75, 0), PkPointF(100, 0));
        QCOMPARE(s1.paramAtLength(0), 0.0);
        QCOMPARE(s1.paramAtLength(100.0), 1.0);
        QCOMPARE(s1.paramAtLength(120.0), 1.0);
    }
}

SIMPLE_TEST_MAIN(TestPathSegment)
