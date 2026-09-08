/*
 *  SPDX-FileCopyrightText: 2025 Agata Cacko <cacko.azh@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisGridInterpolationToolsTest.h"

#include <QtMath>
#include <simpletest.h>

#include <kis_grid_interpolation_tools.h>


#include <QRandomGenerator>

#include <KoColor.h>
#include <KoProgressUpdater.h>
#include <KoUpdater.h>

#include <testutil.h>
#include <kis_liquify_transform_worker.h>
#include <kis_algebra_2d.h>

using IntVector = PkVector<int>;
using PointVector = PkVector<PkPointF>;
using RectList = PkList<PkRectF>;

Q_DECLARE_METATYPE(PkPoint)
Q_DECLARE_METATYPE(PkRect)
Q_DECLARE_METATYPE(PkRectF)
Q_DECLARE_METATYPE(PkSize)
Q_DECLARE_METATYPE(IntVector)
Q_DECLARE_METATYPE(PointVector)
Q_DECLARE_METATYPE(RectList)

// NOTE: copied from Liquify Transform Worker cpp file
struct AllPointsFetcherOp
{
    AllPointsFetcherOp(PkRect srcRect) : m_srcRect(srcRect) {}

    inline void processPoint(int col, int row,
                             int prevCol, int prevRow,
                             int colIndex, int rowIndex) {

        Q_UNUSED(prevCol);
        Q_UNUSED(prevRow);
        Q_UNUSED(colIndex);
        Q_UNUSED(rowIndex);

        PkPointF pt(col, row);
        m_points << pt;
    }

    inline void nextLine() {
    }

    PkVector<PkPointF> m_points;
    PkRect m_srcRect;
};


PkVector<PkVector<int>> getCalcGridDimensionsExpectedValues()
{
    // start, end, pixelPrecision, expected grid size
    return {
        {0, 8, 8, 2},
        {0, 9, 8, 3},
        {8, 16, 8, 2},
        {8, 17, 8, 3},
        {10, 24, 8, 3},
        {10, 25, 8, 4}
    };
}

void KisGridInterpolationToolsTest::testCalcGridDimension()
{

    PkVector<PkVector<int>> testCases = getCalcGridDimensionsExpectedValues();
    for (int i = 0; i < testCases.length(); i++) {
        PkVector<int> test = testCases[i];
        QCOMPARE(GridIterationTools::calcGridDimension(test[0], test[1], test[2]), test[3]);
    }

}

void KisGridInterpolationToolsTest::testCalcGridSize()
{

    PkVector<PkVector<int>> testCases = getCalcGridDimensionsExpectedValues();

    for (int i = 0; i < testCases.length(); i++) {
        for (int j = 0; j < testCases.length(); j++) {
            PkVector<int> x = testCases[i];
            PkVector<int> y = testCases[j];
            if (x[2] != y[2]) {
                continue; // different pixel precision
            }

            PkRect srcBounds = PkRect(PkPoint(x[0], y[0]), PkPoint(x[1], y[1]));
            PkSize result = GridIterationTools::calcGridSize(srcBounds, x[2]);
            QCOMPARE(result, PkSize(x[3], y[3]));
        }
    }
}


void KisGridInterpolationToolsTest::testCalculateCellIndexes_data()
{
    QTest::addColumn<int>("column");
    QTest::addColumn<int>("row");
    QTest::addColumn<PkSize>("gridSize");
    QTest::addColumn<IntVector>("expected");

    QTest::addRow("a") << 0 << 0 << PkSize(100, 100) << (PkVector<int> {0, 1, 101, 100});
    QTest::addRow("b") << 10 << 0 << PkSize(100, 100) << (PkVector<int> {10, 11, 111, 110});
    QTest::addRow("c") << 0 << 10 << PkSize(100, 100) << (PkVector<int> {1000, 1001, 1101, 1100});
    QTest::addRow("d") << 15 << 20 << PkSize(100, 100) << (PkVector<int> {2015, 2016, 2116, 2115});


    QTest::addRow("e") << 0 << 0 << PkSize(100, 200) << (PkVector<int> {0, 1, 101, 100});
    QTest::addRow("f") << 10 << 0 << PkSize(100, 200) << (PkVector<int> {10, 11, 111, 110});
    QTest::addRow("g") << 0 << 10 << PkSize(100, 200) << (PkVector<int> {1000, 1001, 1101, 1100});
    QTest::addRow("h") << 15 << 20 << PkSize(100, 200) << (PkVector<int> {2015, 2016, 2116, 2115});

    QTest::addRow("i") << 0 << 0 << PkSize(200, 100) << (PkVector<int> {0, 1, 201, 200});
    QTest::addRow("j") << 10 << 0 << PkSize(200, 100) << (PkVector<int> {10, 11, 211, 210});
    QTest::addRow("k") << 0 << 10 << PkSize(200, 100) << (PkVector<int> {2000, 2001, 2201, 2200});
    QTest::addRow("l") << 15 << 20 << PkSize(200, 100) << (PkVector<int> {4015, 4016, 4216, 4215});
}

void KisGridInterpolationToolsTest::testCalculateCellIndexes()
{
    // it takes the gridSize and row and column index
    // and then gives out cell indexes (in the long vector with all points)
    // in a clock-wise manner
    // 1 -> 2
    //      |
    //      v
    // 4 <- 3

    QFETCH(int, column);
    QFETCH(int, row);
    QFETCH(PkSize, gridSize);
    QFETCH(IntVector, expected);

    PkVector<int> result = GridIterationTools::calculateCellIndexes(column, row, gridSize);
    QCOMPARE(result, expected);

}

void KisGridInterpolationToolsTest::testPointToIndex_data()
{
    QTest::addColumn<int>("column");
    QTest::addColumn<int>("row");
    QTest::addColumn<PkSize>("gridSize");
    QTest::addColumn<int>("expected");

    QTest::addRow("a") << 0 << 0 << PkSize(100, 100) << 0;
    QTest::addRow("b") << 10 << 0 << PkSize(100, 100) << 10;
    QTest::addRow("c") << 0 << 10 << PkSize(100, 100) << 1000;
    QTest::addRow("d") << 15 << 20 << PkSize(100, 100) << 2015;

    QTest::addRow("e") << 10 << 0 << PkSize(100, 200) << 10;
    QTest::addRow("f") << 0 << 10 << PkSize(100, 200) << 1000;
    QTest::addRow("g") << 15 << 20 << PkSize(100, 200) << 2015;

    QTest::addRow("h") << 10 << 0 << PkSize(200, 100) << 10;
    QTest::addRow("i") << 0 << 10 << PkSize(200, 100) << 2000;
    QTest::addRow("j") << 15 << 20 << PkSize(200, 100) << 4015;
}

void KisGridInterpolationToolsTest::testPointToIndex()
{
    QFETCH(int, column);
    QFETCH(int, row);
    QFETCH(PkSize, gridSize);
    QFETCH(int, expected);

    int result = GridIterationTools::pointToIndex(PkPoint(column, row), gridSize);
    QCOMPARE(result, expected);

}

void KisGridInterpolationToolsTest::testPointPolygonIndexToColRow_data()
{
    QTest::addColumn<PkPoint>("baseColRow");
    QTest::addColumn<int>("index");
    QTest::addColumn<PkPoint>("expected");

    QTest::addRow("a") << PkPoint(0, 0) << 0 << PkPoint(0, 0);
    QTest::addRow("a") << PkPoint(21, 34) << 0 << PkPoint(21, 34);
    QTest::addRow("a") << PkPoint(21, 34) << 1 << PkPoint(22, 34);
    QTest::addRow("a") << PkPoint(21, 34) << 2 << PkPoint(22, 35);
    QTest::addRow("a") << PkPoint(21, 34) << 3 << PkPoint(21, 35);


}

void KisGridInterpolationToolsTest::testPointPolygonIndexToColRow()
{
    QFETCH(PkPoint, baseColRow);
    QFETCH(int, index);
    QFETCH(PkPoint, expected);

    PkPoint result = GridIterationTools::Private::pointPolygonIndexToColRow(baseColRow, index);
    QCOMPARE(result, expected);
}


void KisGridInterpolationToolsTest::testGetOrthogonalPointApproximation()
{

}



void KisGridInterpolationToolsTest::testCalculateCorrectSubGrid_data()
{
    QTest::addColumn<PkRect>("originalBoundsForGrid");
    QTest::addColumn<int>("pixelPrecision");
    QTest::addColumn<PkRectF>("currentBounds"); // accumulated brush strokes
    QTest::addColumn<PkSize>("gridSize");
    QTest::addColumn<PkRect>("expected");

    int pixelPrecision = 8;
    PkRect originalBounds = PkRect(20, 20, 100, 200);
    QTest::addRow("a") << originalBounds << 8 << PkRectF(-1000, -1000, 100, 200) << GridIterationTools::calcGridSize(originalBounds, pixelPrecision) << PkRect();
    QTest::addRow("real-test-case") << PkRect(PkPoint(824,30), PkSize(393,330)) << 4 << PkRectF(PkPointF(2519.65,-391.596), PkSizeF(385.124, 1269.36)) << PkSize(100, 84) << PkRect();
    PkRect easyBounds = PkRect(8, 8, 64+8, 64+8);

    QTest::addRow("top side") << easyBounds << 8 << PkRectF(-100, -100, 200, 20) << PkSize(8, 8) << PkRect();
    QTest::addRow("left side") << easyBounds << 8 << PkRectF(-100, -100, 20, 200) << PkSize(8, 8) << PkRect();
    QTest::addRow("right side") << easyBounds << 8 << PkRectF(80, -100, 20, 100) << PkSize(8, 8) << PkRect();
    QTest::addRow("bottom side") << easyBounds << 8 << PkRectF(-100, 80, 100, 20) << PkSize(8, 8) << PkRect();

    QTest::addRow("tiny change") << PkRect(0, 0, 8, 8) << 8 << PkRectF(2.1, 2.1, 0.6, 0.6) << PkSize(1, 1) << PkRect(0, 0, 1, 1);

    QTest::addRow("from liquify mask unit test") << PkRect(PkPoint(1024,775), PkSize(1483,254)) << pixelPrecision << PkRectF(PkPointF(1006.38,336.751), PkSizeF(1688.41,691.249)) << PkSize(187, 34) << PkRect(0, 0, 187, 34);
    QTest::addRow("from liquify mask unit test - simplified") << PkRect(PkPoint(1024,7), PkSize(1483,14)) << pixelPrecision << PkRectF(PkPointF(1006.38,6), PkSizeF(1688.41,15)) << PkSize(187, 4) << PkRect(0, 0, 187, 4);

}

void KisGridInterpolationToolsTest::testCalculateCorrectSubGrid()
{
    QFETCH(PkRect, originalBoundsForGrid);
    QFETCH(int, pixelPrecision);
    QFETCH(PkRectF, currentBounds);
    QFETCH(PkSize, gridSize);
    QFETCH(PkRect, expected);

    PkRect result = GridIterationTools::calculateCorrectSubGrid(originalBoundsForGrid, pixelPrecision, currentBounds, gridSize);
    QCOMPARE(result, expected);

}

PkVector<PkPointF> getPoints(PkRect srcBounds, int pixelPrecision) {
    AllPointsFetcherOp pointsOp(srcBounds);
    GridIterationTools::processGrid(pointsOp, srcBounds, pixelPrecision);
    return pointsOp.m_points;
}

void KisGridInterpolationToolsTest::testCutOutSubgridFromBounds_data()
{
    QTest::addColumn<PkRect>("correctSubgrid");
    QTest::addColumn<PkRect>("srcBounds");
    QTest::addColumn<PkSize>("gridSize");
    QTest::addColumn<PointVector>("originalPoints");
    QTest::addColumn<RectList>("expected");

    int pixelPrecision = 8;

    PkRect srcBounds = PkRect(0, 0, 1240, 1754);
    PkRect srcBoundsSmall = PkRect(0, 0, 100, 100);

    QTest::addRow("out-of-bounds") << PkRect(122, 18, 34, 31) << srcBounds << GridIterationTools::calcGridSize(srcBounds, pixelPrecision)
                                   << getPoints(srcBounds, pixelPrecision) << PkList<PkRectF> {PkRectF(0,0, 1240,144), PkRectF(0,144, 976,241), PkRectF(0,385, 1240,1369)};
    QTest::addRow("out-of-bounds") << PkRect(10, 5, 4, 7) << srcBoundsSmall << GridIterationTools::calcGridSize(srcBoundsSmall, pixelPrecision)
                                   << getPoints(srcBoundsSmall, pixelPrecision) << PkList<PkRectF> {PkRectF(0,0, 100,40), PkRectF(0,40, 80,49), PkRectF(0,89, 100,11)};

    // this happened after an issue in calculateCorrectSubgrid, but it's still weird how it calculates it
    //  GridIterationTools::calcGridSize(PkRect(PkPoint(824,30), PkSize(393,330)), 4)
    PkRect srcBounds2 = PkRect(PkPoint(824,30), PkSize(393,330));

    QTest::addRow("out-of-bounds-0-width-case") << PkRect(PkPoint(100,0),PkSize(0,84)) << srcBounds2 << PkSize(100, 84)
                                   << getPoints(srcBounds2, 4) << PkList<PkRectF> {PkRectF(srcBounds2)};

}

void KisGridInterpolationToolsTest::testCutOutSubgridFromBounds()
{
    QFETCH(PkRect, correctSubgrid);
    QFETCH(PkRect, srcBounds);
    QFETCH(PkSize, gridSize);
    QFETCH(PointVector, originalPoints);
    QFETCH(RectList, expected);

    PkList<PkRectF> result = GridIterationTools::cutOutSubgridFromBounds(correctSubgrid, srcBounds, gridSize, originalPoints);
    QCOMPARE(result, expected);

}

void KisGridInterpolationToolsTest::testCanProcessPolygonsInRandomOrder()
{
    PkRect srcBounds = PkRect(0, 0, 16, 24);
    int pixelPrecision = 8;
    PkSize gridSize = GridIterationTools::calcGridSize(srcBounds, pixelPrecision);
    AllPointsFetcherOp pointsOp(srcBounds);
    GridIterationTools::processGrid(pointsOp, srcBounds, pixelPrecision);
    PkVector<PkPointF> transformedPoints = pointsOp.m_points;
    PkRectF acc = PkRectF(10, 10, 20, 20);
    PkRect correctSubGrid = GridIterationTools::calculateCorrectSubGrid(srcBounds, pixelPrecision, acc, gridSize);


    GridIterationTools::RegularGridIndexesOp indexesOp(gridSize);

    bool canMergeRects = GridIterationTools::canProcessRectsInRandomOrder(indexesOp, transformedPoints, correctSubGrid);
    QVERIFY(canMergeRects);

}

void KisGridInterpolationToolsTest::testQImagePolygonOpStructFastAreaCopy()
{
    PkImage srcImage = TestUtil::pkImageFromQImage(
        QImage(TestUtil::fetchDataFileLazy("test_grid_iteration_tools_qimage_fast_area_copy.png")));
    PkImage dstImage(srcImage.size(), srcImage.format());

    GridIterationTools::PkImagePolygonOp op(srcImage, dstImage, PkPointF(), PkPointF(-100, 100));
    op.fastCopyArea(PkRect(PkPoint(), srcImage.size()));

    //dstImage.save("fast_area_copy_result.png");
}


SIMPLE_TEST_MAIN(KisGridInterpolationToolsTest)
