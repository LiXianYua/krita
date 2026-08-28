/*
 * SPDX-FileCopyrightText: 2022 Agata Cacko <agata.cacko@krita.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "TestAssistants.h"

#include <ConcentricEllipseAssistantGeometry.h>

#include <cmath>

int runConcentricEllipseAdjustLineTest()
{
    const PkList<PkPointF> handles {
        PkPointF(0, 100),
        PkPointF(100, 0),
        PkPointF(200, 200)
    };

    PkPointF begin(0, 100);
    PkList<PkPointF> ends {
        PkPointF(100, 0), PkPointF(100, 5), PkPointF(200, 200), PkPointF(400, 400)
    };

    for (PkPointF end : ends) {
        ConcentricEllipseAssistantGeometry::adjustLine(handles, end, begin);
        if (!std::isfinite(end.x()) || !std::isfinite(end.y())) return 1;
    }

    begin = PkPointF(0, 200);
    ends = {
        PkPointF(200, 0), PkPointF(200, 5), PkPointF(400, 400), PkPointF(500, 500)
    };
    for (PkPointF end : ends) {
        ConcentricEllipseAssistantGeometry::adjustLine(handles, end, begin);
        if (!std::isfinite(end.x()) || !std::isfinite(end.y())) return 2;
    }
    return 0;
}

int main()
{
    return runConcentricEllipseAdjustLineTest();
}
