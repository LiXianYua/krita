/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkGlobal.h>
#include "kis_cage_transform_worker.h"

#include "kis_grid_interpolation_tools.h"
#include "kis_green_coordinates_math.h"

#include "KoColor.h"
#include "kis_selection.h"
#include "kis_painter.h"
#include "kis_image.h"
#include "krita_utils.h"

namespace {

uint32_t sourceOver(uint32_t destination, uint32_t source)
{
    const uint32_t sourceAlpha = source >> 24;
    if (sourceAlpha == 0) return destination;
    if (sourceAlpha == 255) return source;

    const uint32_t destinationAlpha = destination >> 24;
    const uint32_t inverseSourceAlpha = 255 - sourceAlpha;
    const uint32_t outputAlpha =
        sourceAlpha + (destinationAlpha * inverseSourceAlpha + 127) / 255;

    auto blendChannel = [&](int shift) {
        const uint32_t sourceChannel = (source >> shift) & 0xff;
        const uint32_t destinationChannel = (destination >> shift) & 0xff;
        const uint32_t premultiplied =
            sourceChannel * sourceAlpha +
            (destinationChannel * destinationAlpha * inverseSourceAlpha + 127) / 255;
        return (premultiplied + outputAlpha / 2) / outputAlpha;
    };

    return outputAlpha << 24 |
           blendChannel(16) << 16 |
           blendChannel(8) << 8 |
           blendChannel(0);
}

}

struct KisCageTransformWorker::Private
{
    Private(const PkVector<PkPointF> &_origCage,
            KoUpdater *_progress,
            int _pixelPrecision)
        : origCage(_origCage),
          progress(_progress),
          pixelPrecision(_pixelPrecision)
    {
    }

    PkRect srcBounds;

    PkImage srcImage;
    PkPointF srcImageOffset;

    PkVector<PkPointF> origCage;
    PkVector<PkPointF> transfCage;
    KoUpdater *progress;
    int pixelPrecision;

    PkVector<int> allToValidPointsMap;
    PkVector<PkPointF> validPoints;

    /**
     * Contains all points of the grid including non-defined
     * points (the ones which are placed outside the cage).
     */
    PkVector<PkPointF> allSrcPoints;

    KisGreenCoordinatesMath cage;

    PkSize gridSize;

    bool isGridEmpty() const {
        return allSrcPoints.isEmpty();
    }


    PkVector<PkPointF> calculateTransformedPoints();

    inline PkVector<int> calculateMappedIndexes(int col, int row,
                                               int *numExistingPoints);

    int tryGetValidIndex(const PkPoint &cellPt);

    struct MapIndexesOp;
};

KisCageTransformWorker::KisCageTransformWorker(const PkRect &deviceNonDefaultRegion,
                                               const PkVector<PkPointF> &origCage,
                                               KoUpdater *progress,
                                               int pixelPrecision)
    : m_d(new Private(origCage, progress, pixelPrecision))
{
    m_d->srcBounds = deviceNonDefaultRegion;
}

KisCageTransformWorker::KisCageTransformWorker(const PkImage &srcImage,
                                               const PkPointF &srcImageOffset,
                                               const PkVector<PkPointF> &origCage,
                                               KoUpdater *progress,
                                               int pixelPrecision)
    : m_d(new Private(origCage, progress, pixelPrecision))
{
    m_d->srcImage = srcImage;
    m_d->srcImageOffset = srcImageOffset;
    m_d->srcBounds = PkRectF(m_d->srcImageOffset, m_d->srcImage.size()).toAlignedRect();
}

KisCageTransformWorker::~KisCageTransformWorker()
{
}

void KisCageTransformWorker::setTransformedCage(const PkVector<PkPointF> &transformedCage)
{
    m_d->transfCage = transformedCage;
}

struct PointsFetcherOp
{
    PointsFetcherOp(const PkPolygonF &cagePolygon)
        : m_cagePolygon(cagePolygon),
          m_numValidPoints(0)
    {
        m_polygonDirection = KisAlgebra2D::polygonDirection(cagePolygon);
    }

    inline void processPoint(int col, int row,
                             int prevCol, int prevRow,
                             int colIndex, int rowIndex) {

        Q_UNUSED(prevCol);
        Q_UNUSED(prevRow);
        Q_UNUSED(colIndex);
        Q_UNUSED(rowIndex);

        PkPointF pt(col, row);

        if (m_cagePolygon.containsPoint(pt, Pk::OddEvenFill)) {
            KisAlgebra2D::adjustIfOnPolygonBoundary(m_cagePolygon, m_polygonDirection, &pt);

            m_points << pt;
            m_pointValid << true;
            m_numValidPoints++;
        } else {
            m_points << pt;
            m_pointValid << false;
        }
    }

    inline void nextLine() {
    }

    PkVector<bool> m_pointValid;
    PkVector<PkPointF> m_points;
    PkPolygonF m_cagePolygon;
    int m_polygonDirection;
    int m_numValidPoints;
};

void KisCageTransformWorker::prepareTransform()
{
    if (m_d->origCage.size() < 3) return;

    const PkPolygonF srcPolygon(m_d->origCage);

    PkRect srcBounds = m_d->srcBounds;
    srcBounds &= srcPolygon.boundingRect().toAlignedRect();

    // no need to process empty devices
    if (srcBounds.isEmpty()) return;
    m_d->gridSize =
        GridIterationTools::calcGridSize(srcBounds, m_d->pixelPrecision);

    PointsFetcherOp pointsOp(srcPolygon);
    GridIterationTools::processGrid(pointsOp, srcBounds, m_d->pixelPrecision);

    const int numPoints = pointsOp.m_points.size();
    KIS_ASSERT_RECOVER_RETURN(numPoints == m_d->gridSize.width() * m_d->gridSize.height());

    m_d->allSrcPoints = pointsOp.m_points;
    m_d->allToValidPointsMap.resize(pointsOp.m_points.size());
    m_d->validPoints.resize(pointsOp.m_numValidPoints);

    {
        int validIdx = 0;
        for (int i = 0; i < numPoints; i++) {
            const PkPointF &pt = pointsOp.m_points[i];
            const bool pointValid = pointsOp.m_pointValid[i];

            if (pointValid) {
                m_d->validPoints[validIdx] = pt;
                m_d->allToValidPointsMap[i] = validIdx;
                validIdx++;
            } else {
                m_d->allToValidPointsMap[i] = -1;
            }
        }
        KIS_ASSERT_RECOVER_NOOP(validIdx == m_d->validPoints.size());
    }

    m_d->cage.precalculateGreenCoordinates(m_d->origCage, m_d->validPoints);
}

PkVector<PkPointF> KisCageTransformWorker::Private::calculateTransformedPoints()
{
    cage.generateTransformedCageNormals(transfCage);

    const int numValidPoints = validPoints.size();
    PkVector<PkPointF> transformedPoints(numValidPoints);

    for (int i = 0; i < numValidPoints; i++) {
        transformedPoints[i] = cage.transformedPoint(i, transfCage);

        if (pkIsNaN(transformedPoints[i].x()) ||
            pkIsNaN(transformedPoints[i].y())) {
            warnKrita << "WARNING: One grid point has been removed from consideration" << validPoints[i];
            transformedPoints[i] = validPoints[i];
        }

    }

    return transformedPoints;
}

inline PkVector<int> KisCageTransformWorker::Private::
calculateMappedIndexes(int col, int row,
                       int *numExistingPoints)
{
    *numExistingPoints = 0;
    PkVector<int> cellIndexes =
        GridIterationTools::calculateCellIndexes(col, row, gridSize);

    for (int i = 0; i < 4; i++) {
        cellIndexes[i] = allToValidPointsMap[cellIndexes[i]];
        *numExistingPoints += cellIndexes[i] >= 0;
    }

    return cellIndexes;
}



int KisCageTransformWorker::Private::
tryGetValidIndex(const PkPoint &cellPt)
{
    int index = -1;
    if (cellPt.x() >= 0 &&
        cellPt.y() >= 0 &&
        cellPt.x() < gridSize.width() - 1 &&
        cellPt.y() < gridSize.height() - 1) {

        index = allToValidPointsMap[GridIterationTools::pointToIndex(cellPt, gridSize)];
    }

    return index;
}


struct KisCageTransformWorker::Private::MapIndexesOp {

    MapIndexesOp(KisCageTransformWorker::Private *d)
        : m_d(d),
          m_srcCagePolygon(PkPolygonF(m_d->origCage))
    {
    }

    inline PkVector<int> calculateMappedIndexes(int col, int row,
                                               int *numExistingPoints) const {

        return m_d->calculateMappedIndexes(col, row, numExistingPoints);
    }

    inline int tryGetValidIndex(const PkPoint &cellPt) const {
        return m_d->tryGetValidIndex(cellPt);
    }

    inline PkPointF getSrcPointForce(const PkPoint &cellPt) const {
        return m_d->allSrcPoints[GridIterationTools::pointToIndex(cellPt, m_d->gridSize)];
    }

    inline const PkPolygonF srcCropPolygon() const {
        return m_srcCagePolygon;
    }

    KisCageTransformWorker::Private *m_d;
    PkPolygonF m_srcCagePolygon;
};

PkRect KisCageTransformWorker::approxChangeRect(const PkRect &rc)
{
    const qreal margin = 0.30;

    PkVector<PkPointF> cageSamplePoints;

    const int minStep = 3;
    const int maxSamples = 200;

    const int totalPixels = rc.width() * rc.height();
    const int realStep = pkMax(minStep, totalPixels / maxSamples);
    const PkPolygonF cagePolygon(m_d->origCage);

    for (int i = 0; i < totalPixels; i += realStep) {
        const int x = rc.x() + i % rc.width();
        const int y = rc.y() + i / rc.width();

        const PkPointF pt(x, y);
        if (cagePolygon.containsPoint(pt, Pk::OddEvenFill)) {
            cageSamplePoints << pt;
        }
    }

    if (cageSamplePoints.isEmpty()) {
        return rc;
    }

    KisGreenCoordinatesMath cage;
    cage.precalculateGreenCoordinates(m_d->origCage, cageSamplePoints);
    cage.generateTransformedCageNormals(m_d->transfCage);

    const int numValidPoints = cageSamplePoints.size();
    PkVector<PkPointF> transformedPoints(numValidPoints);

    for (int i = 0; i < numValidPoints; i++) {
        transformedPoints[i] = cage.transformedPoint(i, m_d->transfCage);

        if (pkIsNaN(transformedPoints[i].x()) ||
            pkIsNaN(transformedPoints[i].y())) {

            transformedPoints[i] = cageSamplePoints[i];
        }
    }

    PkRect resultRect =
        KisAlgebra2D::approximateRectFromPoints(transformedPoints).toAlignedRect();

    return KisAlgebra2D::blowRect(resultRect | rc, margin);
}

PkRect KisCageTransformWorker::approxNeedRect(const PkRect &rc, const PkRect &fullBounds)
{
    Q_UNUSED(rc);
    return fullBounds;
}

void KisCageTransformWorker::run(KisPaintDeviceSP srcDevice, KisPaintDeviceSP dstDevice)
{
    if (m_d->isGridEmpty()) return;

    KIS_SAFE_ASSERT_RECOVER_RETURN(m_d->origCage.size() >= 3);
    KIS_SAFE_ASSERT_RECOVER_RETURN(m_d->origCage.size() == m_d->transfCage.size());
    KIS_SAFE_ASSERT_RECOVER_RETURN(*srcDevice->colorSpace() == *dstDevice->colorSpace());

    PkVector<PkPointF> transformedPoints = m_d->calculateTransformedPoints();

    KisPaintDeviceSP tempDevice = new KisPaintDevice(dstDevice->colorSpace());

    {
        KisSelectionSP selection = new KisSelection();

        KisPainter painter(selection->pixelSelection());
        painter.setPaintColor(KoColor(Pk::black, selection->pixelSelection()->colorSpace()));
        painter.setAntiAliasPolygonFill(true);
        painter.setFillStyle(KisPainter::FillStyleForegroundColor);
        painter.setStrokeStyle(KisPainter::StrokeStyleNone);

        painter.paintPolygon(m_d->origCage);

        dstDevice->clearSelection(selection);
    }

    GridIterationTools::PaintDevicePolygonOp polygonOp(srcDevice, tempDevice);
    Private::MapIndexesOp indexesOp(m_d.data());
    GridIterationTools::iterateThroughGrid
        <GridIterationTools::IncompletePolygonPolicy>(polygonOp, indexesOp,
                                                      m_d->gridSize,
                                                      m_d->validPoints,
                                                      transformedPoints);

    PkRect rect = tempDevice->extent();
    KisPainter gc(dstDevice);
    gc.bitBlt(rect.topLeft(), tempDevice, rect);
}

PkImage KisCageTransformWorker::runOnImage(PkPointF *newOffset)
{
    if (m_d->isGridEmpty()) return PkImage();

    KIS_ASSERT_RECOVER(m_d->origCage.size() >= 3 &&
                       m_d->origCage.size() == m_d->transfCage.size()) {
        return PkImage();
    }

    KIS_ASSERT_RECOVER(!m_d->srcImage.isNull()) {
        return PkImage();
    }

    KIS_ASSERT_RECOVER(m_d->srcImage.format() == PkImage::Format_ARGB32) {
        return PkImage();
    }

    PkVector<PkPointF> transformedPoints = m_d->calculateTransformedPoints();

    PkRectF dstBounds;
    for (const PkPointF &pt : transformedPoints) {
        KisAlgebra2D::accumulateBounds(pt, &dstBounds);
    }

    const PkRectF srcBounds(m_d->srcImageOffset, m_d->srcImage.size());
    dstBounds |= srcBounds;

    PkPointF dstImageOffset = dstBounds.topLeft();
    *newOffset = dstImageOffset;

    PkRect dstBoundsI = dstBounds.toAlignedRect();


    PkImage dstImage(dstBoundsI.size(), m_d->srcImage.format());
    dstImage.fill(0);

    PkImage tempImage(dstImage);

    {
        const PkPoint imageOffset =
            (m_d->srcImageOffset - dstImageOffset).toPoint();
        for (int y = 0; y < m_d->srcImage.height(); ++y) {
            for (int x = 0; x < m_d->srcImage.width(); ++x) {
                const PkPoint destination = PkPoint(x, y) + imageOffset;
                if (dstImage.rect().contains(destination)) {
                    dstImage.setPixel(destination.x(), destination.y(),
                                      m_d->srcImage.pixel(x, y));
                }
            }
        }

        const PkPolygonF localCage = PkPolygonF(m_d->origCage).translated(
            PkPointF(-dstImageOffset.x(), -dstImageOffset.y()));
        const PkRect cageBounds = localCage.boundingRect().toAlignedRect() & dstImage.rect();
        for (int y = cageBounds.top(); y <= cageBounds.bottom(); ++y) {
            for (int x = cageBounds.left(); x <= cageBounds.right(); ++x) {
                if (localCage.containsPoint(PkPointF(x + 0.5, y + 0.5), Pk::OddEvenFill)) {
                    dstImage.setPixel(x, y, 0);
                }
            }
        }
    }

    GridIterationTools::PkImagePolygonOp polygonOp(m_d->srcImage, tempImage, m_d->srcImageOffset, dstImageOffset);
    Private::MapIndexesOp indexesOp(m_d.data());
    GridIterationTools::iterateThroughGrid
        <GridIterationTools::IncompletePolygonPolicy>(polygonOp, indexesOp,
                                                      m_d->gridSize,
                                                      m_d->validPoints,
                                                      transformedPoints);

    for (int y = 0; y < dstImage.height(); ++y) {
        for (int x = 0; x < dstImage.width(); ++x) {
            dstImage.setPixel(x, y,
                              sourceOver(dstImage.pixel(x, y), tempImage.pixel(x, y)));
        }
    }

    return dstImage;
}
