/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISBEZIERTRANSFORMMESH_H
#define KISBEZIERTRANSFORMMESH_H

#include "kritaimage_export.h"
#include "KisBezierMesh.h"
#include <PkImage.h>
#include <PkXmlElement.h>
#include <PkString.h>

#include "kis_types.h"

namespace KisBezierTransformMeshDetail {

class KRITAIMAGE_EXPORT KisBezierTransformMesh : public KisBezierMesh
{
public:
    KisBezierTransformMesh()
    {
    }
    KisBezierTransformMesh(const PkRectF &srcRect, const PkSize &size = PkSize(2,2))
        : KisBezierMesh(srcRect, size)
    {
    }

    PatchIndex hitTestPatch(const PkPointF &pt, PkPointF *localPointResult = 0) const;

    static void transformPatch(const KisBezierPatch &patch,
                               const PkPoint &srcImageOffset,
                               const PkImage &srcImage,
                               const PkPoint &dstImageOffset,
                               PkImage *dstImage);

    static void transformPatch(const KisBezierPatch &patch,
                               KisPaintDeviceSP srcDevice,
                               KisPaintDeviceSP dstDevice);


    void transformMesh(const PkPoint &srcImageOffset,
                       const PkImage &srcImage,
                       const PkPoint &dstImageOffset,
                       PkImage *dstImage) const;

    void transformMesh(KisPaintDeviceSP srcDevice,
                       KisPaintDeviceSP dstDevice) const;

    PkRect approxNeedRect(const PkRect &rc) const;
    PkRect approxChangeRect(const PkRect &rc) const;

    static PkRectF calcTightSrcRectRangeInParamSpace(const KisBezierPatch &patch,
                                                    const PkRectF &srcSpaceRect,
                                                    qreal srcPrecision);

    friend KRITAIMAGE_EXPORT void saveValue(PkXmlElement *parent, const PkString &tag, const KisBezierTransformMesh &mesh);
    friend KRITAIMAGE_EXPORT bool loadValue(const PkXmlElement &parent, KisBezierTransformMesh *mesh);

    PkRect hitTestPatchInSourceSpace(const PkRectF &rect) const;

private:
    patch_const_iterator hitTestPatchImpl(const PkPointF &pt, PkPointF *localPointResult = 0) const;
};

KRITAIMAGE_EXPORT
void saveValue(PkXmlElement *parent, const PkString &tag, const KisBezierTransformMesh &mesh);

KRITAIMAGE_EXPORT
bool loadValue(const PkXmlElement &parent, KisBezierTransformMesh *mesh);

}

namespace KisDomUtils {
using KisBezierTransformMeshDetail::loadValue;
using KisBezierTransformMeshDetail::saveValue;
}

using KisBezierTransformMesh = KisBezierTransformMeshDetail::KisBezierTransformMesh;

#endif // KISBEZIERTRANSFORMMESH_H
