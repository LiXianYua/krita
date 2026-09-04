/*
    Copyright (C) 2012 <hanna.et.scott@gmail.com> 

    SPDX-License-Identifier: LGPL-2.1-or-later

*/

#include "TestSnapStrategy.h"
#include "KoPathPoint.h"
#include "KoPathShape.h"
#include "KoShapeControllerBase.h"
#include "KoSnapProxy.h"
#include "KoSnapStrategy.h"
#include "KisSnapPointStrategy.h"
#include "KoViewConverter.h"
#include "MockShapes.h"
#include <PkPainterPath.h>
#include <simpletest.h>
#include <testflake.h>

//#include <PointProperties.h>
#include <KoSnapData.h>

void TestSnapStrategy::testCustomPointSnap()
{
    KisSnapPointStrategy strategy;
    strategy.addPoint(PkPointF(10.0, 10.0));
    strategy.addPoint(PkPointF(2.0, 2.0));

    QVERIFY(strategy.snap(PkPointF(1.5, 1.5), nullptr, 2.0));
    QCOMPARE(strategy.snappedPosition(), PkPointF(2.0, 2.0));
    QVERIFY(!strategy.snap(PkPointF(20.0, 20.0), nullptr, 2.0));
}

void TestSnapStrategy::testOrthogonalSnap()
{
    //Test case one - expected not to snap

    OrthogonalSnapStrategy toTest;
    const PkPointF paramMousePosition;
    MockShapeController fakeShapeControllerBase;
    MockCanvas fakeKoCanvasBase(&fakeShapeControllerBase); //the shapeManager() function of this will be called
    KoSnapGuide aKoSnapGuide(&fakeKoCanvasBase); //the call that will be made to the snap guide created is m_snapGuide->canvas()->shapeManager()->shapes();
    KoSnapProxy paramProxy(&aKoSnapGuide);  //param proxy will have no shapes hence it will not snap
    qreal paramSnapDistance = 0;

    bool didSnap = toTest.snap(paramMousePosition, &paramProxy, paramSnapDistance);
    QVERIFY(!didSnap);


    //Second test case - makes sure the there are shapes in the fakeShapeControllerBase thus it should snap
    OrthogonalSnapStrategy toTestTwo;
    //paramMousePosition must be within paramSnapDistance of the points in firstSnapPointList
    const PkPointF paramMousePositionTwo(3,3);
    MockShapeController fakeShapeControllerBaseTwo;

    //This call will be made on the paramProxy: proxy->pointsFromShape(shape) which in turn
    //will make this call shape->snapData().snapPoints(); so the shapes have to have snapPoints
    //In order to have snapPoints we have to use the call
    //shape->snapData().setSnapPoints() for each fakeShape, where we send in a const
    //PkList<PkPointF> &snapPoints in order to have snapPoints to iterate - which is the only
    //way to change the value of minHorzDist and minVertDist in KoSnapStrategy.cpp so it
    //differs from HUGE_VAL - i.e. gives us the true value for the snap function.

    //creating the lists of points
    //example PkList<PkPointF> pts; pts.push_back(PkPointF(0.2, 0.3)); pts.push_back(PkPointF(0.5, 0.7));


    MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo); //the shapeManager() function of this will be called

    KoShapeManager *fakeShapeManager = fakeKoCanvasBaseTwo.shapeManager();
    MockShape fakeShapeOne;
    PkList<PkPointF> firstSnapPointList;
    firstSnapPointList.push_back(PkPointF(1,2));
    firstSnapPointList.push_back(PkPointF(2,2));
    firstSnapPointList.push_back(PkPointF(3,2));
    firstSnapPointList.push_back(PkPointF(4,2));

    fakeShapeOne.snapData().setSnapPoints(firstSnapPointList);
    fakeShapeOne.isVisible(true);
    fakeShapeManager->addShape(&fakeShapeOne);
    KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo); //the call that will be made to the snap guide created is m_snapGuide->canvas()->shapeManager()->shapes();
    KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);  //param proxy will have shapes now

    //Make sure at least one point in firstSnapPointList is within this distance of
    //paramMousePoint to trigger the branches if (dx < minHorzDist && dx < maxSnapDistance)
    //and if (dy < minVertDist && dy < maxSnapDistance) WHICH IS WHERE minVertDist and minHorzDist
    //ARE CHANGED FROM HUGE_VAL
    qreal paramSnapDistanceTwo = 4;
    bool didSnapTwo = toTestTwo.snap(paramMousePositionTwo, &paramProxyTwo, paramSnapDistanceTwo);
    QVERIFY(didSnapTwo);

    // don't forget to remove the shape from the shape manager before exiting!
    fakeShapeManager->remove(&fakeShapeOne);
}
void TestSnapStrategy::testNodeSnap()
{

    //Test case one - expected to not snap
    NodeSnapStrategy toTest;
    const PkPointF paramMousePos;
    MockShapeController fakeShapeControllerBase;
    MockCanvas fakeKoCanvasBase(&fakeShapeControllerBase);
    KoSnapGuide aKoSnapGuide(&fakeKoCanvasBase);
    KoSnapProxy paramProxy(&aKoSnapGuide);
    qreal paramSnapDistance = 0;

    bool didSnap = toTest.snap(paramMousePos, &paramProxy, paramSnapDistance);
    QVERIFY(!didSnap);

    //Test case two - exercising the branches by putting a shape and snap points into the ShapeManager
    NodeSnapStrategy toTestTwo;
    const PkPointF paramMousePosTwo;
    MockShapeController fakeShapeControllerBaseTwo;
    MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo);
    KoShapeManager *fakeShapeManager = fakeKoCanvasBaseTwo.shapeManager();
    MockShape fakeShapeOne;
    PkList<PkPointF> firstSnapPointList;
    firstSnapPointList.push_back(PkPointF(1,2));
    firstSnapPointList.push_back(PkPointF(2,2));
    firstSnapPointList.push_back(PkPointF(3,2));
    firstSnapPointList.push_back(PkPointF(4,2));

    qreal paramSnapDistanceTwo = 4;

    fakeShapeOne.snapData().setSnapPoints(firstSnapPointList);
    fakeShapeOne.isVisible(true);
    fakeShapeManager->addShape(&fakeShapeOne);
    KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo);
    KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);

    bool didSnapTwo = toTestTwo.snap(paramMousePosTwo, &paramProxyTwo, paramSnapDistanceTwo);
    QVERIFY(didSnapTwo);

    // don't forget to remove the shape from the shape manager before exiting!
    fakeShapeManager->remove(&fakeShapeOne);
}
void TestSnapStrategy::testExtensionSnap()
{
    //bool ExtensionSnapStrategy::snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance)
    ExtensionSnapStrategy toTest;
    const PkPointF paramMousePos;
    MockShapeController fakeShapeControllerBase;
    MockCanvas fakeKoCanvasBase(&fakeShapeControllerBase);
    KoSnapGuide aKoSnapGuide(&fakeKoCanvasBase);
    KoSnapProxy paramProxy(&aKoSnapGuide);
    qreal paramSnapDistance = 0;
    bool didSnap = toTest.snap(paramMousePos, &paramProxy, paramSnapDistance);
    QVERIFY(!didSnap);

    //Second test case - testing the snap by providing ShapeManager with a shape that has snap points and a path
    //fakeShapeOne needs at least one subpath that is open in order to change the values of minDistances
    //which in turn opens the path where it is possible to get a true bool value back from the snap function
    // KoPathPointIndex openSubpath(const KoPathPointIndex &pointIndex); in KoPathShape needs to be called
    ExtensionSnapStrategy toTestTwo;
    const PkPointF paramMousePosTwo;
    MockShapeController fakeShapeControllerBaseTwo;
    MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo);
    KoShapeManager *fakeShapeManager = fakeKoCanvasBaseTwo.shapeManager();
    KoPathShape fakeShapeOne;
    PkList<PkPointF> firstSnapPointList;
    firstSnapPointList.push_back(PkPointF(1,2));
    firstSnapPointList.push_back(PkPointF(2,2));
    firstSnapPointList.push_back(PkPointF(3,2));
    firstSnapPointList.push_back(PkPointF(4,2));

    qreal paramSnapDistanceTwo = 4;
    fakeShapeOne.snapData().setSnapPoints(firstSnapPointList);
    fakeShapeOne.isVisible(true);

    PkPointF firstPoint(0,2);
    PkPointF secondPoint(1,2);
    PkPointF thirdPoint(2,3);
    PkPointF fourthPoint(3,4);

    fakeShapeOne.moveTo(firstPoint);
    fakeShapeOne.lineTo(secondPoint);
    fakeShapeOne.lineTo(thirdPoint);
    fakeShapeOne.lineTo(fourthPoint);

    fakeShapeManager->addShape(&fakeShapeOne);
    KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo);
    KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);

    bool didSnapTwo = toTest.snap(paramMousePosTwo, &paramProxyTwo, paramSnapDistanceTwo);
    QVERIFY(didSnapTwo);

    // don't forget to remove the shape from the shape manager before exiting!
    fakeShapeManager->remove(&fakeShapeOne);
}
void TestSnapStrategy::testIntersectionSnap()
{
    //Testing so it does not work without a path
    IntersectionSnapStrategy toTest;
    const PkPointF paramMousePos;
    MockShapeController fakeShapeControllerBase;
    MockCanvas fakeKoCanvasBase(&fakeShapeControllerBase);
    KoSnapGuide aKoSnapGuide(&fakeKoCanvasBase);
    KoSnapProxy paramProxy(&aKoSnapGuide);
    qreal paramSnapDistance = 0;
    bool didSnap = toTest.snap(paramMousePos, &paramProxy, paramSnapDistance);
    QVERIFY(!didSnap);

    //Exercising the working snap by providing the shape manager with three path shapes
    //In order for this test to work the shapeManager has to have more than one fakeShape in it
    //(requirement in PkList<KoShape *> KoShapeManager::shapesAt(const PkRectF &rect, bool omitHiddenShapes)
    //And both shapes have to be not-visible
    IntersectionSnapStrategy toTestTwo;
    const PkPointF paramMousePosTwo;
    MockShapeController fakeShapeControllerBaseTwo;
    MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo);
    KoShapeManager *ShapeManager = fakeKoCanvasBaseTwo.shapeManager();

    qreal paramSnapDistanceTwo = 8;
    KoPathShape pathShapeOne;
    PkList<PkPointF> firstSnapPointList;

    pathShapeOne.moveTo(PkPointF(1,2));
    pathShapeOne.lineTo(PkPointF(2,2));
    pathShapeOne.lineTo(PkPointF(3,2));
    pathShapeOne.lineTo(PkPointF(4,2));

    //pathShapeOne.snapData().setSnapPoints(firstSnapPointList);

    pathShapeOne.isVisible(true);
    ShapeManager->addShape(&pathShapeOne);

    KoPathShape pathShapeTwo;
    PkList<PkPointF> secondSnapPointList;

    pathShapeTwo.moveTo(PkPointF(1,1));
    pathShapeTwo.lineTo(PkPointF(2,2));
    pathShapeTwo.lineTo(PkPointF(3,3));
    pathShapeTwo.lineTo(PkPointF(4,4));

    //pathShapeTwo.snapData().setSnapPoints(secondSnapPointList);

    pathShapeTwo.isVisible(true);
    ShapeManager->addShape(&pathShapeTwo);

    KoPathShape pathShapeThree;
    PkList<PkPointF> thirdSnapPointList;
    pathShapeThree.moveTo(PkPointF(5,5));
    pathShapeThree.lineTo(PkPointF(6,6));
    pathShapeThree.lineTo(PkPointF(7,7));
    pathShapeThree.lineTo(PkPointF(8,8));

    pathShapeThree.isVisible(true);
    ShapeManager->addShape(&pathShapeThree);

    KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo);
    KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);
    bool didSnapTwo = toTestTwo.snap(paramMousePosTwo, &paramProxyTwo, paramSnapDistanceTwo);
    QVERIFY(didSnapTwo);

    // don't forget to remove the shape from the shape manager before exiting!
    ShapeManager->remove(&pathShapeOne);
    ShapeManager->remove(&pathShapeTwo);
    ShapeManager->remove(&pathShapeThree);
}
void TestSnapStrategy::testGridSnap()
{
    //This test is the default case - meant to fail since the grid of the SnapGuide is not set
    GridSnapStrategy toTest;
    const PkPointF paramMousePos;
    MockShapeController fakeShapeControllerBase;
    MockCanvas fakeKoCanvasBase(&fakeShapeControllerBase);
    KoSnapGuide aKoSnapGuide(&fakeKoCanvasBase);
    KoSnapProxy paramProxy(&aKoSnapGuide);
    qreal paramSnapDistance = 0;
    bool didSnap = toTest.snap(paramMousePos, &paramProxy, paramSnapDistance);
    QVERIFY(!didSnap);

    //This test tests the snapping by providing the SnapGuide with a grid to snap against
    GridSnapStrategy toTestTwo;
    const PkPointF paramMousePosTwo(40,60);
    MockShapeController fakeShapeControllerBaseTwo;
    MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo);
    fakeKoCanvasBaseTwo.setHorz(10);
    fakeKoCanvasBaseTwo.setVert(8);
    KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo);
    KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);
    qreal paramSnapDistanceTwo = 8;
    bool didSnapTwo = toTestTwo.snap(paramMousePosTwo, &paramProxyTwo, paramSnapDistanceTwo);
    QVERIFY(didSnapTwo);

}
void TestSnapStrategy::testBoundingBoxSnap()
{
    //Tests so the snap does not work when there is no shape with a path
    BoundingBoxSnapStrategy toTest;
    const PkPointF paramMousePos;
    MockShapeController fakeShapeControllerBase;
    MockCanvas fakeKoCanvasBase(&fakeShapeControllerBase);
    KoSnapGuide aKoSnapGuide(&fakeKoCanvasBase);
    KoSnapProxy paramProxy(&aKoSnapGuide);
    qreal paramSnapDistance = 0;
    bool didSnap = toTest.snap(paramMousePos, &paramProxy, paramSnapDistance);
    QVERIFY(!didSnap);

    //tests the snap by providing three path shapes to the shape manager
    BoundingBoxSnapStrategy toTestTwo;
    const PkPointF paramMousePosTwo;
    MockShapeController fakeShapeControllerBaseTwo;
    MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo);
    KoShapeManager *ShapeManager = fakeKoCanvasBaseTwo.shapeManager();

    qreal paramSnapDistanceTwo = 8;
    KoPathShape pathShapeOne;
    PkList<PkPointF> firstSnapPointList;

    pathShapeOne.moveTo(PkPointF(1,2));
    pathShapeOne.lineTo(PkPointF(2,2));
    pathShapeOne.lineTo(PkPointF(3,2));
    pathShapeOne.lineTo(PkPointF(4,2));

    pathShapeOne.isVisible(true);
    ShapeManager->addShape(&pathShapeOne);

    KoPathShape pathShapeTwo;
    PkList<PkPointF> secondSnapPointList;

    pathShapeTwo.moveTo(PkPointF(1,1));
    pathShapeTwo.lineTo(PkPointF(2,2));
    pathShapeTwo.lineTo(PkPointF(3,3));
    pathShapeTwo.lineTo(PkPointF(4,4));

    pathShapeTwo.isVisible(true);
    ShapeManager->addShape(&pathShapeTwo);

    KoPathShape pathShapeThree;
    PkList<PkPointF> thirdSnapPointList;
    pathShapeThree.moveTo(PkPointF(5,5));
    pathShapeThree.lineTo(PkPointF(6,6));
    pathShapeThree.lineTo(PkPointF(7,7));
    pathShapeThree.lineTo(PkPointF(8,8));

    pathShapeThree.isVisible(true);
    ShapeManager->addShape(&pathShapeThree);

    KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo);
    KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);
    bool didSnapTwo = toTestTwo.snap(paramMousePosTwo, &paramProxyTwo, paramSnapDistanceTwo);
    QVERIFY(didSnapTwo);

    // don't forget to remove the shape from the shape manager before exiting!
    ShapeManager->remove(&pathShapeOne);
    ShapeManager->remove(&pathShapeTwo);
    ShapeManager->remove(&pathShapeThree);
}
void TestSnapStrategy::testLineGuideSnap()
{
    // KoGuides data has been moved into Krita
    // 
    // //Testing so the snap does not work without horizontal and vertical lines
    // LineGuideSnapStrategy toTest;
    // const PkPointF paramMousePos;
    // MockShapeController fakeShapeControllerBase;
    // MockCanvas fakeKoCanvasBase(&fakeShapeControllerBase);
    // KoSnapGuide aKoSnapGuide(&fakeKoCanvasBase);
    // KoSnapProxy paramProxy(&aKoSnapGuide);
    // qreal paramSnapDistance = 0;
    // bool didSnap = toTest.snap(paramMousePos, &paramProxy, paramSnapDistance);
    // QVERIFY(!didSnap);

    // //Test case that covers the path of the snap by providing horizontal and vertical lines for the GuidesData
    // LineGuideSnapStrategy toTestTwo;
    // const PkPointF paramMousePosTwo;
    // MockShapeController fakeShapeControllerBaseTwo;
    // MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo);

    // KoGuidesData guidesData;

    // PkList<qreal> horzLines;
    // horzLines.push_back(2);
    // horzLines.push_back(3);
    // horzLines.push_back(4);
    // horzLines.push_back(5);

    // PkList<qreal> vertLines;
    // vertLines.push_back(1);
    // vertLines.push_back(2);
    // vertLines.push_back(3);
    // vertLines.push_back(4);

    // guidesData.setHorizontalGuideLines(horzLines);
    // guidesData.setVerticalGuideLines(vertLines);
    // fakeKoCanvasBaseTwo.setGuidesData(&guidesData);
    // qreal paramSnapDistanceTwo = 8;
    // KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo);
    // KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);
    // bool didSnapTwo = toTestTwo.snap(paramMousePosTwo, &paramProxyTwo, paramSnapDistanceTwo);
    // QVERIFY(didSnapTwo);
}

void TestSnapStrategy::testOrthogonalDecoration()
{
    //Making sure the decoration is created but is empty
    OrthogonalSnapStrategy toTestTwo;
    const PkPointF paramMousePositionTwo(3,3);
    MockShapeController fakeShapeControllerBaseTwo;
    MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo);

    KoShapeManager *fakeShapeManager = fakeKoCanvasBaseTwo.shapeManager();
    MockShape fakeShapeOne;
    PkList<PkPointF> firstSnapPointList;
    firstSnapPointList.push_back(PkPointF(1,2));
    firstSnapPointList.push_back(PkPointF(2,2));
    firstSnapPointList.push_back(PkPointF(3,2));
    firstSnapPointList.push_back(PkPointF(4,2));

    fakeShapeOne.snapData().setSnapPoints(firstSnapPointList);
    fakeShapeOne.isVisible(true);
    fakeShapeManager->addShape(&fakeShapeOne);
    KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo);
    KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);

    //Make sure at least one point in firstSnapPointList is within this distance of
    //paramMousePoint to trigger the branches if (dx < minHorzDist && dx < maxSnapDistance)
    //and if (dy < minVertDist && dy < maxSnapDistance) WHICH IS WHERE minVertDist and minHorzDist
    //ARE CHANGED FROM HUGE_VAL
    qreal paramSnapDistanceTwo = 4;
    toTestTwo.snap(paramMousePositionTwo, &paramProxyTwo, paramSnapDistanceTwo);

    KoViewConverter irrelevantParameter;
    PkPainterPath resultingDecoration = toTestTwo.decoration(irrelevantParameter);

    QVERIFY( resultingDecoration.isEmpty() );

    // don't forget to remove the shape from the shape manager before exiting!
    fakeShapeManager->remove(&fakeShapeOne);

}
void TestSnapStrategy::testNodeDecoration()
{
    //Tests so the decoration returns a rect which is inside the "standard outer rect"
    NodeSnapStrategy toTest;
    KoViewConverter irrelevantParameter;
    PkRectF originalRect = PkRectF(-5.5, -5.5, 11, 11);
    PkPainterPath resultingDecoration = toTest.decoration(irrelevantParameter);
    PkRectF rectInsidePath = resultingDecoration.boundingRect();
    QVERIFY(originalRect==rectInsidePath);
}
void TestSnapStrategy::testExtensionDecoration()
{
    //Tests the decoration is exercised by providing it with path
    //fakeShapeOne needs at least one subpath that is open in order to change the values of minDistances
    //which in turn opens the path where it is possible to get a true bool value back from the snap function
    // KoPathPointIndex openSubpath(const KoPathPointIndex &pointIndex); in KoPathShape needs to be called

    ExtensionSnapStrategy toTestTwo;
    const PkPointF paramMousePosTwo;
    MockShapeController fakeShapeControllerBaseTwo;
    MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo);
    KoShapeManager *fakeShapeManager = fakeKoCanvasBaseTwo.shapeManager();
    KoPathShape fakeShapeOne;
    PkList<PkPointF> firstSnapPointList;
    firstSnapPointList.push_back(PkPointF(1,2));
    firstSnapPointList.push_back(PkPointF(2,2));
    firstSnapPointList.push_back(PkPointF(3,2));
    firstSnapPointList.push_back(PkPointF(4,2));

    qreal paramSnapDistanceTwo = 4;
    fakeShapeOne.snapData().setSnapPoints(firstSnapPointList);
    fakeShapeOne.isVisible(true);

    PkPointF firstPoint(0,2);
    PkPointF secondPoint(1,2);
    PkPointF thirdPoint(2,3);
    PkPointF fourthPoint(3,4);

    fakeShapeOne.moveTo(firstPoint);
    fakeShapeOne.lineTo(secondPoint);
    fakeShapeOne.lineTo(thirdPoint);
    fakeShapeOne.lineTo(fourthPoint);

    fakeShapeManager->addShape(&fakeShapeOne);
    KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo);
    KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);

    toTestTwo.snap(paramMousePosTwo, &paramProxyTwo, paramSnapDistanceTwo);

    const KoViewConverter aConverter;
    PkPainterPath resultingDecoration = toTestTwo.decoration(aConverter);
    PkPointF resultDecorationLastPoint = resultingDecoration.currentPosition();

    QVERIFY( resultDecorationLastPoint == PkPointF(0,2) );

    // don't forget to remove the shape from the shape manager before exiting!
    fakeShapeManager->remove(&fakeShapeOne);
}
void TestSnapStrategy::testIntersectionDecoration()
{
    //Tests the decoration by making sure that the returned rect is within the "standard outer rect"
    IntersectionSnapStrategy toTest;
    KoViewConverter irrelevantParameter;
    PkRectF originalRect = PkRectF(-5.5,-5.5,11,11); //std outer rect
    PkPainterPath resultingDecoration = toTest.decoration(irrelevantParameter);
    PkRectF rectInsidePath = resultingDecoration.boundingRect();
    QVERIFY(originalRect==rectInsidePath);
}
void TestSnapStrategy::testGridDecoration()
{
    //Tests the decoration by making sure the path returned has the calculated endpoint
    GridSnapStrategy toTest;
    const PkPointF paramMousePosTwo(40,60);
    MockShapeController fakeShapeControllerBaseTwo;
    MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo);
    fakeKoCanvasBaseTwo.setHorz(10);
    fakeKoCanvasBaseTwo.setVert(8);
    KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo);
    KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);
    qreal paramSnapDistanceTwo = 8;
    toTest.snap(paramMousePosTwo, &paramProxyTwo, paramSnapDistanceTwo);

    KoViewConverter viewConverter;
    PkSizeF unzoomedSize = viewConverter.viewToDocument(PkSizeF(5, 5));
    PkPointF snappedPos(40, 56); //the snapped position is 40, 56 because horz 10 - so 40 is right on the gridline, and 56 because 7*8 = 56 which is within 8 of 60
    PkPointF originalEndPoint(snappedPos + PkPointF(0, unzoomedSize.height()));
    PkPainterPath resultingDecoration = toTest.decoration(viewConverter);

    QVERIFY( resultingDecoration.currentPosition() == originalEndPoint );
}
void TestSnapStrategy::testBoundingBoxDecoration()
{
    //tests the decoration by making sure the returned path has the pre-calculated end point
    BoundingBoxSnapStrategy toTest;

    KoViewConverter viewConverter;
    PkSizeF unzoomedSize = viewConverter.viewToDocument(PkSizeF(5, 5));
    PkPointF snappedPos(0,0);
    PkPointF originalEndPoint(snappedPos + PkPointF(unzoomedSize.width(), -unzoomedSize.height()));
    PkPainterPath resultingDecoration = toTest.decoration(viewConverter);

    QVERIFY( resultingDecoration.currentPosition() == originalEndPoint );
}

void TestSnapStrategy::testLineGuideDecoration()
{
    // KoGuides data has been moved into Krita
    //
    // //tests the decoration by making sure there are horizontal and vertical lines in the guidesData
    // LineGuideSnapStrategy toTest;

    // const PkPointF paramMousePosTwo;
    // MockShapeController fakeShapeControllerBaseTwo;
    // MockCanvas fakeKoCanvasBaseTwo(&fakeShapeControllerBaseTwo);

    // KoGuidesData guidesData;
    // //firstSnapPointList.push_back(
    // PkList<qreal> horzLines;
    // horzLines.push_back(2);
    // horzLines.push_back(3);
    // horzLines.push_back(4);
    // horzLines.push_back(5);

    // PkList<qreal> vertLines;
    // vertLines.push_back(1);
    // vertLines.push_back(2);
    // vertLines.push_back(3);
    // vertLines.push_back(4);

    // guidesData.setHorizontalGuideLines(horzLines);
    // guidesData.setVerticalGuideLines(vertLines);
    // fakeKoCanvasBaseTwo.setGuidesData(&guidesData);
    // qreal paramSnapDistanceTwo = 8;
    // KoSnapGuide aKoSnapGuideTwo(&fakeKoCanvasBaseTwo);
    // KoSnapProxy paramProxyTwo(&aKoSnapGuideTwo);
    // toTest.snap(paramMousePosTwo, &paramProxyTwo, paramSnapDistanceTwo);

    // KoViewConverter parameterConverter;
    // PkSizeF unzoomedSize = parameterConverter.viewToDocument(PkSizeF(5, 5));
    // PkPointF snappedPos(1,2);
    // PkPointF originalEndPointOne(snappedPos + PkPointF(unzoomedSize.width(), 0));
    // PkPointF originalEndPointTwo(snappedPos + PkPointF(0, unzoomedSize.height()));
    // PkPainterPath resultingDecoration = toTest.decoration(parameterConverter);

    // QVERIFY( (resultingDecoration.currentPosition() == originalEndPointOne) || (resultingDecoration.currentPosition() == originalEndPointTwo ) );
}

void TestSnapStrategy::testSquareDistance()
{
    //tests that it does not work without setting the points
    OrthogonalSnapStrategy toTest;

    PkPointF p1;
    PkPointF p2;

    qreal resultingRealOne = toTest.squareDistance(p1, p2);
    QVERIFY(resultingRealOne == 0);
    //tests that the returned value is as expected for positive values
    OrthogonalSnapStrategy toTestTwo;

    PkPointF p1_2(2,2);
    PkPointF p2_2(1,1);

    qreal resultingRealTwo = toTestTwo.squareDistance(p1_2, p2_2);
    QVERIFY(resultingRealTwo == 2);
    //tests that the returned value is as expected for positive and negative values
    OrthogonalSnapStrategy toTestThree;

    PkPointF p1_3(2,2);
    PkPointF p2_3(-2,-2);

    qreal resultingRealThree = toTestThree.squareDistance(p1_3, p2_3);
    QVERIFY(resultingRealThree == 32);

    //tests that the returned value is 0 when the points are the same
    OrthogonalSnapStrategy toTestFour;

    PkPointF p1_4(2,2);
    PkPointF p2_4(2,2);

    qreal resultingRealFour = toTestFour.squareDistance(p1_4, p2_4);
    QVERIFY(resultingRealFour == 0);
}
void TestSnapStrategy::testScalarProduct()
{
    //Tests so the scalarProduct cannot be calculated unless the points are set
    OrthogonalSnapStrategy toTest;

    PkPointF p1_5;
    PkPointF p2_5;

    qreal resultingRealOne = toTest.squareDistance(p1_5, p2_5);
    QVERIFY(resultingRealOne == 0 );
    //tests that the product is correctly calculated for positive point values
    OrthogonalSnapStrategy toTestTwo;

    PkPointF p1_6(2,2);
    PkPointF p2_6(3,3);

    qreal resultingRealTwo = toTestTwo.squareDistance(p1_6, p2_6);
    QVERIFY(resultingRealTwo == 2 );
    //tests that the product is correctly calculated for positive and negative point values
    OrthogonalSnapStrategy toTestThree;

    PkPointF p1_7(2,2);
    PkPointF p2_7(-2,-2);

    qreal resultingRealThree = toTestThree.squareDistance(p1_7, p2_7);
    QVERIFY(resultingRealThree == 32);
    //tests so the product is 0 when the points are the same
    OrthogonalSnapStrategy toTestFour;

    PkPointF p1_8(1,1);
    PkPointF p2_8(1,1);

    qreal resultingRealFour = toTestFour.squareDistance(p1_8, p2_8);
    QVERIFY(resultingRealFour == 0);
    //tests so there is nothing fishy when using origin
    OrthogonalSnapStrategy toTestFive;

    PkPointF p1_9(1,1);
    PkPointF p2_9(0,0);

    qreal resultingRealFive = toTestFive.squareDistance(p1_9, p2_9);
    QVERIFY(resultingRealFive == 2);
}



//------------------------------------------------------------------



void TestSnapStrategy::testSnapToExtension()
{
    /*

    toTest.snapToExtension(paramPosition, &paramPoint, paramMatrix);

qDebug() << direction << " is the returned direction for this point in TestSnapStrategy::testSnapToExtension()";
    QCOMPARE(direction, );
    */
}
void TestSnapStrategy::testProject()
{
    //tests for positive point values but backwards leaning line
    ExtensionSnapStrategy toTestOne;
    qreal toCompWithOne = -1;
    PkPointF lineStart(4,4);
    PkPointF lineEnd(2,2);
    PkPointF comparisonPoint(6,6);

    qreal resultingRealOne = toTestOne.project(lineStart, lineEnd, comparisonPoint);
    QCOMPARE(resultingRealOne, toCompWithOne);
    //testing for negative point values
    ExtensionSnapStrategy toTestTwo;
    qreal toCompWithTwo = -4;
    PkPointF lineStart_2(-2,-2);
    PkPointF lineEnd_2(-4,-4);
    PkPointF comparisonPoint_2(6,6);

    qreal resultingRealTwo = toTestTwo.project(lineStart_2, lineEnd_2, comparisonPoint_2);
    QCOMPARE(resultingRealTwo, toCompWithTwo);
    //testing for negative and positive point values
    ExtensionSnapStrategy toTestThree;
    qreal toCompWithThree = (10*(6/sqrt(72.0)) + 10*(6/sqrt(72.0))) / sqrt(72.0); //diffLength = sqrt(72), scalar = (10*(6/sqrt(72)) + 10*(6/sqrt(72)))
    PkPointF lineStart_3(-2,-2);
    PkPointF lineEnd_3(4, 4);
    PkPointF comparisonPoint_3(8,8);

    qreal resultingRealThree = toTestThree.project(lineStart_3, lineEnd_3, comparisonPoint_3);
    QCOMPARE(resultingRealThree, toCompWithThree);

    //Below we test the formula itself for the dot-product by using values we know return t=0.5
    //Formula for how to use the t value is:
    //ProjectionPoint = lineStart*(1-resultingReal) + resultingReal*lineEnd; (this is the formula used in BoundingBoxSnapStrategy::squareDistanceToLine())
    //Note: The angle of the line from projection point to comparison point is always 90 degrees

    ExtensionSnapStrategy toTestFour;
    qreal toCompWithFour = 0.5;
    PkPointF lineStart_4(2,1);
    PkPointF lineEnd_4(6,3);
    PkPointF comparisonPoint_4(3,4);

    qreal resultingRealFour = toTestFour.project(lineStart_4, lineEnd_4, comparisonPoint_4);
    QCOMPARE(resultingRealFour, toCompWithFour);
}

void TestSnapStrategy::testExtensionDirection()
{
    /* TEST CASE 0
       Supposed to return null
    */
    ExtensionSnapStrategy toTestOne;
    KoPathShape uninitiatedPathShape;
    KoPathPoint::PointProperties normal = KoPathPoint::Normal;
    const PkPointF initiatedPoint0(0,0);
    KoPathPoint initiatedPoint(&uninitiatedPathShape, initiatedPoint0, normal);
    const PkTransform initiatedMatrix(1, 1, 1, 1, 1, 1);
    PkPointF direction2 = toTestOne.extensionDirection( &initiatedPoint, initiatedMatrix);
    QVERIFY(direction2.isNull());

    /* TEST CASE 1
    tests a point that:
     - is the first in a subpath,
     - does not have the firstSubpath property set,
     - it has no activeControlPoint1,
     - is has no previous point

     = expected returning an empty PkPointF
    */
    ExtensionSnapStrategy toTestTwo;
    PkPointF expectedPointTwo(0,0);
    KoPathShape shapeOne;

    PkPointF firstPoint(0,1);
    PkPointF secondPoint(1,2);
    PkPointF thirdPoint(2,3);
    PkPointF fourthPoint(3,4);

    shapeOne.moveTo(firstPoint);
    shapeOne.lineTo(secondPoint);
    shapeOne.lineTo(thirdPoint);
    shapeOne.lineTo(fourthPoint);

    PkPointF paramPositionTwo(0,1);
    KoPathPoint paramPointTwo;
    paramPointTwo.setPoint(paramPositionTwo);
    paramPointTwo.setParent(&shapeOne);

    const PkTransform paramTransMatrix(1,2,3,4,5,6);
    PkPointF directionTwo = toTestTwo.extensionDirection( &paramPointTwo, paramTransMatrix);
    QCOMPARE(directionTwo, expectedPointTwo);

    /* TEST CASE 2
    tests a point that:
     - is the second in a subpath,
     - does not have the firstSubpath property set,
     - it has no activeControlPoint1,
     - is has a previous point

     = expected returning an
    */
    ExtensionSnapStrategy toTestThree;
    PkPointF expectedPointThree(0,0);
    PkPointF paramPositionThree(1,1);
    KoPathPoint paramPointThree;
    paramPointThree.setPoint(paramPositionThree);
    paramPointThree.setParent(&shapeOne);
    PkPointF directionThree = toTestThree.extensionDirection( &paramPointThree, paramTransMatrix);
    QCOMPARE(directionThree, expectedPointThree);

}

void TestSnapStrategy::testSquareDistanceToLine()
{
    BoundingBoxSnapStrategy toTestOne;

    const PkPointF lineA(4,1);
    const PkPointF lineB(6,3);
    const PkPointF point(5,8);
    PkPointF pointOnLine(0,0);

    qreal result = toTestOne.squareDistanceToLine(lineA, lineB, point, pointOnLine);
    //Should be HUGE_VAL because scalar > diffLength
    QVERIFY(result == HUGE_VAL);

    BoundingBoxSnapStrategy toTestTwo;
    PkPointF lineA2(4,4);
    PkPointF lineB2(4,4);
    PkPointF point2(5,8);
    PkPointF pointOnLine2(0,0);

    qreal result2 = toTestTwo.squareDistanceToLine(lineA2, lineB2, point2, pointOnLine2);
    //Should be HUGE_VAL because lineA2 == lineB2
    QVERIFY(result2 == HUGE_VAL);

    BoundingBoxSnapStrategy toTestThree;
    PkPointF lineA3(6,4);
    PkPointF lineB3(8,6);
    PkPointF point3(2,2);
    PkPointF pointOnLine3(0,0);

    qreal result3 = toTestThree.squareDistanceToLine(lineA3, lineB3, point3, pointOnLine3);
    //Should be HUGE_VAL because scalar < 0.0
    QVERIFY(result3 == HUGE_VAL);

    BoundingBoxSnapStrategy toTestFour;
    PkPointF lineA4(2,2);
    PkPointF lineB4(8,6);
    PkPointF point4(3,4);
    PkPointF pointOnLine4(0,0);

    PkPointF diff(6,4);
    //diff = lineB3 - point3 = 6,4
    //diffLength = sqrt(52)
    //scalar = (1*(6/sqrt(52)) + 2*(4/sqrt(52)));

    //pointOnLine = lineA + scalar / diffLength * diff;  lineA + ((1*(6/sqrt(52)) + 2*(4/sqrt(52))) / sqrt(52)) * 6,4;
    PkPointF distToPointOnLine = (lineA4 + ((1*(6/sqrt(52.0)) + 2*(4/sqrt(52.0))) / sqrt(52.0)) * diff)-point4;
    qreal toCompWithFour = distToPointOnLine.x()*distToPointOnLine.x()+distToPointOnLine.y()*distToPointOnLine.y();

    qreal result4 = toTestFour.squareDistanceToLine(lineA4, lineB4, point4, pointOnLine4);
    //Normal case with example data
    QVERIFY(qFuzzyCompare(result4, toCompWithFour));

}
KISTEST_MAIN(TestSnapStrategy)
