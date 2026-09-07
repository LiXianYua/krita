/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2025 Agata Cacko <cacko.azh@gmail.com>
 *
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_GRID_INTERPOLATION_TOOLS_H
#define __KIS_GRID_INTERPOLATION_TOOLS_H

#include <PkGlobal.h>
#include <limits>
#include <algorithm>

#include <PkImage.h>
#include <PkColor.h>
#include <PkList.h>
#include <PkPainterPath.h>
#include <PkPolygon.h>
#include <PkRect.h>
#include <PkSize.h>
#include <PkTransform.h>
#include <PkVector.h>

#include "kis_algebra_2d.h"
#include "kis_four_point_interpolator_forward.h"
#include "kis_four_point_interpolator_backward.h"
#include "kis_iterator_ng.h"
#include "kis_random_sub_accessor.h"
#include "kis_painter.h"
#include "KisRegion.h"

//#define DEBUG_PAINTING_POLYGONS

namespace GridIterationTools {

inline int calcGridDimension(int start, int end, const int pixelPrecision)
{
    const int alignmentMask = ~(pixelPrecision - 1);

    int alignedStart = (start + pixelPrecision - 1) & alignmentMask;
    int alignedEnd = end & alignmentMask;

    int size = 0;

    if (alignedEnd > alignedStart) {
        size = (alignedEnd - alignedStart) / pixelPrecision + 1;
        size += alignedStart != start;
        size += alignedEnd != end;
    } else {
        size = 2 + (end - start >= pixelPrecision);
    }

    return size;
}

inline PkSize calcGridSize(const PkRect &srcBounds, const int pixelPrecision) {
    return PkSize(calcGridDimension(srcBounds.x(), srcBounds.right(), pixelPrecision),
                 calcGridDimension(srcBounds.y(), srcBounds.bottom(), pixelPrecision));
}

template <class ProcessPolygon, class ForwardTransform>
struct CellOp
{
    CellOp(ProcessPolygon &_polygonOp, ForwardTransform &_transformOp)
        : polygonOp(_polygonOp),
          transformOp(_transformOp)
    {
    }

    inline void processPoint(int col, int row,
                             int prevCol, int prevRow,
                             int colIndex, int rowIndex) {

        PkPointF dstPosF = transformOp(PkPointF(col, row));
        currLinePoints << dstPosF;

        if (rowIndex >= 1 && colIndex >= 1) {
            PkPolygonF srcPolygon;

            srcPolygon << PkPointF(prevCol, prevRow);
            srcPolygon << PkPointF(col, prevRow);
            srcPolygon << PkPointF(col, row);
            srcPolygon << PkPointF(prevCol, row);

            PkPolygonF dstPolygon;

            dstPolygon << prevLinePoints.at(colIndex - 1);
            dstPolygon << prevLinePoints.at(colIndex);
            dstPolygon << currLinePoints.at(colIndex);
            dstPolygon << currLinePoints.at(colIndex - 1);

            polygonOp(srcPolygon, dstPolygon);
        }

    }

    inline void nextLine() {
        std::swap(prevLinePoints, currLinePoints);

        // we are erasing elements for not freeing the occupied
        // memory, which is more efficient since we are going to fill
        // the vector again
        currLinePoints.erase(currLinePoints.begin(), currLinePoints.end());
    }

    PkVector<PkPointF> prevLinePoints;
    PkVector<PkPointF> currLinePoints;
    ProcessPolygon &polygonOp;
    ForwardTransform &transformOp;
};

template <class ProcessCell>
void processGrid(ProcessCell &cellOp,
                 const PkRect &srcBounds,
                 const int pixelPrecision)
{
    if (srcBounds.isEmpty()) return;

    const int alignmentMask = ~(pixelPrecision - 1);

    int prevRow = std::numeric_limits<int>::max();
    int prevCol = std::numeric_limits<int>::max();

    int rowIndex = 0;
    int colIndex = 0;

    for (int row = srcBounds.top(); row <= srcBounds.bottom();) {
        for (int col = srcBounds.left(); col <= srcBounds.right();) {

            cellOp.processPoint(col, row,
                                prevCol, prevRow,
                                colIndex, rowIndex);

            prevCol = col;
            col += pixelPrecision;
            colIndex++;

            if (col > srcBounds.right() &&
                col <= srcBounds.right() + pixelPrecision - 1) {

                col = srcBounds.right();
            } else {
                col &= alignmentMask;
            }
        }

        cellOp.nextLine();
        colIndex = 0;

        prevRow = row;
        row += pixelPrecision;
        rowIndex++;

        if (row > srcBounds.bottom() &&
            row <= srcBounds.bottom() + pixelPrecision - 1) {

            row = srcBounds.bottom();
        } else {
            row &= alignmentMask;
        }
    }
}

template <class ProcessPolygon, class ForwardTransform>
void processGrid(ProcessPolygon &polygonOp, ForwardTransform &transformOp,
                 const PkRect &srcBounds, const int pixelPrecision)
{
    CellOp<ProcessPolygon, ForwardTransform> cellOp(polygonOp, transformOp);
    processGrid(cellOp, srcBounds, pixelPrecision);
}

struct PaintDevicePolygonOp
{
    PaintDevicePolygonOp(KisPaintDeviceSP srcDev, KisPaintDeviceSP dstDev)
        : m_srcDev(srcDev), m_dstDev(dstDev) {}

    ~PaintDevicePolygonOp()
    {
        /**
         * When setCanMergeRects() is set to true, the caller should
         * call finalize() to process all the postponed rects, which
         * would clear the vector
         */
        KIS_SAFE_ASSERT_RECOVER_NOOP(m_rectsToCopy.isEmpty());
    }

    void fastCopyArea(PkRect areaToCopy) {
        fastCopyArea(areaToCopy, m_canMergeRects);
    }

    void fastCopyArea(PkRect areaToCopy, bool lazy) {
#ifdef DEBUG_PAINTING_POLYGONS

        PkRect boundRect = areaToCopy;
        KisSequentialIterator dstIt(m_dstDev, boundRect);
        KisSequentialIterator srcIt(m_srcDev, boundRect);

        // this can possibly be optimized with scanlining the polygon
        // (use intersectLineConvexPolygon to get a line at every height)
        // but it doesn't matter much because in the vast majority of cases
        // it should go straight to the rect area copying

        while (dstIt.nextPixel()  && srcIt.nextPixel()) {
            memcpy(dstIt.rawData(), srcIt.oldRawData(), m_dstDev->pixelSize());
            PkColor color = m_debugColor;
            color.setHsl(KisAlgebra2D::wrapValue(m_debugColor.hslHue() + 20, 0, 360), m_debugColor.hslSaturation(), m_debugColor.lightness());
            m_dstDev->colorSpace()->fromQColor(color, dstIt.rawData());
        }
        return;
#endif
        if (lazy) {
            m_rectsToCopy.append(areaToCopy.adjusted(0, 0, -1, -1));
        } else {
            KisPainter::copyAreaOptimized(areaToCopy.topLeft(), m_srcDev, m_dstDev, areaToCopy);
        }
    }

    void operator() (const PkPolygonF &srcPolygon, const PkPolygonF &dstPolygon) {
        operator() (srcPolygon, dstPolygon, dstPolygon);
    }

    void operator() (const PkPolygonF &srcPolygon, const PkPolygonF &dstPolygon, const PkPolygonF &clipDstPolygon) {
#ifdef DEBUG_PAINTING_POLYGONS
        //m_rectId++;
#endif
        PkRect boundRect = clipDstPolygon.boundingRect().toAlignedRect();
        if (boundRect.isEmpty()) return;

        bool samePolygon = (m_dstDev->colorSpace() == m_srcDev->colorSpace())
                && KisAlgebra2D::fuzzyPointCompare(srcPolygon, dstPolygon, m_epsilon)
                && KisAlgebra2D::fuzzyPointCompare(srcPolygon, clipDstPolygon, m_epsilon);

        if (samePolygon && KisAlgebra2D::isPolygonPixelAlignedRect(dstPolygon, m_epsilon)) {
            PkRect boundRect = dstPolygon.boundingRect().toAlignedRect();
            fastCopyArea(boundRect);
            return;
        }


        // provess previous rects so they are all processed
        // in the same order as without any performance improvements
        // (according to the grid processing order)
        copyPreviousRects();


        KisSequentialIterator dstIt(m_dstDev, boundRect);
        KisRandomSubAccessorSP srcAcc = m_srcDev->createRandomSubAccessor();

        KisFourPointInterpolatorBackward interp(srcPolygon, dstPolygon);
#ifdef DEBUG_PAINTING_POLYGONS
        int pixelId = 0;
#endif

        /**
         * We need to make sure that the destination polygon is not too small,
         * otherwise even small rounding will send the src-accessor into
         * infinity
         */
        if (interp.isValid(0.1)) {
            int y = boundRect.top();
            interp.setY(y);

            while (dstIt.nextPixel()) {
                int newY = dstIt.y();

                if (y != newY) {
                    y = newY;
                    interp.setY(y);
                }

                PkPointF srcPoint(dstIt.x(), y);

                if (clipDstPolygon.containsPoint(srcPoint, Pk::OddEvenFill)) {

                    interp.setX(srcPoint.x());
                    PkPointF dstPoint = interp.getValue();

                    // brain-blowing part:
                    //
                    // since the interpolator does the inverted
                    // transformation we read data from "dstPoint"
                    // (which is non-transformed) and write it into
                    // "srcPoint" (which is transformed position)

                    srcAcc->moveTo(dstPoint);
                    quint8* rawData = dstIt.rawData();
                    srcAcc->sampledOldRawData(rawData);
#ifdef DEBUG_PAINTING_POLYGONS
                    PkColor color = m_debugColor;
                    color.setHsl(KisAlgebra2D::wrapValue(m_debugColor.hslHue() + m_rectId, 0, 360), m_debugColor.hslSaturation(), pkBound(0, m_debugColor.lightness() - 50 - pixelId, 100));
                    pixelId++;
                    m_dstDev->colorSpace()->fromQColor(color, rawData);
#endif
                }
            }

        } else {
            srcAcc->moveTo(interp.fallbackSourcePoint());

            while (dstIt.nextPixel()) {
                PkPointF srcPoint(dstIt.x(), dstIt.y());

                if (clipDstPolygon.containsPoint(srcPoint, Pk::OddEvenFill)) {
                    srcAcc->sampledOldRawData(dstIt.rawData());
#ifdef DEBUG_PAINTING_POLYGONS
                    PkColor color = m_debugColor;
                    color.setHsl(KisAlgebra2D::wrapValue(m_debugColor.hslHue() + m_rectId, 0, 360), m_debugColor.hslSaturation(), pkBound(0, m_debugColor.lightness() + 50, 100));
                    m_dstDev->colorSpace()->fromQColor(color, dstIt.rawData());
#endif
                }
            }
        }

    }

    void copyPreviousRects() {
        PkVector<PkRect>::iterator end = KisRegion::mergeSparseRects(m_rectsToCopy.begin(), m_rectsToCopy.end());

        for (PkVector<PkRect>::iterator it = m_rectsToCopy.begin(); it < end; it++) {
            PkRect areaToCopy = *it;
            fastCopyArea(areaToCopy.adjusted(0, 0, 1, 1), false);
        }
        m_rectsToCopy = PkVector<PkRect>();
    }

    void finalize() {
        copyPreviousRects();
    }

    /**
     * IMPORTANT: When setCanMergeRects() is set to `true`,
     * the caller should calls finalilze() in the end of
     * the processing action to actually copy all the lazily
     * postponed rects.
     */
    inline void setCanMergeRects(bool newCanMergeRects) {
        m_canMergeRects = newCanMergeRects;
    }

    KisPaintDeviceSP m_srcDev;
    KisPaintDeviceSP m_dstDev;
    const qreal m_epsilon {0.001};

#ifdef DEBUG_PAINTING_POLYGONS
    PkColor m_debugColor {Pk::red};
    int m_rectId {0};
    inline void setDebugColor(PkColor color) {
        m_debugColor = color;
    }
#endif

private:
    bool m_canMergeRects {false};
    PkVector<PkRect> m_rectsToCopy;

};

struct PkImagePolygonOp
{
    PkImagePolygonOp(const PkImage &srcImage, PkImage &dstImage,
                    const PkPointF &srcImageOffset,
                    const PkPointF &dstImageOffset)
        : m_srcImage(srcImage), m_dstImage(dstImage),
          m_srcImageOffset(srcImageOffset),
          m_dstImageOffset(dstImageOffset),
          m_srcImageRect(m_srcImage.rect()),
          m_dstImageRect(m_dstImage.rect())
    {
    }

    ~PkImagePolygonOp()
    {
        /**
         * When setCanMergeRects() is set to true, the caller should
         * call finalize() to process all the postponed rects, which
         * would clear the vector
         */
        KIS_SAFE_ASSERT_RECOVER_NOOP(m_rectsToCopy.isEmpty());
    }

    void fastCopyArea(PkRect areaToCopy) {
        fastCopyArea(areaToCopy, m_canMergeRects);
    }

    void fastCopyArea(PkRect areaToCopy, bool lazy) {
        if (lazy) {
            m_rectsToCopy.append(areaToCopy.adjusted(0, 0, -1, -1));
            return;
        }

        // only handling saved offsets
        PkRect srcArea = areaToCopy.translated(-m_srcImageOffset.toPoint());
        PkRect dstArea = areaToCopy.translated(-m_dstImageOffset.toPoint());

        srcArea = srcArea.intersected(m_srcImageRect);
        dstArea = dstArea.intersected(m_dstImageRect);

        // it might look pointless but it cuts off unneeded areas on both rects based on where they end up
        // since *I know* they are the same rectangle before translation
        // TODO: I'm pretty sure this logic is correct, but let's check it when I'm less sleepy
        PkRect srcAreaUntranslated = srcArea.translated(m_srcImageOffset.toPoint());
        PkRect dstAreaUntranslated = dstArea.translated(m_dstImageOffset.toPoint());

        PkRect actualCopyArea = srcAreaUntranslated.intersected(dstAreaUntranslated);
        srcArea = actualCopyArea.translated(-m_srcImageOffset.toPoint());
        dstArea = actualCopyArea.translated(-m_dstImageOffset.toPoint());

        int bytesPerPixel = m_srcImage.sizeInBytes()/m_srcImage.height()/m_srcImage.width();

        int srcX = srcArea.left()*bytesPerPixel;
        int dstX = dstArea.left()*bytesPerPixel;

        for (int srcY = srcArea.top(); srcY <= srcArea.bottom(); ++srcY) {

            int dstY = dstArea.top() + srcY - srcArea.top();
            const uchar *srcLine = m_srcImage.constScanLine(srcY);
            uchar *dstLine = m_dstImage.scanLine(dstY);
            memcpy(dstLine + dstX, srcLine + srcX, srcArea.width()*bytesPerPixel);

        }
    }

    void operator() (const PkPolygonF &srcPolygon, const PkPolygonF &dstPolygon) {
        this->operator() (srcPolygon, dstPolygon, dstPolygon);
    }

    void operator() (const PkPolygonF &srcPolygon, const PkPolygonF &dstPolygon, const PkPolygonF &clipDstPolygon) {
        PkRect boundRect = clipDstPolygon.boundingRect().toAlignedRect();

        bool samePolygon = (m_dstImage.format() == m_srcImage.format())
                && KisAlgebra2D::fuzzyPointCompare(srcPolygon, dstPolygon, m_epsilon)
                && KisAlgebra2D::fuzzyPointCompare(srcPolygon, clipDstPolygon, m_epsilon);

        if (samePolygon && KisAlgebra2D::isPolygonPixelAlignedRect(dstPolygon, m_epsilon)) {
            PkRect boundRect = dstPolygon.boundingRect().toAlignedRect();
            fastCopyArea(boundRect);
            return;
        }

        // provess previous rects so they are all processed
        // in the same order as without any performance improvements
        copyPreviousRects();

        KisFourPointInterpolatorBackward interp(srcPolygon, dstPolygon);

        for (int y = boundRect.top(); y <= boundRect.bottom(); y++) {
            interp.setY(y);
            for (int x = boundRect.left(); x <= boundRect.right(); x++) {

                PkPointF srcPoint(x, y);
                if (clipDstPolygon.containsPoint(srcPoint, Pk::OddEvenFill)) {

                    interp.setX(srcPoint.x());
                    PkPointF dstPoint = interp.getValue();

                    // about srcPoint/dstPoint hell please see a
                    // comment in PaintDevicePolygonOp::operator() ()

                    srcPoint -= m_dstImageOffset;
                    dstPoint -= m_srcImageOffset;

                    PkPoint srcPointI = srcPoint.toPoint();
                    PkPoint dstPointI = dstPoint.toPoint();

                    if (!m_dstImageRect.contains(srcPointI)) continue;
                    if (!m_srcImageRect.contains(dstPointI)) continue;

                    m_dstImage.setPixel(srcPointI.x(), srcPointI.y(),
                                        m_srcImage.pixel(dstPointI.x(), dstPointI.y()));
                }
            }
        }

    }

    void copyPreviousRects() {

        PkVector<PkRect>::iterator end = KisRegion::mergeSparseRects(m_rectsToCopy.begin(), m_rectsToCopy.end());

        for (PkVector<PkRect>::iterator it = m_rectsToCopy.begin(); it < end; it++) {
            PkRect areaToCopy = *it;
            fastCopyArea(areaToCopy.adjusted(0, 0, 1, 1), false);
        }

        m_rectsToCopy = PkVector<PkRect>();
    }

    void finalize() {
        copyPreviousRects();
    }

    /**
     * IMPORTANT: When setCanMergeRects() is set to `true`,
     * the caller should calls finalilze() in the end of
     * the processing action to actually copy all the lazily
     * postponed rects.
     */
    inline void setCanMergeRects(bool canMergeRects) {
        m_canMergeRects = canMergeRects;
    }

    const PkImage &m_srcImage;
    PkImage &m_dstImage;
    PkPointF m_srcImageOffset;
    PkPointF m_dstImageOffset;

    PkRect m_srcImageRect;
    PkRect m_dstImageRect;

    const qreal m_epsilon {0.001};

private:
    bool m_canMergeRects {false};
    PkVector<PkRect> m_rectsToCopy;
};

/*************************************************************/
/*      Iteration through precalculated grid                 */
/*************************************************************/

/**
 *    A-----B         The polygons will be in the following order:
 *    |     |
 *    |     |         polygon << A << B << D << C;
 *    C-----D
 */
inline PkVector<int> calculateCellIndexes(int col, int row, const PkSize &gridSize)
{
    const int tl = col + row * gridSize.width();
    const int tr = tl + 1;
    const int bl = tl + gridSize.width();
    const int br = bl + 1;

    PkVector<int> cellIndexes;
    cellIndexes << tl;
    cellIndexes << tr;
    cellIndexes << br;
    cellIndexes << bl;

    return cellIndexes;
}

inline int pointToIndex(const PkPoint &cellPt, const PkSize &gridSize)
{
    return cellPt.x() +
        cellPt.y() * gridSize.width();
}

namespace Private {
    inline PkPoint pointPolygonIndexToColRow(PkPoint baseColRow, int index)
    {
        static PkVector<PkPoint> pointOffsets;
        if (pointOffsets.isEmpty()) {
            pointOffsets << PkPoint(0,0);
            pointOffsets << PkPoint(1,0);
            pointOffsets << PkPoint(1,1);
            pointOffsets << PkPoint(0,1);
        }

        return baseColRow + pointOffsets[index];
    }

    struct PointExtension {
        int near;
        int far;
    };
}

inline PkRect calculateCorrectSubGrid(PkRect originalBoundsForGrid, int pixelPrecision, PkRectF currentBounds, PkSize gridSize) {

    if (!PkRectF(originalBoundsForGrid).intersects(currentBounds)) {
        return PkRect();
    }

    PkPointF imaginaryGridStartF = PkPoint(originalBoundsForGrid.x()/pixelPrecision, originalBoundsForGrid.y()/pixelPrecision)*pixelPrecision;

    PkPointF startPointB = currentBounds.topLeft() - imaginaryGridStartF;
    PkPoint startPointG = PkPoint(startPointB.x()/pixelPrecision, startPointB.y()/pixelPrecision);
    startPointG = PkPoint(kisBoundFast(0, startPointG.x(), gridSize.width()), kisBoundFast(0, startPointG.y(), gridSize.height()));

    PkPointF endPointB = currentBounds.bottomRight() + PkPoint(1, 1) - imaginaryGridStartF;
    PkPoint endPointG = PkPoint(std::ceil(endPointB.x()/pixelPrecision), std::ceil(endPointB.y()/pixelPrecision)) + PkPoint(1, 1);
    PkPoint endPointPotential = endPointG;

    PkPoint trueEndPoint = PkPoint(kisBoundFast(0, endPointPotential.x(), gridSize.width()), kisBoundFast(0, endPointPotential.y(), gridSize.height()));

    PkPoint size = trueEndPoint - startPointG;

    return PkRect(startPointG, PkSize(size.x(), size.y()));
}

inline PkList<PkRectF> cutOutSubgridFromBounds(PkRect subGrid, PkRect srcBounds, const PkSize &gridSize, const PkVector<PkPointF> &originalPoints) {
    if (subGrid.width() == 0 || subGrid.height() == 0) {
        return PkList<PkRectF> {PkRectF(srcBounds)};
    }
    PkPoint topLeft = subGrid.topLeft();
    PkPoint bottomRight = subGrid.topLeft() + PkPoint(subGrid.width() - 1, subGrid.height() - 1);

    int topLeftIndex = pointToIndex(topLeft, gridSize);
    int bottomRightIndex = pointToIndex(bottomRight, gridSize);

    topLeftIndex = pkMax(0, pkMin(topLeftIndex, originalPoints.length() - 1));
    bottomRightIndex = pkMax(0, pkMin(bottomRightIndex, originalPoints.length() - 1));

    PkPointF topLeftReal = originalPoints[topLeftIndex];
    PkPointF bottomRightReal = originalPoints[bottomRightIndex];
    PkRectF cutOut = PkRectF(topLeftReal, bottomRightReal);

    PkList<PkRectF> response;
    // *-----------*
    // |    top    |
    // |-----------|
    // | l |xxx| r |
    // | e |xxx| i |
    // | f |xxx| g |
    // | t |xxx| h |
    // |   |xxx| t |
    // |-----------|
    // |   bottom  |
    // *-----------*


    PkRectF top = PkRectF(srcBounds.topLeft(), PkPointF(srcBounds.right() + 1, topLeftReal.y()));
    PkRectF bottom = PkRectF(PkPointF(srcBounds.left(), bottomRightReal.y() + 1), srcBounds.bottomRight() + PkPointF(1, 1));
    PkRectF left = PkRectF(PkPointF(srcBounds.left(), cutOut.top()), PkPointF(cutOut.left(), cutOut.bottom() + 1));
    PkRectF right = PkRectF(PkPointF(cutOut.right() + 1, cutOut.top()), PkPointF(srcBounds.right() + 1, cutOut.bottom() + 1));
    PkList<PkRectF> rects = {top, left, right, bottom};
    for (int i = 0; i < rects.length(); i++) {
        if (!rects[i].isEmpty()) {
            response << rects[i];
        }
    }
    return response;

}




template <class IndexesOp>
bool getOrthogonalPointApproximation(const PkPoint &cellPt,
                                const PkVector<PkPointF> &originalPoints,
                                const PkVector<PkPointF> &transformedPoints,
                                IndexesOp indexesOp,
                                PkPointF *srcPoint,
                                PkPointF *dstPoint)
{
    PkVector<Private::PointExtension> extensionPoints;
    Private::PointExtension ext;

    // left
    if ((ext.near = indexesOp.tryGetValidIndex(cellPt + PkPoint(-1, 0))) >= 0 &&
        (ext.far = indexesOp.tryGetValidIndex(cellPt + PkPoint(-2, 0))) >= 0) {

        extensionPoints << ext;
    }
    // top
    if ((ext.near = indexesOp.tryGetValidIndex(cellPt + PkPoint(0, -1))) >= 0 &&
        (ext.far = indexesOp.tryGetValidIndex(cellPt + PkPoint(0, -2))) >= 0) {

        extensionPoints << ext;
    }
    // right
    if ((ext.near = indexesOp.tryGetValidIndex(cellPt + PkPoint(1, 0))) >= 0 &&
        (ext.far = indexesOp.tryGetValidIndex(cellPt + PkPoint(2, 0))) >= 0) {

        extensionPoints << ext;
    }
    // bottom
    if ((ext.near = indexesOp.tryGetValidIndex(cellPt + PkPoint(0, 1))) >= 0 &&
        (ext.far = indexesOp.tryGetValidIndex(cellPt + PkPoint(0, 2))) >= 0) {

        extensionPoints << ext;
    }

    if (extensionPoints.isEmpty()) {
        // top-left
        if ((ext.near = indexesOp.tryGetValidIndex(cellPt + PkPoint(-1, -1))) >= 0 &&
            (ext.far = indexesOp.tryGetValidIndex(cellPt + PkPoint(-2, -2))) >= 0) {

            extensionPoints << ext;
        }
        // top-right
        if ((ext.near = indexesOp.tryGetValidIndex(cellPt + PkPoint(1, -1))) >= 0 &&
            (ext.far = indexesOp.tryGetValidIndex(cellPt + PkPoint(2, -2))) >= 0) {

            extensionPoints << ext;
        }
        // bottom-right
        if ((ext.near = indexesOp.tryGetValidIndex(cellPt + PkPoint(1, 1))) >= 0 &&
            (ext.far = indexesOp.tryGetValidIndex(cellPt + PkPoint(2, 2))) >= 0) {

            extensionPoints << ext;
        }
        // bottom-left
        if ((ext.near = indexesOp.tryGetValidIndex(cellPt + PkPoint(-1, 1))) >= 0 &&
            (ext.far = indexesOp.tryGetValidIndex(cellPt + PkPoint(-2, 2))) >= 0) {

            extensionPoints << ext;
        }
    }

    if (extensionPoints.isEmpty()) {
        return false;
    }

    int numResultPoints = 0;
    *srcPoint = indexesOp.getSrcPointForce(cellPt);
    *dstPoint = PkPointF();

    for (const Private::PointExtension &ext : extensionPoints) {
        PkPointF near = transformedPoints[ext.near];
        PkPointF far = transformedPoints[ext.far];

        PkPointF nearSrc = originalPoints[ext.near];
        PkPointF farSrc = originalPoints[ext.far];

        PkPointF base1 = nearSrc - farSrc;
        PkPointF base2 = near - far;

        PkPointF pt = near +
            KisAlgebra2D::transformAsBase(*srcPoint - nearSrc, base1, base2);

        *dstPoint += pt;
        numResultPoints++;
    }

    *dstPoint /= numResultPoints;

    return true;
}

template <class PolygonOp, class IndexesOp>
struct IncompletePolygonPolicy {

    static inline bool tryProcessPolygon(int col, int row,
                                         int numExistingPoints,
                                         PolygonOp &polygonOp,
                                         IndexesOp &indexesOp,
                                         const PkVector<int> &polygonPoints,
                                         const PkVector<PkPointF> &originalPoints,
                                         const PkVector<PkPointF> &transformedPoints)
    {
        if (numExistingPoints >= 4) return false;
        if (numExistingPoints == 0) return true;

        PkPolygonF srcPolygon;
        PkPolygonF dstPolygon;

        for (int i = 0; i < 4; i++) {
            const int index = polygonPoints[i];

            if (index >= 0) {
                srcPolygon << originalPoints[index];
                dstPolygon << transformedPoints[index];
            } else {
                PkPoint cellPt = Private::pointPolygonIndexToColRow(PkPoint(col, row), i);
                PkPointF srcPoint;
                PkPointF dstPoint;
                bool result =
                    getOrthogonalPointApproximation(cellPt,
                                                    originalPoints,
                                                    transformedPoints,
                                                    indexesOp,
                                                    &srcPoint,
                                                    &dstPoint);

                if (!result) {
                    //dbgKrita << "*NOT* found any valid point" << allSrcPoints[pointToIndex(cellPt)] << "->" << ppVar(pt);
                    break;
                } else {
                    srcPolygon << srcPoint;
                    dstPolygon << dstPoint;
                }
            }
        }

        if (dstPolygon.size() == 4) {
            PkPainterPath srcPath;
            srcPath.addPolygon(srcPolygon);
            PkPainterPath cropPath;
            cropPath.addPolygon(indexesOp.srcCropPolygon());
            PkPolygonF srcClipPolygon(
                srcPath.intersected(cropPath).toFillPolygon(PkTransform()));

            KisFourPointInterpolatorForward forwardTransform(srcPolygon, dstPolygon);
            for (int i = 0; i < srcClipPolygon.size(); i++) {
                const PkPointF newPt = forwardTransform.map(srcClipPolygon[i]);
                srcClipPolygon[i] = newPt;
            }

            polygonOp(srcPolygon, dstPolygon, srcClipPolygon);
        }

        return true;
    }
};

template <class PolygonOp, class IndexesOp>
struct AlwaysCompletePolygonPolicy {

    static inline bool tryProcessPolygon(int col, int row,
                                         int numExistingPoints,
                                         PolygonOp &polygonOp,
                                         IndexesOp &indexesOp,
                                         const PkVector<int> &polygonPoints,
                                         const PkVector<PkPointF> &originalPoints,
                                         const PkVector<PkPointF> &transformedPoints)
    {
        Q_UNUSED(col);
        Q_UNUSED(row);
        Q_UNUSED(polygonOp);
        Q_UNUSED(indexesOp);
        Q_UNUSED(polygonPoints);
        Q_UNUSED(originalPoints);
        Q_UNUSED(transformedPoints);

        KIS_ASSERT_RECOVER_NOOP(numExistingPoints == 4);
        return false;
    }
};

struct RegularGridIndexesOp {

    RegularGridIndexesOp(const PkSize &gridSize)
        : m_gridSize(gridSize)
    {
    }

    inline PkVector<int> calculateMappedIndexes(int col, int row,
                                               int *numExistingPoints) const {

        *numExistingPoints = 4;
        PkVector<int> cellIndexes =
            GridIterationTools::calculateCellIndexes(col, row, m_gridSize);

        return cellIndexes;
    }

    inline int tryGetValidIndex(const PkPoint &cellPt) const {
        Q_UNUSED(cellPt);

        KIS_ASSERT_RECOVER_NOOP(0 && "Not applicable");
        return -1;
    }

    inline PkPointF getSrcPointForce(const PkPoint &cellPt) const {
        Q_UNUSED(cellPt);

        KIS_ASSERT_RECOVER_NOOP(0 && "Not applicable");
        return PkPointF();
    }

    inline const PkPolygonF srcCropPolygon() const {
        KIS_ASSERT_RECOVER_NOOP(0 && "Not applicable");
        return PkPolygonF();
    }

    PkSize m_gridSize;
};

/**
 * There is a weird problem in fetching correct bounds of the polygon.
 * If the rightmost (bottommost) point of the polygon is integral, then
 * PkRectF() will end exactly on it, but when converting into PkRect the last
 * point will not be taken into account. It happens due to the difference
 * between center-point/topleft-point point representation. In many cases
 * the latter is expected, but we don't work with it in Qt/Krita.
 */
inline void adjustAlignedPolygon(PkPolygonF &polygon)
{
    static const qreal eps = 1e-5;
    static const  PkPointF p1(eps, 0.0);
    static const  PkPointF p2(eps, eps);
    static const  PkPointF p3(0.0, eps);

    polygon[1] += p1;
    polygon[2] += p2;
    polygon[3] += p3;
}

template <class IndexesOp>
bool canProcessRectsInRandomOrder(IndexesOp &indexesOp, const PkVector<PkPointF> &transformedPoints, PkSize grid) {
    return canProcessRectsInRandomOrder(indexesOp, transformedPoints, PkRect(PkPoint(0, 0), grid));
}

template <class IndexesOp>
bool canProcessRectsInRandomOrder(IndexesOp &indexesOp, const PkVector<PkPointF> &transformedPoints, PkRect subgrid) {
    PkVector<int> polygonPoints(4);
    PkPoint startPoint = subgrid.topLeft();
    PkPoint endPoint = subgrid.bottomRight();

    for (int row = startPoint.y(); row < endPoint.y(); row++) {
        for (int col = startPoint.x(); col < endPoint.x(); col++) {
            int numExistingPoints = 0;

            polygonPoints = indexesOp.calculateMappedIndexes(col, row, &numExistingPoints);

            PkPolygonF dstPolygon;

            for (int i = 0; i < polygonPoints.count(); i++) {
                const int index = polygonPoints[i];
                dstPolygon << transformedPoints[index];
            }


            adjustAlignedPolygon(dstPolygon);


            if (!KisAlgebra2D::isPolygonTrulyConvex(dstPolygon)) {
                return false;
            }

        }
    }
    return true;
}



template <template <class PolygonOp, class IndexesOp> class IncompletePolygonPolicy,
          class PolygonOp,
          class IndexesOp>
void iterateThroughGrid(PolygonOp &polygonOp,
                        IndexesOp &indexesOp,
                        const PkSize &gridSize,
                        const PkVector<PkPointF> &originalPoints,
                        const PkVector<PkPointF> &transformedPoints)
{
    iterateThroughGrid<IncompletePolygonPolicy, PolygonOp, IndexesOp>(polygonOp, indexesOp, gridSize, originalPoints, transformedPoints, PkRect(PkPoint(0, 0), gridSize));
}

template <template <class PolygonOp, class IndexesOp> class IncompletePolygonPolicy,
          class PolygonOp,
          class IndexesOp>
void iterateThroughGrid(PolygonOp &polygonOp,
                        IndexesOp &indexesOp,
                        const PkSize &gridSize,
                        const PkVector<PkPointF> &originalPoints,
                        const PkVector<PkPointF> &transformedPoints,
                        const PkRect subGrid)
{
    PkVector<int> polygonPoints(4);
    PkPoint startPoint = subGrid.topLeft();
    PkPoint endPoint = subGrid.bottomRight(); // it's weird but bottomRight on PkRect point does give us one unit of margin on both x and y
    // when start is on (0, 0), and size is (500, 500), bottomRight is on (499, 499)
    // but remember that it also only needs a top left corner of the polygon

    KIS_SAFE_ASSERT_RECOVER(startPoint.x() >= 0 && startPoint.y() >= 0 && endPoint.x() <= gridSize.width() - 1 && endPoint.y() <= gridSize.height() - 1) {
        startPoint = PkPoint(pkMax(startPoint.x(), 0), pkMax(startPoint.y(), 0));
        endPoint = PkPoint(pkMin(endPoint.x(), gridSize.width() - 1), pkMin(startPoint.y(), gridSize.height() - 1));
    }

    for (int row = startPoint.y(); row < endPoint.y(); row++) {
        for (int col = startPoint.x(); col < endPoint.x(); col++) {
            int numExistingPoints = 0;

            polygonPoints = indexesOp.calculateMappedIndexes(col, row, &numExistingPoints);

            if (!IncompletePolygonPolicy<PolygonOp, IndexesOp>::
                 tryProcessPolygon(col, row,
                                   numExistingPoints,
                                   polygonOp,
                                   indexesOp,
                                   polygonPoints,
                                   originalPoints,
                                   transformedPoints)) {

                PkPolygonF srcPolygon;
                PkPolygonF dstPolygon;

                for (int i = 0; i < 4; i++) {
                    const int index = polygonPoints[i];
                    srcPolygon << originalPoints[index];
                    dstPolygon << transformedPoints[index];
                }

                adjustAlignedPolygon(srcPolygon);
                adjustAlignedPolygon(dstPolygon);

                polygonOp(srcPolygon, dstPolygon);
            }
        }
    }

    polygonOp.finalize();
}

}

#endif /* __KIS_GRID_INTERPOLATION_TOOLS_H */
