/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2007 Thorsten Zachmann <zachmann@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include <QtMath>
#include "TestControlPointMoveCommand.h"

#include <PkPainterPath.h>
#include "KoPathShape.h"
#include "KoPathControlPointMoveCommand.h"

#include <simpletest.h>
#include <PkFlakeBridge.h>

void TestControlPointMoveCommand::redoUndoControlPoint1()
{
    KoPathShape path;
    path.moveTo(PkPointF(0, 0));
    path.lineTo(PkPointF(0, 100));
    KoPathPoint *point1 = path.curveTo(PkPointF(0, 50), PkPointF(100, 50), PkPointF(100, 100));
    path.curveTo(PkPointF(100, 150), PkPointF(200, 150), PkPointF(200, 100));
    KoPathPoint *point2 = path.moveTo(PkPointF(0, 300));
    path.lineTo(PkPointF(100, 400));
    path.curveTo(PkPointF(50, 400), PkPointF(0, 350), PkPointF(0, 300));
    path.closeMerge();

    PkPainterPath ppathOrg = path.outline();
    KoPathControlPointMoveCommand cmd1(KoPathPointData(&path, path.pathPointIndex(point1)), toPkPointF(PkPointF(10, 10)), KoPathPoint::ControlPoint1);
    cmd1.redo();

    PkPainterPath ppathNew1(PkPointF(0, 0));
    ppathNew1.lineTo(0, 100);
    ppathNew1.cubicTo(0, 50, 110, 60, 100, 100);
    ppathNew1.cubicTo(100, 150, 200, 150, 200, 100);
    ppathNew1.moveTo(0, 300);
    ppathNew1.lineTo(100, 400);
    ppathNew1.cubicTo(50, 400, 0, 350, 0, 300);
    ppathNew1.closeSubpath();

    QVERIFY(ppathNew1 == path.outline());

    KoPathControlPointMoveCommand cmd2(KoPathPointData(&path, path.pathPointIndex(point2)), toPkPointF(PkPointF(10, -10)), KoPathPoint::ControlPoint1);
    cmd2.redo();

    PkPainterPath ppathNew2(PkPointF(0, 0));
    ppathNew2.lineTo(0, 100);
    ppathNew2.cubicTo(0, 50, 110, 60, 100, 100);
    ppathNew2.cubicTo(100, 150, 200, 150, 200, 100);
    ppathNew2.moveTo(0, 300);
    ppathNew2.lineTo(100, 400);
    ppathNew2.cubicTo(50, 400, 10, 340, 0, 300);
    ppathNew2.closeSubpath();

    cmd2.undo();

    QVERIFY(ppathNew1 == path.outline());

    cmd1.undo();

    QVERIFY(ppathOrg == path.outline());
}

void TestControlPointMoveCommand::redoUndoControlPoint1Smooth()
{
    KoPathShape path;
    path.moveTo(PkPointF(0, 0));
    path.lineTo(PkPointF(0, 100));
    KoPathPoint *point1 = path.curveTo(PkPointF(0, 50), PkPointF(100, 50), PkPointF(100, 100));
    path.curveTo(PkPointF(100, 150), PkPointF(200, 150), PkPointF(200, 100));
    path.moveTo(PkPointF(0, 300));
    path.lineTo(PkPointF(100, 400));
    path.curveTo(PkPointF(50, 400), PkPointF(0, 350), PkPointF(0, 300));
    path.closeMerge();

    point1->setProperties(point1->properties() | KoPathPoint::IsSmooth);

    PkPainterPath ppathOrg = path.outline();
    KoPathControlPointMoveCommand cmd1(KoPathPointData(&path, path.pathPointIndex(point1)), toPkPointF(PkPointF(-25, 50)), KoPathPoint::ControlPoint1);
    cmd1.redo();

    PkPainterPath ppathNew1(PkPointF(0, 0));
    ppathNew1.lineTo(0, 100);
    ppathNew1.cubicTo(0, 50, 75, 100, 100, 100);
    ppathNew1.cubicTo(150, 100, 200, 150, 200, 100);
    ppathNew1.moveTo(0, 300);
    ppathNew1.lineTo(100, 400);
    ppathNew1.cubicTo(50, 400, 0, 350, 0, 300);
    ppathNew1.closeSubpath();

    QVERIFY(ppathNew1 == path.outline());

    cmd1.undo();

    QVERIFY(ppathOrg == path.outline());
}

void TestControlPointMoveCommand::redoUndoControlPoint1Symmetric()
{
    KoPathShape path;
    path.moveTo(PkPointF(0, 0));
    path.lineTo(PkPointF(0, 100));
    KoPathPoint *point1 = path.curveTo(PkPointF(0, 50), PkPointF(100, 50), PkPointF(100, 100));
    path.curveTo(PkPointF(100, 150), PkPointF(200, 150), PkPointF(200, 100));
    path.moveTo(PkPointF(0, 300));
    path.lineTo(PkPointF(100, 400));
    path.curveTo(PkPointF(50, 400), PkPointF(0, 350), PkPointF(0, 300));
    path.closeMerge();

    point1->setProperties(point1->properties() | KoPathPoint::IsSymmetric);

    PkPainterPath ppathOrg = path.outline();
    KoPathControlPointMoveCommand cmd1(KoPathPointData(&path, path.pathPointIndex(point1)), toPkPointF(PkPointF(-25, 50)), KoPathPoint::ControlPoint1);
    cmd1.redo();

    PkPainterPath ppathNew1(PkPointF(0, 0));
    ppathNew1.lineTo(0, 100);
    ppathNew1.cubicTo(0, 50, 75, 100, 100, 100);
    ppathNew1.cubicTo(125, 100, 200, 150, 200, 100);
    ppathNew1.moveTo(0, 300);
    ppathNew1.lineTo(100, 400);
    ppathNew1.cubicTo(50, 400, 0, 350, 0, 300);
    ppathNew1.closeSubpath();

    QVERIFY(ppathNew1 == path.outline());

    cmd1.undo();

    QVERIFY(ppathOrg == path.outline());
}

void TestControlPointMoveCommand::redoUndoControlPoint2()
{
    KoPathShape path;
    path.moveTo(PkPointF(0, 0));
    path.lineTo(PkPointF(0, 100));
    KoPathPoint *point1 = path.curveTo(PkPointF(0, 50), PkPointF(100, 50), PkPointF(100, 100));
    path.curveTo(PkPointF(100, 150), PkPointF(200, 150), PkPointF(200, 100));
    path.moveTo(PkPointF(0, 300));
    KoPathPoint *point2 = path.lineTo(PkPointF(100, 400));
    path.curveTo(PkPointF(50, 400), PkPointF(0, 350), PkPointF(0, 300));
    path.closeMerge();

    PkPainterPath ppathOrg = path.outline();
    KoPathControlPointMoveCommand cmd1(KoPathPointData(&path, path.pathPointIndex(point1)), toPkPointF(PkPointF(10, 10)), KoPathPoint::ControlPoint2);
    cmd1.redo();

    PkPainterPath ppathNew1(PkPointF(0, 0));
    ppathNew1.lineTo(0, 100);
    ppathNew1.cubicTo(0, 50, 100, 50, 100, 100);
    ppathNew1.cubicTo(110, 160, 200, 150, 200, 100);
    ppathNew1.moveTo(0, 300);
    ppathNew1.lineTo(100, 400);
    ppathNew1.cubicTo(50, 400, 0, 350, 0, 300);
    ppathNew1.closeSubpath();

    QVERIFY(ppathNew1 == path.outline());

    KoPathControlPointMoveCommand cmd2(KoPathPointData(&path, path.pathPointIndex(point2)), toPkPointF(PkPointF(-10, -10)), KoPathPoint::ControlPoint2);
    cmd2.redo();

    PkPainterPath ppathNew2(PkPointF(0, 0));
    ppathNew2.lineTo(0, 100);
    ppathNew2.cubicTo(0, 50, 100, 50, 100, 100);
    ppathNew2.cubicTo(110, 160, 200, 150, 200, 100);
    ppathNew2.moveTo(0, 300);
    ppathNew2.lineTo(100, 400);
    ppathNew2.cubicTo(40, 390, 0, 350, 0, 300);
    ppathNew2.closeSubpath();

    cmd2.undo();

    QVERIFY(ppathNew1 == path.outline());

    cmd1.undo();

    QVERIFY(ppathOrg == path.outline());
}

void TestControlPointMoveCommand::redoUndoControlPoint2Smooth()
{
    KoPathShape path;
    path.moveTo(PkPointF(0, 0));
    path.lineTo(PkPointF(0, 100));
    KoPathPoint *point1 = path.curveTo(PkPointF(0, 50), PkPointF(100, 50), PkPointF(100, 100));
    path.curveTo(PkPointF(100, 150), PkPointF(200, 150), PkPointF(200, 100));
    path.moveTo(PkPointF(0, 300));
    path.lineTo(PkPointF(100, 400));
    path.curveTo(PkPointF(50, 400), PkPointF(0, 350), PkPointF(0, 300));
    path.closeMerge();

    point1->setProperties(point1->properties() | KoPathPoint::IsSmooth);

    PkPainterPath ppathOrg = path.outline();
    KoPathControlPointMoveCommand cmd1(KoPathPointData(&path, path.pathPointIndex(point1)), toPkPointF(PkPointF(25, -50)), KoPathPoint::ControlPoint2);
    cmd1.redo();

    PkPainterPath ppathNew1(PkPointF(0, 0));
    ppathNew1.lineTo(0, 100);
    ppathNew1.cubicTo(0, 50, 50, 100, 100, 100);
    ppathNew1.cubicTo(125, 100, 200, 150, 200, 100);
    ppathNew1.moveTo(0, 300);
    ppathNew1.lineTo(100, 400);
    ppathNew1.cubicTo(50, 400, 0, 350, 0, 300);
    ppathNew1.closeSubpath();

    QVERIFY(ppathNew1 == path.outline());

    cmd1.undo();

    QVERIFY(ppathOrg == path.outline());
}

void TestControlPointMoveCommand::redoUndoControlPoint2Symmetric()
{
    KoPathShape path;
    path.moveTo(PkPointF(0, 0));
    path.lineTo(PkPointF(0, 100));
    KoPathPoint *point1 = path.curveTo(PkPointF(0, 50), PkPointF(100, 50), PkPointF(100, 100));
    path.curveTo(PkPointF(100, 150), PkPointF(200, 150), PkPointF(200, 100));
    path.moveTo(PkPointF(0, 300));
    path.lineTo(PkPointF(100, 400));
    path.curveTo(PkPointF(50, 400), PkPointF(0, 350), PkPointF(0, 300));
    path.closeMerge();

    point1->setProperties(point1->properties() | KoPathPoint::IsSymmetric);

    PkPainterPath ppathOrg = path.outline();
    KoPathControlPointMoveCommand cmd1(KoPathPointData(&path, path.pathPointIndex(point1)), toPkPointF(PkPointF(25, -50)), KoPathPoint::ControlPoint2);
    cmd1.redo();

    PkPainterPath ppathNew1(PkPointF(0, 0));
    ppathNew1.lineTo(0, 100);
    ppathNew1.cubicTo(0, 50, 75, 100, 100, 100);
    ppathNew1.cubicTo(125, 100, 200, 150, 200, 100);
    ppathNew1.moveTo(0, 300);
    ppathNew1.lineTo(100, 400);
    ppathNew1.cubicTo(50, 400, 0, 350, 0, 300);
    ppathNew1.closeSubpath();

    QVERIFY(ppathNew1 == path.outline());

    cmd1.undo();

    QVERIFY(ppathOrg == path.outline());
}

SIMPLE_TEST_MAIN(TestControlPointMoveCommand)
