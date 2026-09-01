/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisRectsGridTest.h"

#include "KisRectsGrid.h"
#include "kis_debug.h"

void KisRectsGridTest::test()
{
    KisRectsGrid grid;

    PkVector<PkRect> result;

    result = grid.addRect(PkRect(5,5,10,10));

    QCOMPARE(result, PkVector<PkRect>({PkRect(0,0,64,64)}));

    QVERIFY(grid.contains(PkRect(10,10,15,15)));
    QVERIFY(grid.contains(PkRect(0,0,64,64)));
    QVERIFY(!grid.contains(PkRect(64,10,1,1)));
    QVERIFY(!grid.contains(PkRect(0,0,65,65)));
    QVERIFY(!grid.contains(PkRect(64,64,1,1)));

    result = grid.addRect(PkRect(5,5,128,10));

    QCOMPARE(result, PkVector<PkRect>({PkRect(64,0,64,64), PkRect(128,0,64,64)}));

    QVERIFY(grid.contains(PkRect(10,10,15,15)));
    QVERIFY(grid.contains(PkRect(0,0,64,64)));
    QVERIFY(grid.contains(PkRect(64,10,1,1)));
    QVERIFY(!grid.contains(PkRect(0,0,65,65)));
    QVERIFY(!grid.contains(PkRect(64,64,1,1)));

    result = grid.removeRect(PkRect(64,65,10,10));

    QVERIFY(result.isEmpty());
    QVERIFY(grid.contains(PkRect(64,10,1,1)));

    result = grid.removeRect(PkRect(64,0,64,64));

    //qDebug() << ppVar(result);

    QCOMPARE(result, PkVector<PkRect>({PkRect(64,0,64,64)}));
    QVERIFY(!grid.contains(PkRect(64,10,1,1)));
    QVERIFY(grid.contains(PkRect(128,10,1,1)));

    result = grid.removeRect(PkRect(64,-1,128,70));

    QCOMPARE(result, PkVector<PkRect>({PkRect(128,0,64,64)}));
    QVERIFY(!grid.contains(PkRect(64,10,1,1)));
    QVERIFY(!grid.contains(PkRect(128,10,1,1)));
}

QTEST_MAIN(KisRectsGridTest)
