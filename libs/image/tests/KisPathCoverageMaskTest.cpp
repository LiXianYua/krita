/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <simpletest.h>

#include <cstdint>
#include <type_traits>
#include <vector>

#include "../private/kis_path_rasterizer_p.h"

namespace {
using FillRasterizer = KisPathRasterizer::CoverageMask (*)(const PkPainterPath &,
                                                            const PkRect &, bool);
using StrokeRasterizer = KisPathRasterizer::CoverageMask (*)(const PkPainterPath &,
                                                              const PkPen &, const PkRect &,
                                                              bool);

static_assert(std::is_same_v<decltype(&KisPathRasterizer::rasterizeFill), FillRasterizer>);
static_assert(std::is_same_v<decltype(&KisPathRasterizer::rasterizeStroke), StrokeRasterizer>);
}

class KisPathCoverageMaskTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void absoluteCoordinateAccess();
    void emptyMask();
};

void KisPathCoverageMaskTest::absoluteCoordinateAccess()
{
    KisPathRasterizer::CoverageMask mask;
    mask.bounds = PkRect(-2, 3, 4, 2);
    mask.stride = 4;
    mask.alpha = {0, 1, 2, 3, 4, 5, 6, 255};

    QCOMPARE(mask.isEmpty(), false);
    QCOMPARE(mask.coverageAt(-2, 3), uint8_t(0));
    QCOMPARE(mask.coverageAt(1, 3), uint8_t(3));
    QCOMPARE(mask.coverageAt(-2, 4), uint8_t(4));
    QCOMPARE(mask.coverageAt(1, 4), uint8_t(255));
    QCOMPARE(mask.coverageAt(-3, 3), uint8_t(0));
    QCOMPARE(mask.coverageAt(2, 3), uint8_t(0));
    QCOMPARE(mask.coverageAt(-2, 2), uint8_t(0));
    QCOMPARE(mask.coverageAt(-2, 5), uint8_t(0));

    QVERIFY(mask.scanLine(2) == nullptr);
    QVERIFY(mask.scanLine(5) == nullptr);
    QCOMPARE(mask.scanLine(3), mask.alpha.data());
    QCOMPARE(mask.scanLine(4), mask.alpha.data() + 4);
}

void KisPathCoverageMaskTest::emptyMask()
{
    const KisPathRasterizer::CoverageMask mask;

    QVERIFY(mask.isEmpty());
    QVERIFY(mask.scanLine(0) == nullptr);
    QCOMPARE(mask.coverageAt(0, 0), uint8_t(0));
}

SIMPLE_TEST_MAIN(KisPathCoverageMaskTest)

#include "KisPathCoverageMaskTest.moc"
