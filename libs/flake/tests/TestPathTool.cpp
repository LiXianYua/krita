/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2007 Thorsten Zachmann <zachmann@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/
#include <QtMath>
#include <vector>
#include "TestPathTool.h"

#include <PkPainterPath.h>
#include "../KoPathShape.h"
#include "../KoCanvasCursorHost.h"
#include "../tools/KoPathTool.h"
#include "../tools/KoPathToolSelection.h"
#include "../KoPathPointData.h"
#include <MockShapes.h>
#include <simpletest.h>

namespace {

class CursorResourceMockCanvas final : public MockCanvas, public KoCanvasCursorHost
{
public:
    struct Request {
        PkString resource;
        PkSize size;
        PkPoint hotspot;
    };

    QCursor loadCursorResource(const PkString &resource,
                               const PkSize &size,
                               const PkPoint &hotspot) const override
    {
        requests.push_back({resource, size, hotspot});
        return QCursor(requests.size() == 1 ? Qt::CrossCursor : Qt::SizeAllCursor);
    }

    mutable std::vector<Request> requests;
};

}

void TestPathTool::cursorResourcesAreResolvedByHost()
{
    CursorResourceMockCanvas canvas;
    KoPathTool tool(&canvas);

    QCOMPARE(canvas.requests.size(), size_t(2));
    QCOMPARE(canvas.requests[0].resource, PkString(":/cursor-needle.svg"));
    QCOMPARE(canvas.requests[0].size, PkSize(32, 32));
    QCOMPARE(canvas.requests[0].hotspot, PkPoint(0, 0));
    QCOMPARE(canvas.requests[1].resource, PkString(":/cursor-needle-move.svg"));
    QCOMPARE(canvas.requests[1].size, PkSize(32, 32));
    QCOMPARE(canvas.requests[1].hotspot, PkPoint(0, 0));
    QCOMPARE(tool.m_selectCursor.shape(), Qt::CrossCursor);
    QCOMPARE(tool.m_moveCursor.shape(), Qt::SizeAllCursor);
}

void TestPathTool::koPathPointSelection_selectedSegmentsData()
{
    KoPathShape path1;
    KoPathPoint *point11 = path1.moveTo(PkPointF(10, 10));
    KoPathPoint *point12 = path1.lineTo(PkPointF(20, 10));
    KoPathPoint *point13 = path1.lineTo(PkPointF(20, 20));
    KoPathPoint *point14 = path1.lineTo(PkPointF(15, 25));
    path1.lineTo(PkPointF(10, 20));
    KoPathPoint *point16 = path1.moveTo(PkPointF(30, 30));
    path1.lineTo(PkPointF(40, 30));
    KoPathPoint *point18 = path1.lineTo(PkPointF(40, 40));
    KoPathPoint *point19 = path1.curveTo(PkPointF(40, 45), PkPointF(30, 45), PkPointF(30, 40));
    path1.close();

    KoPathShape path2;
    KoPathPoint *point21 = path2.moveTo(PkPointF(100, 100));
    KoPathPoint *point22 = path2.lineTo(PkPointF(110, 100));
    KoPathPoint *point23 = path2.lineTo(PkPointF(110, 110));

    KoPathShape path3;
    KoPathPoint *point31 = path3.moveTo(PkPointF(200, 220));
    KoPathPoint *point32 = path3.lineTo(PkPointF(210, 220));
    KoPathPoint *point33 = path3.lineTo(PkPointF(220, 220));
    path3.close();

    MockCanvas canvas;
    KoPathTool tool(&canvas);
    QVERIFY(1 == 1);
    KoPathToolSelection pps(&tool);
    pps.add(point11, false);
    pps.add(point12, false);
    pps.add(point13, false);
    pps.add(point14, false);
    pps.add(point16, false);
    pps.add(point18, false);
    pps.add(point19, false);
    pps.add(point21, false);
    pps.add(point22, false);
    pps.add(point23, false);
    pps.add(point31, false);
    pps.add(point32, false);
    pps.add(point33, false);

    PkList<KoPathPointData> pd2;
    pd2.append(KoPathPointData(&path1, path1.pathPointIndex(point11)));
    pd2.append(KoPathPointData(&path1, path1.pathPointIndex(point12)));
    pd2.append(KoPathPointData(&path1, path1.pathPointIndex(point13)));
    pd2.append(KoPathPointData(&path1, path1.pathPointIndex(point18)));
    pd2.append(KoPathPointData(&path1, path1.pathPointIndex(point19)));
    pd2.append(KoPathPointData(&path2, path2.pathPointIndex(point21)));
    pd2.append(KoPathPointData(&path2, path2.pathPointIndex(point22)));
    pd2.append(KoPathPointData(&path3, path3.pathPointIndex(point31)));
    pd2.append(KoPathPointData(&path3, path3.pathPointIndex(point32)));
    pd2.append(KoPathPointData(&path3, path3.pathPointIndex(point33)));

    std::sort(pd2.begin(), pd2.end());

    PkList<KoPathPointData> pd1(pps.selectedSegmentsData());
    QVERIFY(pd1 == pd2);
}


SIMPLE_TEST_MAIN(TestPathTool)
