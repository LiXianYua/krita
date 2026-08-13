/*
 *  SPDX-FileCopyrightText: 2022 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "TestAssistants.h"

#include <ConcentricEllipseAssistantGeometry.h>
#include <PerspectiveBasedAssistantHelper.h>
#include <kis_algebra_2d.h>
#include <kis_debug.h>
#include <kis_global.h>

void TestAssistants::testConcentricEllipseAdjustLine()
{
    const QList<QPointF> handles {
        QPointF(0, 100),
        QPointF(100, 0),
        QPointF(200, 200)
    };

    QPointF begin = QPointF(0, 100);
    //QPointF end = QPointF(100, 5);
    //QPointF end = QPointF(100, 0);
    //ellipse->adjustLine(end, begin);
    //ENTER_FUNCTION() << begin << end;

    ENTER_FUNCTION() << "Begin is " << begin;

    QList<QPointF> ends;
    ends << QPointF(100, 0) << QPointF(100, 5) << QPointF(200, 200) << QPointF(400, 400);

    for (int i = 0; i < ends.size(); i++) {
        QPointF endHere = ends[i];
        ConcentricEllipseAssistantGeometry::adjustLine(handles, endHere, begin);
        ENTER_FUNCTION() << ends[i] << "=>" << endHere;
    }


    begin = QPointF(0, 200);

    ENTER_FUNCTION() << "Begin is " << begin;

    ends.clear();
    ends << QPointF(200, 0) << QPointF(200, 5) << QPointF(400, 400) << QPointF(500, 500);

    for (int i = 0; i < ends.size(); i++) {
        QPointF endHere = ends[i];
        ConcentricEllipseAssistantGeometry::adjustLine(handles, endHere, begin);
        ENTER_FUNCTION() << ends[i] << "=>" << endHere;
    }



}


SIMPLE_TEST_MAIN(TestAssistants)
