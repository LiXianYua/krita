/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISBEZIERGRADIENTMESH_H
#define KISBEZIERGRADIENTMESH_H

#include "kritaimage_export.h"
#include <KisBezierMesh.h>

#include <PkColor.h>
#include <PkImage.h>
#include <PkXmlElement.h>

namespace KisBezierGradientMeshDetail {

inline PkColor lerp(const PkColor &c1, const PkColor &c2, qreal t) {
    using KisAlgebra2D::lerp;

    return PkColor::fromRgbF(lerp(c1.redF(), c2.redF(), t),
                             lerp(c1.greenF(), c2.greenF(), t),
                             lerp(c1.blueF(), c2.blueF(), t),
                             lerp(c1.alphaF(), c2.alphaF(), t));
}

struct GradientMeshPatch : public KisBezierPatch {
    std::array<PkColor, 4> colors;
};

struct GradientMeshNode : public KisBezierMeshDetails::BaseMeshNode, public boost::equality_comparable<GradientMeshNode>
{
    using BaseMeshNode::BaseMeshNode;
    PkColor color;

    bool operator==(const GradientMeshNode &rhs) const {
        return static_cast<const KisBezierMeshDetails::BaseMeshNode&>(*this) ==
                static_cast<const KisBezierMeshDetails::BaseMeshNode&>(rhs) &&
                color == rhs.color;
    }
};

inline void lerpNodeData(const GradientMeshNode &left, const GradientMeshNode &right, qreal t, GradientMeshNode &dst)
{
    dst.color = lerp(left.color, right.color, t);
}

inline void assignPatchData(GradientMeshPatch *patch,
                     const PkRectF &srcRect,
                     const GradientMeshNode &tl,
                     const GradientMeshNode &tr,
                     const GradientMeshNode &bl,
                     const GradientMeshNode &br)
{
    Q_UNUSED(srcRect);

    patch->originalRect = PkRectF(0.0, 0.0, 1.0, 1.0);
    patch->colors[0] = tl.color;
    patch->colors[1] = tr.color;
    patch->colors[2] = bl.color;
    patch->colors[3] = br.color;
}

class KRITAIMAGE_EXPORT KisBezierGradientMesh : public KisBezierMeshBase<GradientMeshNode, GradientMeshPatch>
{
public:

    PatchIndex hitTestPatch(const PkPointF &pt, PkPointF *localPointResult) const;

    static void renderPatch(const GradientMeshPatch &patch,
                     const PkPoint &dstImageOffset,
                     PkImage *dstImage);

    void renderMesh(const PkPoint &dstImageOffset,
                    PkImage *dstImage) const;

    friend KRITAIMAGE_EXPORT void saveValue(PkXmlElement *parent, const PkString &tag, const KisBezierGradientMesh &mesh);
    friend KRITAIMAGE_EXPORT bool loadValue(const PkXmlElement &parent, const PkString &tag, KisBezierGradientMesh *mesh);
};

KRITAIMAGE_EXPORT
void saveValue(PkXmlElement *parent, const PkString &tag, const GradientMeshNode &node);

KRITAIMAGE_EXPORT
bool loadValue(const PkXmlElement &parent, GradientMeshNode *node);

KRITAIMAGE_EXPORT
void saveValue(PkXmlElement *parent, const PkString &tag, const KisBezierGradientMesh &mesh);

KRITAIMAGE_EXPORT
bool loadValue(const PkXmlElement &parent, const PkString &tag, KisBezierGradientMesh *mesh);
}

namespace KisDomUtils {
using KisBezierGradientMeshDetail::loadValue;
using KisBezierGradientMeshDetail::saveValue;
}

using KisBezierGradientMesh = KisBezierGradientMeshDetail::KisBezierGradientMesh;

#endif // KISBEZIERGRADIENTMESH_H
