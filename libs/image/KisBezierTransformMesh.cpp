/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisBezierTransformMesh.h"

#include "kis_grid_interpolation_tools.h"
#include <KisBezierPatchParamSpaceUtils.h>
#include <KisSampleRectIterator.h>
#include <KisBezierPatchParamToSourceSampler.h>
#include "kis_debug.h"

KisBezierTransformMesh::patch_const_iterator
KisBezierTransformMesh::hitTestPatchImpl(const PkPointF &pt, PkPointF *localPointResult) const
{
    auto result = endPatches();

    const PkRectF unitRect(0, 0, 1, 1);

    for (auto it = beginPatches(); it != endPatches(); ++it) {
        Patch patch = *it;

        if (patch.dstBoundingRect().contains(pt)) {
            const PkPointF localPos = KisBezierUtils::calculateLocalPos(patch.points, pt);

            if (unitRect.contains(localPos)) {

                if (localPointResult) {
                    *localPointResult = localPos;
                }

                result = it;
                break;
            }
        }
    }

    return result;
}

KisBezierTransformMesh::PatchIndex KisBezierTransformMesh::hitTestPatch(const PkPointF &pt, PkPointF *localPointResult) const
{
    return hitTestPatchImpl(pt, localPointResult).patchIndex();
}

PkRect KisBezierTransformMesh::hitTestPatchInSourceSpace(const PkRectF &rect) const
{
    const PkRectF searchRect = rect & m_originalRect;

    if (searchRect.isEmpty()) return PkRect();

    const PkPointF proportionalTL = KisAlgebra2D::absoluteToRelative(searchRect.topLeft(), m_originalRect);
    const PkPointF proportionalBR = KisAlgebra2D::absoluteToRelative(searchRect.bottomRight(), m_originalRect);

    const auto topItY = prev(upper_bound(m_rows.begin(), prev(m_rows.end()), proportionalTL.y()));
    const int topRow = distance(m_rows.begin(), topItY);

    const auto leftItX = prev(upper_bound(m_columns.begin(), prev(m_columns.end()), proportionalTL.x()));
    const int leftColumn = distance(m_columns.begin(), leftItX);

    const auto bottomItY = prev(upper_bound(m_rows.begin(), prev(m_rows.end()), proportionalBR.y()));
    const int bottomRow = distance(m_rows.begin(), bottomItY);

    const auto rightItX = prev(upper_bound(m_columns.begin(), prev(m_columns.end()), proportionalBR.x()));
    const int rightColumn = distance(m_columns.begin(), rightItX);

    return PkRect(leftColumn, topRow,
                 rightColumn - leftColumn + 1,
                 bottomRow - topRow + 1);
}

void KisBezierTransformMesh::transformPatch(const KisBezierPatch &patch, const PkPoint &srcImageOffset, const PkImage &srcImage, const PkPoint &dstImageOffset, PkImage *dstImage)
{
    PkVector<PkPointF> originalPointsLocal;
    PkVector<PkPointF> transformedPointsLocal;
    PkSize gridSize;

    patch.sampleRegularGrid(gridSize, originalPointsLocal, transformedPointsLocal, PkPointF(8,8));

    const PkRect dstBoundsI = patch.dstBoundingRect().toAlignedRect();
    const PkRect imageSize = PkRect(dstImageOffset, dstImage->size());
    KIS_SAFE_ASSERT_RECOVER_NOOP(imageSize.contains(dstBoundsI));

    {
        GridIterationTools::PkImagePolygonOp polygonOp(srcImage, *dstImage, srcImageOffset, dstImageOffset);

        GridIterationTools::RegularGridIndexesOp indexesOp(gridSize);
        GridIterationTools::iterateThroughGrid
                <GridIterationTools::AlwaysCompletePolygonPolicy>(polygonOp, indexesOp,
                                                                  gridSize,
                                                                  originalPointsLocal,
                                                                  transformedPointsLocal);
    }
}

void KisBezierTransformMesh::transformPatch(const KisBezierPatch &patch, KisPaintDeviceSP srcDevice, KisPaintDeviceSP dstDevice)
{
    PkVector<PkPointF> originalPointsLocal;
    PkVector<PkPointF> transformedPointsLocal;
    PkSize gridSize;

    patch.sampleRegularGrid(gridSize, originalPointsLocal, transformedPointsLocal, PkPointF(8,8));

    {
        GridIterationTools::PaintDevicePolygonOp polygonOp(srcDevice, dstDevice);

        GridIterationTools::RegularGridIndexesOp indexesOp(gridSize);
        GridIterationTools::iterateThroughGrid
                <GridIterationTools::AlwaysCompletePolygonPolicy>(polygonOp, indexesOp,
                                                                  gridSize,
                                                                  originalPointsLocal,
                                                                  transformedPointsLocal);
    }
}

void KisBezierTransformMesh::transformMesh(const PkPoint &srcImageOffset, const PkImage &srcImage, const PkPoint &dstImageOffset, PkImage *dstImage) const
{
    for (auto it = beginPatches(); it != endPatches(); ++it) {
        transformPatch(*it, srcImageOffset, srcImage, dstImageOffset, dstImage);
    }
}

void KisBezierTransformMesh::transformMesh(KisPaintDeviceSP srcDevice, KisPaintDeviceSP dstDevice) const
{
    for (auto it = beginPatches(); it != endPatches(); ++it) {
        transformPatch(*it, srcDevice, dstDevice);
    }
}

PkRect KisBezierTransformMesh::approxNeedRect(const PkRect &rc) const
{
    PkRect result;

    const PkRect sampleRect = rc & dstBoundingRect().toAlignedRect();
    if (sampleRect.isEmpty()) return result;

    const PkRectF unitRect(0, 0, 1, 1);
    const int samplesLimit = sampleRect.width() * sampleRect.height() / 2;

    PkRectF stepRect;

    {
        /**
         * First, try to approximate the bounding need rect by sampling
         * control points. That is the main property of bezier curves:
         * the resulting curve is **always** contained inside the control
         * polygon.
         *
         * TODO: sample the whole wrapping polygon in a more uniform way,
         * that is, sample the whole perimeter of the patch.
         */

        const PkRectF dstRect(rc);

        auto tryAddHandle = [&dstRect, &stepRect] (const KisBezierPatch &patch, KisBezierPatch::ControlPointType controlType) {

            auto fetchLocalPoint =
                    [] (const KisBezierPatch &patch,
                        KisBezierPatch::ControlPointType c0,
                        KisBezierPatch::ControlPointType c1,
                        KisBezierPatch::ControlPointType c2,
                        KisBezierPatch::ControlPointType c3) {

                const qreal handleLength = kisDistance(patch.points[c0], patch.points[c1]);
                const qreal totalLength = handleLength +
                        kisDistance(patch.points[c1], patch.points[c2]) +
                        kisDistance(patch.points[c2], patch.points[c3]);

                return KisAlgebra2D::lerp(patch.originalRect.topLeft(), patch.originalRect.topRight(),
                                          handleLength / totalLength);
            };

            if (dstRect.contains(patch.points[controlType])) {
                PkPointF localPoint;

                switch (controlType) {
                case KisBezierPatch::TL:
                    localPoint = patch.originalRect.topLeft();
                    break;
                case KisBezierPatch::TL_HC: {
                    localPoint = fetchLocalPoint(patch,
                                                 KisBezierPatch::TL,
                                                 KisBezierPatch::TL_HC,
                                                 KisBezierPatch::TR_HC,
                                                 KisBezierPatch::TR);
                    break;
                }
                case KisBezierPatch::TL_VC:
                    localPoint = fetchLocalPoint(patch,
                                                 KisBezierPatch::TL,
                                                 KisBezierPatch::TL_VC,
                                                 KisBezierPatch::BL_VC,
                                                 KisBezierPatch::BL);
                    break;
                case KisBezierPatch::TR:
                    localPoint = patch.originalRect.topRight();
                    break;
                case KisBezierPatch::TR_HC:
                    localPoint = fetchLocalPoint(patch,
                                                 KisBezierPatch::TR,
                                                 KisBezierPatch::TR_HC,
                                                 KisBezierPatch::TL_HC,
                                                 KisBezierPatch::TL);
                    break;
                case KisBezierPatch::TR_VC:
                    localPoint = fetchLocalPoint(patch,
                                                 KisBezierPatch::TR,
                                                 KisBezierPatch::TR_VC,
                                                 KisBezierPatch::BR_VC,
                                                 KisBezierPatch::BR);

                    break;
                case KisBezierPatch::BL:
                    localPoint = patch.originalRect.bottomLeft();
                    break;
                case KisBezierPatch::BL_HC:
                    localPoint = fetchLocalPoint(patch,
                                                 KisBezierPatch::BL,
                                                 KisBezierPatch::BL_HC,
                                                 KisBezierPatch::BR_HC,
                                                 KisBezierPatch::BR);
                    break;
                case KisBezierPatch::BL_VC:
                    localPoint = fetchLocalPoint(patch,
                                                 KisBezierPatch::BL,
                                                 KisBezierPatch::BL_VC,
                                                 KisBezierPatch::TL_VC,
                                                 KisBezierPatch::TL);
                    break;
                case KisBezierPatch::BR:
                    localPoint = patch.originalRect.bottomRight();
                    break;
                case KisBezierPatch::BR_HC:
                    localPoint = fetchLocalPoint(patch,
                                                 KisBezierPatch::BR,
                                                 KisBezierPatch::BR_HC,
                                                 KisBezierPatch::BL_HC,
                                                 KisBezierPatch::BL);
                    break;
                case KisBezierPatch::BR_VC:
                    localPoint = fetchLocalPoint(patch,
                                                 KisBezierPatch::BR,
                                                 KisBezierPatch::BR_VC,
                                                 KisBezierPatch::TR_VC,
                                                 KisBezierPatch::TR);

                    break;
                }

                KisAlgebra2D::accumulateBounds(localPoint, &stepRect);
            }
        };

        for (auto it = beginPatches(); it != endPatches(); ++it) {
            tryAddHandle(*it, KisBezierPatch::TL);
            tryAddHandle(*it, KisBezierPatch::TL_HC);
            tryAddHandle(*it, KisBezierPatch::TL_VC);

            tryAddHandle(*it, KisBezierPatch::TR);
            tryAddHandle(*it, KisBezierPatch::TR_HC);
            tryAddHandle(*it, KisBezierPatch::TR_VC);

            tryAddHandle(*it, KisBezierPatch::BL);
            tryAddHandle(*it, KisBezierPatch::BL_HC);
            tryAddHandle(*it, KisBezierPatch::BL_VC);

            tryAddHandle(*it, KisBezierPatch::BR);
            tryAddHandle(*it, KisBezierPatch::BR_HC);
            tryAddHandle(*it, KisBezierPatch::BR_VC);
        }
    }

    KisSampleRectIterator dstRectSampler{PkRectF(sampleRect)};
    KisBezierPatch patch = *beginPatches();
    KisBezierPatchParamToSourceSampler patchSampler(patch);

    /// the number of points that has actually been
    /// sampled from the destination rect
    int hitPoints = 0;

    while (1) {
        for (int i = 0; i < 10; i++) {
            const PkPointF dstPoint = *dstRectSampler++;

            if (patch.dstBoundingRect().contains(dstPoint)) {
                const PkPointF localPoint = patch.globalToLocal(dstPoint);
                if (unitRect.contains(localPoint)) {
                    KisAlgebra2D::accumulateBounds(patchSampler.point(localPoint), &stepRect);
                    hitPoints++;
                    continue;
                }
            }

            {
                PkPointF localPoint;
                auto it = hitTestPatchImpl(dstPoint, &localPoint);
                if (it != endPatches()) {
                    patch = *it;
                    patchSampler = KisBezierPatchParamToSourceSampler(patch);

                    KisAlgebra2D::accumulateBounds(patchSampler.point(localPoint), &stepRect);
                    hitPoints++;
                }
            }
        }

        PkRect alignedRect = stepRect.toAlignedRect();

        if (hitPoints > 20 && !alignedRect.isEmpty() && alignedRect == result) {
            break;
        }

        result = alignedRect;

        if (dstRectSampler.numSamples() > pkMin(2000, samplesLimit)) {
            /**
             * We don't warn if the "found" rect is empty, that is a perfectly
             * valid case.
             */
            if (!result.isEmpty()) {
                qWarning() << "KisBezierTransformMesh::approxNeedRect: the algorithm hasn't converged!"
                           << ppVar(hitPoints) << ppVar(stepRect) << ppVar(alignedRect) << ppVar(result);
            }
            break;
        }
    }

    return result;
}

PkRect KisBezierTransformMesh::approxChangeRect(const PkRect &rc) const
{
    PkRect result;

    const PkRect affectedPatches = hitTestPatchInSourceSpace(PkRectF(rc));

    for (int row = affectedPatches.top(); row <= affectedPatches.bottom(); row++) {
        for (int column = affectedPatches.left(); column <= affectedPatches.right(); column++) {
            const KisBezierPatch patch = *find(PatchIndex(column, row));
            const PkRectF srcRect = PkRectF(rc) & patch.srcBoundingRect();
            const PkRectF paramRect = calcTightSrcRectRangeInParamSpace(patch, srcRect, 0.1);

            KisSampleRectIterator paramRectSampler(paramRect);
            PkRect patchResultRect;
            PkRectF stepRect;

            while (1) {
                for (int i = 0; i < 10; i++) {
                    const PkPointF sampledParamPoint = *paramRectSampler++;
                    const PkPointF globalPoint = patch.localToGlobal(sampledParamPoint);
                    KisAlgebra2D::accumulateBounds(globalPoint, &stepRect);
                }

                const PkRect alignedRect = stepRect.toAlignedRect();

                if (!alignedRect.isEmpty() && alignedRect == patchResultRect) {
                    break;
                }

                patchResultRect = alignedRect;

                if (paramRectSampler.numSamples() > 2000) {
                    qWarning() << "KisBezierTransformMesh::approxChangeRect: the algorithm hasn't converged!"
                               << ppVar(result) << ppVar(patchResultRect) << ppVar(stepRect);
                    break;
                }
            }

            result |= patchResultRect;
        }
    }

    return result;
}


/**
 * Approximate the param-space rect that corresponds to \p srcSpaceRect in the source-space.
 * The resulting param-space rect will fully cover the source-space rect (and will be bigger).
 */
PkRectF KisBezierTransformMesh::calcTightSrcRectRangeInParamSpace(const KisBezierPatch &patch, const PkRectF &srcSpaceRect, qreal srcPrecision)
{
    using KisBezierUtils::Range;
    using KisBezierUtils::calcTightSrcRectRangeInParamSpace1D;

    KIS_ASSERT_RECOVER_NOOP(patch.srcBoundingRect().contains(srcSpaceRect));

    KisBezierPatchParamToSourceSampler sampler(patch);

    auto xSampler = [sampler] (qreal xParam) -> Range {
        return sampler.xRange(xParam);
    };

    auto ySampler = [sampler] (qreal yParam) -> Range {
        return sampler.yRange(yParam);
    };

    Range externalRangeX;
    Range internalRangeX;

    Range externalRangeY;
    Range internalRangeY;

    std::tie(externalRangeX, internalRangeX) =
        calcTightSrcRectRangeInParamSpace1D({0.0, 1.0},
                                            Range::fromRectX(patch.originalRect),
                                            Range::fromRectX(srcSpaceRect),
                                            xSampler, srcPrecision);

    std::tie(externalRangeY, internalRangeY) =
        calcTightSrcRectRangeInParamSpace1D({0.0, 1.0},
                                            Range::fromRectY(patch.originalRect),
                                            Range::fromRectY(srcSpaceRect),
                                            ySampler, srcPrecision);

    return Range::makeRectF(externalRangeX, externalRangeY);
}

#include <kis_dom_utils.h>

void KisBezierTransformMeshDetail::saveValue(PkXmlElement *parent, const PkString &tag, const KisBezierTransformMesh &mesh)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "transform-mesh");

    KisDomUtils::saveValue(&e, "size", mesh.m_size);
    KisDomUtils::saveValue(&e, "srcRect", mesh.m_originalRect);
    KisDomUtils::saveValue(&e, "columns", mesh.m_columns);
    KisDomUtils::saveValue(&e, "rows", mesh.m_rows);
    KisDomUtils::saveValue(&e, "nodes", mesh.m_nodes);
}

bool KisBezierTransformMeshDetail::loadValue(const PkXmlElement &e, KisBezierTransformMesh *mesh)
{
    if (!KisDomUtils::Private::checkType(e, "transform-mesh")) return false;

    mesh->m_columns.clear();
    mesh->m_rows.clear();
    mesh->m_nodes.clear();

    KisDomUtils::loadValue(e, "size", &mesh->m_size);
    KisDomUtils::loadValue(e, "srcRect", &mesh->m_originalRect);
    KisDomUtils::loadValue(e, "columns", &mesh->m_columns);
    KisDomUtils::loadValue(e, "rows", &mesh->m_rows);
    KisDomUtils::loadValue(e, "nodes", &mesh->m_nodes);

    return true;
}
