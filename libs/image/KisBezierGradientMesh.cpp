/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkGlobal.h>
#include "KisBezierGradientMesh.h"

#include "kis_grid_interpolation_tools.h"
#include "kis_debug.h"
#include "kis_dom_utils.h"

namespace KisBezierGradientMeshDetail {

struct PkImageGradientOp
{
    PkImageGradientOp(const std::array<PkColor, 4> &colors, PkImage &dstImage,
                      const PkPointF &dstImageOffset)
        : m_colors(colors), m_dstImage(dstImage),
          m_dstImageOffset(dstImageOffset),
          m_dstImageRect(m_dstImage.rect())
    {
    }

    void operator() (const PkPolygonF &srcPolygon, const PkPolygonF &dstPolygon) {
        this->operator() (srcPolygon, dstPolygon, dstPolygon);
    }

    void operator() (const PkPolygonF &srcPolygon, const PkPolygonF &dstPolygon, const PkPolygonF &clipDstPolygon) {
        PkRect boundRect = clipDstPolygon.boundingRect().toAlignedRect();
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

                    PkPoint srcPointI = srcPoint.toPoint();

                    if (!m_dstImageRect.contains(srcPointI)) continue;

                    // TODO: move vertical calculation into the upper loop
                    const PkColor c1 = lerp(m_colors[0], m_colors[1], pkBound(0.0, dstPoint.x(), 1.0));
                    const PkColor c2 = lerp(m_colors[2], m_colors[3], pkBound(0.0, dstPoint.x(), 1.0));

                    m_dstImage.setPixelColor(srcPointI.x(), srcPointI.y(),
                                             lerp(c1, c2, pkBound(0.0, dstPoint.y(), 1.0)).rgba());
                }
            }
        }
    }

    void finalize() {}

    const std::array<PkColor, 4> &m_colors;
    PkImage &m_dstImage;
    PkPointF m_dstImageOffset;
    PkRect m_dstImageRect;
};

void saveValue(PkXmlElement *parent, const PkString &tag, const GradientMeshNode &node)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "gradient-mesh-node");
    KisDomUtils::saveValue(&e, "color", node.color);
    KisDomUtils::saveValue(&e, "node", node.node);
    KisDomUtils::saveValue(&e, "left-control", node.leftControl);
    KisDomUtils::saveValue(&e, "right-control", node.rightControl);
    KisDomUtils::saveValue(&e, "top-control", node.topControl);
    KisDomUtils::saveValue(&e, "bottom-control", node.bottomControl);

}

bool loadValue(const PkXmlElement &parent, GradientMeshNode *node)
{
    if (!KisDomUtils::Private::checkType(parent, "gradient-mesh-node")) return false;

    KisDomUtils::loadValue(parent, "node", &node->node);
    KisDomUtils::loadValue(parent, "left-control", &node->leftControl);
    KisDomUtils::loadValue(parent, "right-control", &node->rightControl);
    KisDomUtils::loadValue(parent, "top-control", &node->topControl);
    KisDomUtils::loadValue(parent, "bottom-control", &node->bottomControl);

    return true;
}

void saveValue(PkXmlElement *parent, const PkString &tag, const KisBezierGradientMesh &mesh)
{
    PkXmlDocument doc = parent->ownerDocument();
    PkXmlElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "gradient-mesh");

    KisDomUtils::saveValue(&e, "size", mesh.m_size);
    KisDomUtils::saveValue(&e, "srcRect", mesh.m_originalRect);
    KisDomUtils::saveValue(&e, "columns", mesh.m_columns);
    KisDomUtils::saveValue(&e, "rows", mesh.m_rows);
    KisDomUtils::saveValue(&e, "nodes", mesh.m_nodes);
}

bool loadValue(const PkXmlElement &parent, const PkString &tag, KisBezierGradientMesh *mesh)
{
    PkXmlElement e;
    if (!KisDomUtils::findOnlyElement(parent, tag, &e)) return false;

    if (!KisDomUtils::Private::checkType(e, "gradient-mesh")) return false;

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

}

KisBezierGradientMesh::PatchIndex KisBezierGradientMesh::hitTestPatch(const PkPointF &pt, PkPointF *localPointResult) const {
    auto result = endPatches();

    const PkRectF unitRect(0, 0, 1, 1);

    for (auto it = beginPatches(); it != endPatches(); ++it) {
        Patch patch = *it;

        if (patch.dstBoundingRect().contains(pt)) {
            const PkPointF localPos = KisBezierUtils::calculateLocalPosSVG2(patch.points, pt);

            if (unitRect.contains(localPos)) {

                if (localPointResult) {
                    *localPointResult = localPos;
                }

                result = it;
                break;
            }
        }
    }

    return result.patchIndex();
}

void KisBezierGradientMesh::renderPatch(const KisBezierGradientMeshDetail::GradientMeshPatch &patch,
                                        const PkPoint &dstImageOffset,
                                        PkImage *dstImage)
{
    PkVector<PkPointF> originalPointsLocal;
    PkVector<PkPointF> transformedPointsLocal;
    PkSize gridSize;

    patch.sampleRegularGridSVG2(gridSize, originalPointsLocal, transformedPointsLocal, PkPointF(8,8));

    const PkRect dstBoundsI = patch.dstBoundingRect().toAlignedRect();
    const PkRect imageSize = PkRect(dstImageOffset, dstImage->size());
    KIS_SAFE_ASSERT_RECOVER_NOOP(imageSize.contains(dstBoundsI));

    {
        PkImageGradientOp polygonOp(patch.colors, *dstImage, dstImageOffset);


        GridIterationTools::RegularGridIndexesOp indexesOp(gridSize);
        GridIterationTools::iterateThroughGrid
                <GridIterationTools::AlwaysCompletePolygonPolicy>(polygonOp, indexesOp,
                                                                  gridSize,
                                                                  originalPointsLocal,
                                                                  transformedPointsLocal);

    }
}

void KisBezierGradientMesh::renderMesh(const PkPoint &dstImageOffset,
                                       PkImage *dstImage) const
{
    for (auto it = beginPatches(); it != endPatches(); ++it) {
        renderPatch(*it, dstImageOffset, dstImage);
    }
}
