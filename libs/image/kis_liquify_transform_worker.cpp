/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkGlobal.h>
#include "kis_liquify_transform_worker.h"

#include <KoColorSpace.h>
#include "kis_grid_interpolation_tools.h"
#include "kis_dom_utils.h"
#include "krita_utils.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>
#include <vector>

namespace {

class LiquifySpatialIndex
{
public:
    explicit LiquifySpatialIndex(int cellSize)
        : m_cellSize(pkMax(cellSize, 1))
    {
    }

    void initializeWith(const PkVector<PkPointF> &points)
    {
        m_points.assign(points.begin(), points.end());
        m_buckets.clear();

        for (int i = 0; i < static_cast<int>(m_points.size()); ++i) {
            m_buckets[cellForPoint(m_points[static_cast<std::size_t>(i)])].push_back(i);
        }
    }

    void movePoint(int index, const PkPointF &, const PkPointF &positionAfter)
    {
        KIS_SAFE_ASSERT_RECOVER_RETURN(index >= 0 && index < static_cast<int>(m_points.size()));

        const std::size_t pointIndex = static_cast<std::size_t>(index);
        const Cell oldCell = cellForPoint(m_points[pointIndex]);
        const Cell newCell = cellForPoint(positionAfter);

        if (oldCell != newCell) {
            auto bucketIt = m_buckets.find(oldCell);
            KIS_SAFE_ASSERT_RECOVER_RETURN(bucketIt != m_buckets.end());

            std::vector<int> &bucket = bucketIt->second;
            const auto pointIt = std::find(bucket.begin(), bucket.end(), index);
            KIS_SAFE_ASSERT_RECOVER_RETURN(pointIt != bucket.end());
            bucket.erase(pointIt);
            if (bucket.empty()) {
                m_buckets.erase(bucketIt);
            }

            m_buckets[newCell].push_back(index);
        }

        m_points[pointIndex] = positionAfter;
    }

    void findAllInRange(PkVector<int> &indexes, const PkPointF &center, qreal range) const
    {
        const Cell first = cellForPoint(center - PkPointF(range, range));
        const Cell last = cellForPoint(center + PkPointF(range, range));
        const qreal rangeSquared = range * range;

        for (int y = first.second; y <= last.second; ++y) {
            for (int x = first.first; x <= last.first; ++x) {
                const auto bucketIt = m_buckets.find(Cell(x, y));
                if (bucketIt == m_buckets.end()) continue;

                for (const int index : bucketIt->second) {
                    const PkPointF delta = m_points[static_cast<std::size_t>(index)] - center;
                    if (pkAbs(delta.x()) <= range &&
                        pkAbs(delta.y()) <= range &&
                        delta.x() * delta.x() + delta.y() * delta.y() <= rangeSquared) {
                        indexes.append(index);
                    }
                }
            }
        }
    }

    PkRectF exactBounds() const
    {
        if (m_points.empty()) return PkRectF();

        qreal left = m_points.front().x();
        qreal top = m_points.front().y();
        qreal right = left;
        qreal bottom = top;

        for (const PkPointF &point : m_points) {
            left = pkMin(left, point.x());
            top = pkMin(top, point.y());
            right = pkMax(right, point.x());
            bottom = pkMax(bottom, point.y());
        }

        return PkRectF(PkPointF(left, top), PkPointF(right, bottom));
    }

private:
    using Cell = std::pair<int, int>;

    Cell cellForPoint(const PkPointF &point) const
    {
        return Cell(static_cast<int>(std::floor(point.x() / m_cellSize)),
                    static_cast<int>(std::floor(point.y() / m_cellSize)));
    }

private:
    int m_cellSize;
    std::vector<PkPointF> m_points;
    std::map<Cell, std::vector<int>> m_buckets;
};

}


struct KisLiquifyTransformWorker::Private
{
    Private(const PkRect &_srcBounds,
            KoUpdater *_progress,
            int _pixelPrecision)
        : srcBounds(_srcBounds)
        , originalPointsContainer(_pixelPrecision * 10)
        , transformedPointsContainer(_pixelPrecision * 10)
        , progress(_progress)
        , pixelPrecision(_pixelPrecision)
    {
    }

    PkRect srcBounds;

    PkVector<PkPointF> originalPoints;
    PkVector<PkPointF> transformedPoints;

    LiquifySpatialIndex originalPointsContainer;
    LiquifySpatialIndex transformedPointsContainer;

    PkRectF accumulatedBrushStrokes;

    KoUpdater *progress;
    int pixelPrecision;
    PkSize gridSize;

    void preparePoints();

    struct MapIndexesOp;

    template <class ProcessOp>
    void processTransformedPixelsBuildUp(ProcessOp op,
                                         const PkPointF &base,
                                         qreal sigma);

    template <class ProcessOp>
    void processTransformedPixelsWash(ProcessOp op,
                                      const PkPointF &base,
                                      qreal sigma,
                                      qreal flow);

    template <class ProcessOp>
    void processTransformedPixels(ProcessOp op,
                                  const PkPointF &base,
                                  qreal sigma,
                                  bool useWashMode,
                                  qreal flow);
};

KisLiquifyTransformWorker::KisLiquifyTransformWorker(const PkRect &srcBounds,
                                                     KoUpdater *progress,
                                                     int pixelPrecision)
    : m_d(new Private(srcBounds, progress, pixelPrecision))
{
    KIS_ASSERT_RECOVER_RETURN(!srcBounds.isEmpty());

    // TODO: implement 'progress' stuff
    m_d->preparePoints();
}

KisLiquifyTransformWorker::KisLiquifyTransformWorker(const KisLiquifyTransformWorker &rhs)
    : m_d(new Private(*rhs.m_d.data()))
{
}

KisLiquifyTransformWorker::~KisLiquifyTransformWorker()
{
}

bool KisLiquifyTransformWorker::operator==(const KisLiquifyTransformWorker &other) const
{
    bool result =
            m_d->srcBounds == other.m_d->srcBounds &&
            m_d->pixelPrecision == other.m_d->pixelPrecision &&
            m_d->gridSize == other.m_d->gridSize &&
            m_d->originalPoints.size() == other.m_d->originalPoints.size() &&
            m_d->transformedPoints.size() == other.m_d->transformedPoints.size();

    if (!result) return false;

    const qreal eps = 1e-6;

    result =
        KisAlgebra2D::fuzzyPointCompare(m_d->originalPoints, other.m_d->originalPoints, eps) &&
        KisAlgebra2D::fuzzyPointCompare(m_d->transformedPoints, other.m_d->transformedPoints, eps);

    return result;
}

bool KisLiquifyTransformWorker::isIdentity() const
{
    const qreal eps = 1e-6;
    return KisAlgebra2D::fuzzyPointCompare(m_d->originalPoints, m_d->transformedPoints, eps);
}

int KisLiquifyTransformWorker::pointToIndex(const PkPoint &cellPt)
{
    return GridIterationTools::pointToIndex(cellPt, m_d->gridSize);
}

PkSize KisLiquifyTransformWorker::gridSize() const
{
    return m_d->gridSize;
}

const PkVector<PkPointF>& KisLiquifyTransformWorker::originalPoints() const
{
    return m_d->originalPoints;
}

PkVector<PkPointF>& KisLiquifyTransformWorker::transformedPoints()
{
    return m_d->transformedPoints;
}

struct AllPointsFetcherOp
{
    AllPointsFetcherOp(PkRectF srcRect) : m_srcRect(srcRect) {}

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
    PkRectF m_srcRect;
};

void KisLiquifyTransformWorker::Private::preparePoints()
{
    gridSize =
        GridIterationTools::calcGridSize(srcBounds, pixelPrecision);

    AllPointsFetcherOp pointsOp{PkRectF(srcBounds)};
    GridIterationTools::processGrid(pointsOp, srcBounds, pixelPrecision);

    const int numPoints = pointsOp.m_points.size();

    KIS_ASSERT_RECOVER_RETURN(numPoints == gridSize.width() * gridSize.height());

    originalPoints = pointsOp.m_points;
    transformedPoints = pointsOp.m_points;

    originalPointsContainer.initializeWith(originalPoints);
    transformedPointsContainer.initializeWith(transformedPoints);

}

void KisLiquifyTransformWorker::translate(const PkPointF &offset)
{
    KIS_ASSERT_RECOVER_RETURN(m_d->originalPoints.size() ==
                              m_d->transformedPoints.size());

    // TODO: make it within Spatial Container, either a hidden offset, or just offsetting all points at once
    // and benchmark
    for (int i = 0; i < m_d->transformedPoints.count(); i++) {
        m_d->originalPointsContainer.movePoint(i, m_d->originalPoints[i], m_d->originalPoints[i] + offset);
        m_d->transformedPointsContainer.movePoint(i, m_d->transformedPoints[i], m_d->transformedPoints[i] + offset);

        m_d->originalPoints[i] += offset;
        m_d->transformedPoints[i] += offset;
    }

    m_d->accumulatedBrushStrokes.translate(offset);
}

void KisLiquifyTransformWorker::translateDstSpace(const PkPointF &offset)
{
    // TODO: make it within Spatial Container, either a hidden offset, or just offsetting all points at once
    // and benchmark
    for (int i = 0; i < m_d->transformedPoints.count(); i++) {
        m_d->transformedPointsContainer.movePoint(i, m_d->transformedPoints[i], m_d->transformedPoints[i] + offset);
        m_d->transformedPoints[i] += offset;
    }
}

void KisLiquifyTransformWorker::undoPoints(const PkPointF &base,
                                           qreal amount,
                                           qreal sigma)
{
    const qreal maxDistCoeff = 3.0;
    const qreal maxDist = maxDistCoeff * sigma;

    KIS_ASSERT_RECOVER_RETURN(m_d->originalPoints.size() ==
                              m_d->transformedPoints.size());

    PkVector<int> indexes;
    m_d->transformedPointsContainer.findAllInRange(indexes, base, maxDist);
    for (int i = 0; i < indexes.count(); i++) {

        PkPointF diff = m_d->transformedPoints[indexes[i]] - base;
        qreal dist = KisAlgebra2D::norm(diff);
        qreal lambda = exp(-0.5 * pow2(dist / sigma));
        lambda *= amount;

        PkPointF oldPosition = m_d->transformedPoints[indexes[i]];
        m_d->transformedPoints[indexes[i]] = m_d->originalPoints[indexes[i]] * lambda + m_d->transformedPoints[indexes[i]] * (1.0 - lambda);

        m_d->transformedPointsContainer.movePoint(indexes[i], oldPosition, m_d->transformedPoints[indexes[i]]);
    }
}

template <class ProcessOp>
void KisLiquifyTransformWorker::Private::
processTransformedPixelsBuildUp(ProcessOp op,
                                const PkPointF &base,
                                qreal sigma)
{
    const qreal maxDist = ProcessOp::maxDistCoeff * sigma;
    PkRectF clipRect(base.x() - maxDist, base.y() - maxDist,
                    2 * maxDist, 2 * maxDist);

    accumulatedBrushStrokes |= kisGrowRect(clipRect, pixelPrecision);

    PkVector<int> indexes;
    transformedPointsContainer.findAllInRange(indexes, base, maxDist);

    for (int i = 0; i < indexes.count(); i++) {

        PkPointF diff = transformedPoints[indexes[i]] - base;
        qreal dist = KisAlgebra2D::norm(diff);
        if (dist > maxDist) continue;

        const qreal lambda = exp(-0.5 * pow2(dist / sigma));
        PkPointF oldPosition = transformedPoints[indexes[i]];
        transformedPoints[indexes[i]] = op(transformedPoints[indexes[i]], base, diff, lambda);


        transformedPointsContainer.movePoint(indexes[i], oldPosition, transformedPoints[indexes[i]]);

    }
}

template <class ProcessOp>
void KisLiquifyTransformWorker::Private::
processTransformedPixelsWash(ProcessOp op,
                             const PkPointF &base,
                             qreal sigma,
                             qreal flow)
{
    const qreal maxDist = ProcessOp::maxDistCoeff * sigma;
    PkRectF clipRect(base.x() - maxDist, base.y() - maxDist,
                    2 * maxDist, 2 * maxDist);

    accumulatedBrushStrokes |= kisGrowRect(clipRect, pixelPrecision);

    KIS_ASSERT_RECOVER_RETURN(originalPoints.size() ==
                              transformedPoints.size());

    // TODO: remove the originalPointsContainer entirely, and use GridIterationTools to figure out indexes instead
    // and add unit tests for it

    PkVector<int> indexes;
    originalPointsContainer.findAllInRange(indexes, base, maxDist);
    for (int i = 0; i < indexes.count(); i++) {

        PkPointF diff = originalPoints[indexes[i]] - base;
        qreal dist = KisAlgebra2D::norm(diff);

        const qreal lambda = exp(-0.5 * pow2(dist / sigma));
        PkPointF dstPt = op(originalPoints[indexes[i]], base, diff, lambda);

        if (kisDistance(dstPt, originalPoints[indexes[i]]) > kisDistance(transformedPoints[indexes[i]], originalPoints[indexes[i]])) {
            PkPointF oldPosition = transformedPoints[indexes[i]];
            transformedPoints[indexes[i]] = (1.0 - flow) * transformedPoints[indexes[i]] + flow * dstPt;

            transformedPointsContainer.movePoint(indexes[i], oldPosition, transformedPoints[indexes[i]]);
        }
    }
}

template <class ProcessOp>
void KisLiquifyTransformWorker::Private::
processTransformedPixels(ProcessOp op,
                         const PkPointF &base,
                         qreal sigma,
                         bool useWashMode,
                         qreal flow)
{
    if (useWashMode) {
        processTransformedPixelsWash(op, base, sigma, flow);
    } else {
        processTransformedPixelsBuildUp(op, base, sigma);
    }
}

struct TranslateOp
{
    TranslateOp(const PkPointF &offset) : m_offset(offset) {}

    PkPointF operator() (const PkPointF &pt,
                        const PkPointF &base,
                        const PkPointF &diff,
                        qreal lambda)
    {
        Q_UNUSED(base);
        Q_UNUSED(diff);
        return pt + lambda * m_offset;
    }

    static const qreal maxDistCoeff;

    PkPointF m_offset;
};

const qreal TranslateOp::maxDistCoeff = 3.0;

struct ScaleOp
{
    ScaleOp(qreal scale) : m_scale(scale) {}

    PkPointF operator() (const PkPointF &pt,
                        const PkPointF &base,
                        const PkPointF &diff,
                        qreal lambda)
    {
        Q_UNUSED(pt);
        Q_UNUSED(diff);
        return base + (1.0 + m_scale * lambda) * diff;
    }

    static const qreal maxDistCoeff;

    qreal m_scale;
};

const qreal ScaleOp::maxDistCoeff = 3.0;

struct RotateOp
{
    RotateOp(qreal angle) : m_angle(angle) {}

    PkPointF operator() (const PkPointF &pt,
                        const PkPointF &base,
                        const PkPointF &diff,
                        qreal lambda)
    {
        Q_UNUSED(pt);

        const qreal angle = m_angle * lambda;
        const qreal sinA = std::sin(angle);
        const qreal cosA = std::cos(angle);

        qreal x =  cosA * diff.x() + sinA * diff.y();
        qreal y = -sinA * diff.x() + cosA * diff.y();

        return base + PkPointF(x, y);
    }

    static const qreal maxDistCoeff;

    qreal m_angle;
};

const qreal RotateOp::maxDistCoeff = 3.0;

void KisLiquifyTransformWorker::translatePoints(const PkPointF &base,
                                                const PkPointF &offset,
                                                qreal sigma,
                                                bool useWashMode,
                                                qreal flow)
{
    TranslateOp op(offset);
    m_d->processTransformedPixels(op, base, sigma, useWashMode, flow);
}

void KisLiquifyTransformWorker::scalePoints(const PkPointF &base,
                                            qreal scale,
                                            qreal sigma,
                                            bool useWashMode,
                                            qreal flow)
{
    ScaleOp op(scale);
    m_d->processTransformedPixels(op, base, sigma, useWashMode, flow);
}

void KisLiquifyTransformWorker::rotatePoints(const PkPointF &base,
                                             qreal angle,
                                             qreal sigma,
                                             bool useWashMode,
                                             qreal flow)
{
    RotateOp op(angle);
    m_d->processTransformedPixels(op, base, sigma, useWashMode, flow);
}

void KisLiquifyTransformWorker::run(KisPaintDeviceSP srcDevice, KisPaintDeviceSP dstDevice)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(*srcDevice->colorSpace() == *dstDevice->colorSpace());

    dstDevice->clear();

    using namespace GridIterationTools;
    PkRect correctSubGrid = calculateCorrectSubGrid(m_d->srcBounds, m_d->pixelPrecision, m_d->accumulatedBrushStrokes, m_d->gridSize);

    PaintDevicePolygonOp polygonOp(srcDevice, dstDevice);
    RegularGridIndexesOp indexesOp(m_d->gridSize);

    bool canMergeRects = GridIterationTools::canProcessRectsInRandomOrder(indexesOp, m_d->transformedPoints, correctSubGrid);
    polygonOp.setCanMergeRects(canMergeRects);

#ifdef DEBUG_PAINTING_POLYGONS
    polygonOp.setDebugColor(Pk::red);
#endif

    iterateThroughGrid<AlwaysCompletePolygonPolicy>(polygonOp, indexesOp,
                                                    m_d->gridSize,
                                                    m_d->originalPoints,
                                                    m_d->transformedPoints,
                                                    correctSubGrid);
    PkList<PkRectF> areasToCopy = cutOutSubgridFromBounds(correctSubGrid, m_d->srcBounds, m_d->gridSize, m_d->originalPoints);
#ifdef DEBUG_PAINTING_POLYGONS
    PkList<PkColor> colors = {Pk::blue, Pk::green, Pk::yellow, Pk::black};
#endif
    for (int i = 0; i < areasToCopy.length(); i++) {
#ifdef DEBUG_PAINTING_POLYGONS
        polygonOp.setDebugColor(colors[i]);
#endif
        polygonOp.fastCopyArea(areasToCopy[i].toRect(), false);
    }
}

PkRect KisLiquifyTransformWorker::approxChangeRect(const PkRect &rc)
{
    const qreal margin = 0.05;
    PkRect resultRect = m_d->transformedPointsContainer.exactBounds().toRect();
    return KisAlgebra2D::blowRect(resultRect | rc, margin);
}

PkRect KisLiquifyTransformWorker::approxNeedRect(const PkRect &rc, const PkRect &fullBounds)
{
    Q_UNUSED(rc);
    return fullBounds;
}

PkRectF KisLiquifyTransformWorker::accumulatedStrokesBounds() const
{
    return m_d->accumulatedBrushStrokes;
}

void KisLiquifyTransformWorker::transformSrcAndDst(const PkTransform &t)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(t.type() <= PkTransform::TxScale);

    m_d->srcBounds = t.mapRect(m_d->srcBounds);

    // TODO: do it within Spatial Container
    for (int i = 0; i < m_d->transformedPoints.count(); i++) {
        m_d->originalPointsContainer.movePoint(i, m_d->originalPoints[i], t.map(m_d->originalPoints[i]));
        m_d->transformedPointsContainer.movePoint(i, m_d->transformedPoints[i], t.map(m_d->transformedPoints[i]));

        m_d->originalPoints[i] = t.map(m_d->originalPoints[i]);
        m_d->transformedPoints[i] = t.map(m_d->transformedPoints[i]);
    }
    m_d->accumulatedBrushStrokes = t.map(m_d->accumulatedBrushStrokes).boundingRect();
    if (t == PkTransform::fromScale(t.m11(), t.m22()) && t.m11() == t.m22()) {
        m_d->pixelPrecision *= t.m11();
        KIS_SAFE_ASSERT_RECOVER(m_d->pixelPrecision > 0) { m_d->pixelPrecision = 1; }
        KIS_SAFE_ASSERT_RECOVER(PkList<int>({1, 2, 4, 8, 16}).contains(m_d->pixelPrecision) || m_d->pixelPrecision%16 == 0) { m_d->pixelPrecision = 1; }
        // should check if pixelPrecision is a power of 2, but that's more complicated
    }
}

#include <functional>
#include <PkTransform.h>

using PointMapFunction = std::function<PkPointF (const PkPointF&)>;


PointMapFunction bindPointMapTransform(const PkTransform &transform) {
    using namespace std::placeholders;

    typedef PkPointF (PkTransform::*MapFuncType)(const PkPointF&) const;
    return std::bind(static_cast<MapFuncType>(&PkTransform::map), &transform, _1);
}

PkImage KisLiquifyTransformWorker::runOnImage(const PkImage &srcImage,
                                              const PkPointF &srcImageOffset,
                                              const PkTransform &imageToThumbTransform,
                                              PkPointF *newOffset)
{
    KIS_ASSERT_RECOVER(m_d->originalPoints.size() == m_d->transformedPoints.size()) {
        return PkImage();
    }

    KIS_ASSERT_RECOVER(!srcImage.isNull()) {
        return PkImage();
    }

    KIS_ASSERT_RECOVER(srcImage.format() == PkImage::Format_ARGB32) {
        return PkImage();
    }

    PkVector<PkPointF> originalPointsLocal(m_d->originalPoints);
    PkVector<PkPointF> transformedPointsLocal(m_d->transformedPoints);

    PointMapFunction mapFunc = bindPointMapTransform(imageToThumbTransform);

    std::transform(originalPointsLocal.begin(), originalPointsLocal.end(),
                   originalPointsLocal.begin(), mapFunc);

    std::transform(transformedPointsLocal.begin(), transformedPointsLocal.end(),
                   transformedPointsLocal.begin(), mapFunc);

    PkRectF dstBounds;
    for (const PkPointF &pt : transformedPointsLocal) {
        KisAlgebra2D::accumulateBounds(pt, &dstBounds);
    }

    const PkRectF srcBounds(srcImageOffset, srcImage.size());
    dstBounds |= srcBounds;

    PkPointF dstImageOffset = dstBounds.topLeft();
    *newOffset = dstImageOffset;

    PkRect dstBoundsI = dstBounds.toAlignedRect();

    PkImage dstImage(dstBoundsI.size(), srcImage.format());
    dstImage.fill(0);

    GridIterationTools::PkImagePolygonOp polygonOp(srcImage, dstImage, srcImageOffset, dstImageOffset);
    GridIterationTools::RegularGridIndexesOp indexesOp(m_d->gridSize);


    PkRect correctSubGrid = GridIterationTools::calculateCorrectSubGrid(m_d->srcBounds, m_d->pixelPrecision, m_d->accumulatedBrushStrokes, m_d->gridSize);
    bool canMergeRects = GridIterationTools::canProcessRectsInRandomOrder(indexesOp, m_d->transformedPoints, correctSubGrid);
    polygonOp.setCanMergeRects(canMergeRects);


    GridIterationTools::iterateThroughGrid<GridIterationTools::AlwaysCompletePolygonPolicy>(polygonOp, indexesOp,
                                                    m_d->gridSize,
                                                    originalPointsLocal,
                                                    transformedPointsLocal,
                                                    correctSubGrid);


    PkList<PkRectF> areasToCopy = GridIterationTools::cutOutSubgridFromBounds(correctSubGrid, m_d->srcBounds, m_d->gridSize, m_d->originalPoints);
    polygonOp.setCanMergeRects(false);
    const qreal eps = 0.001;
    for (int i = 0; i < areasToCopy.length(); i++) {
        PkPolygonF transformed = imageToThumbTransform.map(PkPolygonF(areasToCopy[i]));
        if (KisAlgebra2D::isPolygonPixelAlignedRect(transformed, eps)) {
            polygonOp.fastCopyArea(transformed.boundingRect().toRect());
        } else {
            polygonOp.operator()(transformed, transformed);
        }
    }
    return dstImage;
}


void KisLiquifyTransformWorker::toXML(PkXmlElement *e) const
{
    PkXmlDocument doc = e->ownerDocument();
    PkXmlElement liqEl = doc.createElement("liquify_points");
    e->appendChild(liqEl);

    KisDomUtils::saveValue(&liqEl, "srcBounds", m_d->srcBounds);
    KisDomUtils::saveValue(&liqEl, "originalPoints", m_d->originalPoints);
    KisDomUtils::saveValue(&liqEl, "transformedPoints", m_d->transformedPoints);
    KisDomUtils::saveValue(&liqEl, "pixelPrecision", m_d->pixelPrecision);
    KisDomUtils::saveValue(&liqEl, "gridSize", m_d->gridSize);
}

KisLiquifyTransformWorker* KisLiquifyTransformWorker::fromXML(const PkXmlElement &e)
{
    PkXmlElement liquifyEl;

    PkRect srcBounds;
    PkVector<PkPointF> originalPoints;
    PkVector<PkPointF> transformedPoints;
    int pixelPrecision;
    PkSize gridSize;

    bool result = false;


    result =
        KisDomUtils::findOnlyElement(e, "liquify_points", &liquifyEl) &&

        KisDomUtils::loadValue(liquifyEl, "srcBounds", &srcBounds) &&
        KisDomUtils::loadValue(liquifyEl, "originalPoints", &originalPoints) &&
        KisDomUtils::loadValue(liquifyEl, "transformedPoints", &transformedPoints) &&
        KisDomUtils::loadValue(liquifyEl, "pixelPrecision", &pixelPrecision) &&
        KisDomUtils::loadValue(liquifyEl, "gridSize", &gridSize);

    if (!result) {
        warnKrita << "WARNING: Failed to load liquify worker from XML";
        return new KisLiquifyTransformWorker(PkRect(0,0,1024, 1024), 0, 8);
    }

    KisLiquifyTransformWorker *worker =
        new KisLiquifyTransformWorker(srcBounds, 0, pixelPrecision);

    const int numPoints = originalPoints.size();

    if (numPoints != transformedPoints.size() ||
        numPoints != worker->m_d->originalPoints.size() ||
        gridSize != worker->m_d->gridSize) {
        warnKrita << "WARNING: Inconsistent number of points!";
        warnKrita << ppVar(originalPoints.size());
        warnKrita << ppVar(transformedPoints.size());
        warnKrita << ppVar(gridSize);
        warnKrita << ppVar(worker->m_d->originalPoints.size());
        warnKrita << ppVar(worker->m_d->transformedPoints.size());
        warnKrita << ppVar(worker->m_d->gridSize);

        return worker;
    }

    PkRectF changedRect = PkRectF();

    for (int i = 0; i < numPoints; i++) {
        worker->m_d->originalPoints[i] = originalPoints[i];
        worker->m_d->transformedPoints[i] = transformedPoints[i];
        if (!KisAlgebra2D::fuzzyPointCompare(transformedPoints[i], originalPoints[i])) {
            KisAlgebra2D::accumulateBounds(transformedPoints[i], &changedRect);
            KisAlgebra2D::accumulateBounds(originalPoints[i], &changedRect);
        }
    }
    changedRect = kisGrowRect(changedRect, pixelPrecision);

    worker->m_d->transformedPointsContainer.initializeWith(worker->m_d->transformedPoints);
    worker->m_d->originalPointsContainer.initializeWith(worker->m_d->originalPoints);

    worker->m_d->accumulatedBrushStrokes = changedRect;


    return worker;
}
