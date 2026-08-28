/*
 * SPDX-FileCopyrightText: 2022 Agata Cacko <agata.cacko@krita.org>
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "TestAssistants.h"

#include <ConcentricEllipseAssistantGeometry.h>

#include <cmath>

namespace
{
bool closePoint(const PkPointF &actual, const PkPointF &expected, double epsilon = 1e-4)
{
    return std::abs(actual.x() - expected.x()) <= epsilon &&
           std::abs(actual.y() - expected.y()) <= epsilon;
}
}

int runConcentricEllipseAdjustLineTest()
{
    const PkList<PkPointF> handles {
        PkPointF(-10, 0),
        PkPointF(10, 0),
        PkPointF(0, 5)
    };

    const PkPointF begin(0, 10);
    PkList<PkPointF> inputs {
        PkPointF(0, 15), PkPointF(30, 0)
    };
    const PkList<PkPointF> expected {
        PkPointF(0, 10), PkPointF(20, 0)
    };
    for (int i = 0; i < inputs.size(); ++i) {
        PkPointF end = inputs[i];
        ConcentricEllipseAssistantGeometry::adjustLine(handles, end, begin);
        if (!closePoint(end, expected[i])) return 1;
    }

    return 0;
}

int main()
{
    return runConcentricEllipseAdjustLineTest();
}
