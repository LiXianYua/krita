/*
 *  kis_warptransform_worker.h - part of Krita
 *
 *  SPDX-FileCopyrightText: 2010 Marc Pegon <pe.marc@free.fr>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_WARPTRANSFORM_WORKER_H
#define KIS_WARPTRANSFORM_WORKER_H

#include "kis_types.h"
#include "kritaimage_export.h"

#include <PkImage.h>
#include <PkObject.h>
#include <PkPoint.h>
#include <PkPolygon.h>
#include <PkRect.h>
#include <PkVector.h>

#include <KoUpdater.h>

/**
 * Class to apply a transformation (affine, similitude, MLS) to a paintDevice
 * or a PkImage according an original set of points p, a new set of points q,
 * and the constant alpha.
 * The algorithms are based a paper entitled "Image Deformation Using
 * Moving Least Squares", by Scott Schaefer (Texas A&M University), Travis
 * McPhail (Rice University) and Joe Warren (Rice University)
 */

class KRITAIMAGE_EXPORT KisWarpTransformWorker : public PkObject
{
public:
    typedef enum WarpType_ {AFFINE_TRANSFORM = 0, SIMILITUDE_TRANSFORM, RIGID_TRANSFORM, N_MODES} WarpType;
    typedef enum WarpCalculation_ {GRID = 0, DRAW} WarpCalculation;

    static PkPointF affineTransformMath(PkPointF v, PkVector<PkPointF> p, PkVector<PkPointF> q, qreal alpha);
    static PkPointF similitudeTransformMath(PkPointF v, PkVector<PkPointF> p, PkVector<PkPointF> q, qreal alpha);
    static PkPointF rigidTransformMath(PkPointF v, PkVector<PkPointF> p, PkVector<PkPointF> q, qreal alpha);

    static PkImage transformImage(WarpType warpType,
                                  const PkVector<PkPointF> &origPoint,
                                  const PkVector<PkPointF> &transfPoint,
                                  qreal alpha,
                                  const PkImage& srcImage,
                                  const PkPointF &srcImageOffset,
                                  PkPointF *newOffset);

    // Prepare the transformation on dev
    KisWarpTransformWorker(WarpType warpType, PkVector<PkPointF> origPoint, PkVector<PkPointF> transfPoint, qreal alpha, KoUpdater *progress);
    ~KisWarpTransformWorker() override;
    // Perform the prepared transformation
    void run(KisPaintDeviceSP srcDev, KisPaintDeviceSP dstDev);

    PkRect approxChangeRect(const PkRect &rc);
    PkRect approxNeedRect(const PkRect &rc, const PkRect &fullBounds);

private:
    struct FunctionTransformOp;
    typedef PkPointF (*WarpMathFunction)(PkPointF, PkVector<PkPointF>, PkVector<PkPointF>, qreal);

private:
    WarpMathFunction m_warpMathFunction;
    WarpCalculation m_warpCalc {GRID};
    PkVector<PkPointF> m_origPoint;
    PkVector<PkPointF> m_transfPoint;
    qreal m_alpha {1.0};
    KoUpdater *m_progress {0};
};

#endif
