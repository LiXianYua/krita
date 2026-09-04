/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2007 Thorsten Zachmann <zachmann@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include <QtMath>
#include "TestPointTypeCommand.h"

#include <PkPainterPath.h>
#include "KoPathShape.h"
#include "KoPathPointTypeCommand.h"

#include <simpletest.h>
#include <PkFlakeBridge.h>

void TestPointTypeCommand::redoUndoSymmetric()
{
    KoPathShape path1;
    path1.moveTo(PkPointF(0, 0));
    path1.lineTo(PkPointF(0, 100));
    KoPathPoint *point1 = path1.curveTo(PkPointF(0, 50), PkPointF(100, 50), PkPointF(100, 100));
    KoPathPoint *point2 = path1.curveTo(PkPointF(100, 200), PkPointF(200, 200), PkPointF(200, 100));
    path1.curveTo(PkPointF(200, 50), PkPointF(300, 50), PkPointF(300, 100));
    // test with normalize
    KoPathShape path2;
    KoPathPoint *point3 = path2.moveTo(PkPointF(0, 0));
    path2.curveTo(PkPointF(50, 0), PkPointF(100, 50), PkPointF(100, 100));
    path2.curveTo(PkPointF(50, 100), PkPointF(0, 50), PkPointF(0, 0));
    path2.closeMerge();

    PkList<KoPathPointData> pd;
    pd.append(KoPathPointData(&path1, path1.pathPointIndex(point1)));
    pd.append(KoPathPointData(&path1, path1.pathPointIndex(point2)));
    pd.append(KoPathPointData(&path2, path2.pathPointIndex(point3)));

    PkPainterPath ppath1Org = path1.outline();
    PkPainterPath ppath2Org = path2.outline();

    KoPathPointTypeCommand cmd(toPkList(pd), KoPathPointTypeCommand::Symmetric);
    cmd.redo();

    PkPainterPath ppath(PkPointF(0, 0));
    ppath.lineTo(0, 100);
    ppath.cubicTo(0, 50, 100, 25, 100, 100);
    ppath.cubicTo(100, 175, 200, 175, 200, 100);
    ppath.cubicTo(200, 25, 300, 50, 300, 100);

    QVERIFY((point1->properties() & KoPathPoint::IsSymmetric) == KoPathPoint::IsSymmetric);
    QVERIFY((point1->properties() & KoPathPoint::IsSmooth) == KoPathPoint::Normal);

    QVERIFY(ppath == path1.outline());

    cmd.undo();

    QVERIFY(ppath1Org == path1.outline());
    QVERIFY(ppath2Org == path2.outline());
}

void TestPointTypeCommand::redoUndoSmooth()
{
    KoPathShape path1;
    path1.moveTo(PkPointF(0, 0));
    path1.lineTo(PkPointF(0, 100));
    KoPathPoint *point1 = path1.curveTo(PkPointF(0, 50), PkPointF(100, 50), PkPointF(100, 100));
    KoPathPoint *point2 = path1.curveTo(PkPointF(100, 200), PkPointF(200, 200), PkPointF(200, 100));
    path1.curveTo(PkPointF(200, 50), PkPointF(300, 50), PkPointF(300, 100));
    // test with normalize
    KoPathShape path2;
    KoPathPoint *point3 = path2.moveTo(PkPointF(0, 0));
    path2.curveTo(PkPointF(50, 0), PkPointF(100, 50), PkPointF(100, 100));
    path2.curveTo(PkPointF(50, 100), PkPointF(0, 50), PkPointF(0, 0));
    path2.closeMerge();

    PkList<KoPathPointData> pd;
    pd.append(KoPathPointData(&path1, path1.pathPointIndex(point1)));
    pd.append(KoPathPointData(&path1, path1.pathPointIndex(point2)));
    pd.append(KoPathPointData(&path2, path2.pathPointIndex(point3)));

    PkPainterPath ppath1Org = path1.outline();
    PkPainterPath ppath2Org = path2.outline();

    KoPathPointTypeCommand cmd(toPkList(pd), KoPathPointTypeCommand::Smooth);
    cmd.redo();

    PkPainterPath ppath(PkPointF(0, 0));
    ppath.lineTo(0, 100);
    ppath.cubicTo(0, 50, 100, 50, 100, 100);
    ppath.cubicTo(100, 200, 200, 200, 200, 100);
    ppath.cubicTo(200, 50, 300, 50, 300, 100);

    QVERIFY((point1->properties() & KoPathPoint::IsSmooth) == KoPathPoint::IsSmooth);
    QVERIFY((point1->properties() & KoPathPoint::IsSymmetric) == KoPathPoint::Normal);

    QVERIFY(ppath == path1.outline());

    cmd.undo();

    QVERIFY(ppath1Org == path1.outline());
    QVERIFY(ppath2Org == path2.outline());
}

void TestPointTypeCommand::redoUndoCorner()
{
    KoPathShape path1;
    path1.moveTo(PkPointF(0, 0));
    path1.lineTo(PkPointF(0, 100));
    KoPathPoint *point1 = path1.curveTo(PkPointF(0, 50), PkPointF(100, 50), PkPointF(100, 100));
    KoPathPoint *point2 = path1.curveTo(PkPointF(100, 150), PkPointF(200, 150), PkPointF(200, 100));
    path1.curveTo(PkPointF(200, 50), PkPointF(300, 50), PkPointF(300, 100));
    // test with normalize
    KoPathShape path2;
    KoPathPoint *point3 = path2.moveTo(PkPointF(0, 0));
    path2.curveTo(PkPointF(50, 0), PkPointF(100, 50), PkPointF(100, 100));
    path2.curveTo(PkPointF(50, 100), PkPointF(0, 50), PkPointF(0, 0));
    path2.closeMerge();

    PkList<KoPathPointData> pd;
    pd.append(KoPathPointData(&path1, path1.pathPointIndex(point1)));
    pd.append(KoPathPointData(&path1, path1.pathPointIndex(point2)));
    pd.append(KoPathPointData(&path2, path2.pathPointIndex(point3)));

    PkPainterPath ppath1Org = path1.outline();
    PkPainterPath ppath2Org = path2.outline();

    KoPathPointTypeCommand cmd1(toPkList(pd), KoPathPointTypeCommand::Symmetric);
    cmd1.redo();

    KoPathPointTypeCommand cmd2(toPkList(pd), KoPathPointTypeCommand::Corner);
    cmd2.redo();

    PkPainterPath ppath(PkPointF(0, 0));
    ppath.lineTo(0, 100);
    ppath.cubicTo(0, 50, 100, 50, 100, 100);
    ppath.cubicTo(100, 150, 200, 150, 200, 100);
    ppath.cubicTo(200, 50, 300, 50, 300, 100);

    QVERIFY((point1->properties() & KoPathPoint::IsSmooth) == KoPathPoint::Normal);
    QVERIFY((point1->properties() & KoPathPoint::IsSymmetric) == KoPathPoint::Normal);

    QVERIFY(ppath == path1.outline());

    cmd2.undo();
    cmd1.undo();

    QVERIFY(ppath1Org == path1.outline());
    QVERIFY(ppath2Org == path2.outline());
}

SIMPLE_TEST_MAIN(TestPointTypeCommand)
