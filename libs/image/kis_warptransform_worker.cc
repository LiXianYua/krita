/*
 *  kis_warptransform_worker.cc -- part of Krita
 *
 *  SPDX-FileCopyrightText: 2010 Marc Pegon <pe.marc@free.fr>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_warptransform_worker.h"
#include "kis_random_sub_accessor.h"
#include "kis_iterator_ng.h"
#include "kis_datamanager.h"

#include <PkVectorND.h>
#include <PkVector.h>

#include <KoColorSpace.h>
#include <KoColor.h>

#include <math.h>

#include "kis_grid_interpolation_tools.h"

PkPointF KisWarpTransformWorker::affineTransformMath(PkPointF v, PkVector<PkPointF> p, PkVector<PkPointF> q, qreal alpha)
{
    int nbPoints = p.size();
    PkVector<qreal> w(nbPoints);
    qreal sumWi = 0;
    PkPointF pStar(0, 0), qStar(0, 0);
    PkVector<PkPointF> pHat(nbPoints), qHat(nbPoints);

    for (int i = 0; i < nbPoints; ++i) {
        if (v == p[i])
            return q[i];

        PkVector2D tmp(p[i] - v);
        w[i] = 1. / pow(tmp.lengthSquared(), alpha);
        pStar += w[i] * p[i];
        qStar += w[i] * q[i];
        sumWi += w[i];
    }
    pStar /= sumWi;
    qStar /= sumWi;

    qreal A_tmp[4] = {0, 0, 0, 0};
    for (int i = 0; i < nbPoints; ++i) {
        pHat[i] = p[i] - pStar;
        qHat[i] = q[i] - qStar;

        A_tmp[0] += w[i] * pow(pHat[i].x(), 2);
        A_tmp[3] += w[i] * pow(pHat[i].y(), 2);
        A_tmp[1] += w[i] * pHat[i].x() * pHat[i].y();
    }
    A_tmp[2] = A_tmp[1];
    qreal det_A_tmp = A_tmp[0] * A_tmp[3] - A_tmp[1] * A_tmp[2];

    qreal A_tmp_inv[4];

    if (det_A_tmp == 0)
        return v;

    A_tmp_inv[0] = A_tmp[3] / det_A_tmp;
    A_tmp_inv[1] = - A_tmp[1] / det_A_tmp;
    A_tmp_inv[2] = A_tmp_inv[1];
    A_tmp_inv[3] = A_tmp[0] / det_A_tmp;

    PkPointF t = v - pStar;
    PkPointF A_precalc(t.x() * A_tmp_inv[0] + t.y() * A_tmp_inv[1], t.x() * A_tmp_inv[2] + t.y() * A_tmp_inv[3]);
    qreal A_j;

    PkPointF res = qStar;
    for (int j = 0; j < nbPoints; ++j) {
        A_j = A_precalc.x() * pHat[j].x() + A_precalc.y() * pHat[j].y();

        res += w[j] * A_j * qHat[j];
    }

    return res;
}

PkPointF KisWarpTransformWorker::similitudeTransformMath(PkPointF v, PkVector<PkPointF> p, PkVector<PkPointF> q, qreal alpha)
{
    int nbPoints = p.size();
    PkVector<qreal> w(nbPoints);
    qreal sumWi = 0;
    PkPointF pStar(0, 0), qStar(0, 0);
    PkVector<PkPointF> pHat(nbPoints), qHat(nbPoints);

    for (int i = 0; i < nbPoints; ++i) {
        if (v == p[i])
            return q[i];

        PkVector2D tmp(p[i] - v);
        w[i] = 1. / pow(tmp.lengthSquared(), alpha);
        pStar += w[i] * p[i];
        qStar += w[i] * q[i];
        sumWi += w[i];
    }
    pStar /= sumWi;
    qStar /= sumWi;

    qreal mu_s = 0;
    PkPointF res_tmp(0, 0);
    qreal qx, qy, px, py;
    for (int i = 0; i < nbPoints; ++i) {
        pHat[i] = p[i] - pStar;
        qHat[i] = q[i] - qStar;

        PkVector2D tmp(pHat[i]);
        mu_s += w[i] * tmp.lengthSquared();

        qx = w[i] * qHat[i].x();
        qy = w[i] * qHat[i].y();
        px = pHat[i].x();
        py = pHat[i].y();

        res_tmp += PkPointF(qx * px + qy * py, qx * py - qy * px);
    }

    res_tmp /= mu_s;
    PkPointF v_m_pStar(v - pStar);
    PkPointF res(res_tmp.x() * v_m_pStar.x() + res_tmp.y() * v_m_pStar.y(), res_tmp.x() * v_m_pStar.y() - res_tmp.y() * v_m_pStar.x());
    res += qStar;

    return res;
}

PkPointF KisWarpTransformWorker::rigidTransformMath(PkPointF v, PkVector<PkPointF> p, PkVector<PkPointF> q, qreal alpha)
{
    int nbPoints = p.size();
    PkVector<qreal> w(nbPoints);
    qreal sumWi = 0;
    PkPointF pStar(0, 0), qStar(0, 0);
    PkVector<PkPointF> pHat(nbPoints), qHat(nbPoints);

    for (int i = 0; i < nbPoints; ++i) {
        if (v == p[i])
            return q[i];

        PkVector2D tmp(p[i] - v);
        w[i] = 1. / pow(tmp.lengthSquared(), alpha);
        pStar += w[i] * p[i];
        qStar += w[i] * q[i];
        sumWi += w[i];
    }
    pStar /= sumWi;
    qStar /= sumWi;

    PkVector2D res_tmp(0, 0);
    qreal qx, qy, px, py;
    for (int i = 0; i < nbPoints; ++i) {
        pHat[i] = p[i] - pStar;
        qHat[i] = q[i] - qStar;

        qx = w[i] * qHat[i].x();
        qy = w[i] * qHat[i].y();
        px = pHat[i].x();
        py = pHat[i].y();

        res_tmp += PkVector2D(qx * px + qy * py, qx * py - qy * px);
    }

    PkPointF f_arrow(res_tmp.normalized().toPointF());
    PkVector2D v_m_pStar(v - pStar);
    PkPointF res(f_arrow.x() * v_m_pStar.x() + f_arrow.y() * v_m_pStar.y(), f_arrow.x() * v_m_pStar.y() - f_arrow.y() * v_m_pStar.x());
    res += qStar;

    return res;
}

KisWarpTransformWorker::KisWarpTransformWorker(WarpType warpType, PkVector<PkPointF> origPoint, PkVector<PkPointF> transfPoint, qreal alpha, KoUpdater *progress)
        : m_progress(progress)
{
    m_origPoint = origPoint;
    m_transfPoint = transfPoint;
    m_alpha = alpha;

    switch(warpType) {
    case AFFINE_TRANSFORM:
        m_warpMathFunction = &affineTransformMath;
        break;
    case SIMILITUDE_TRANSFORM:
        m_warpMathFunction = &similitudeTransformMath;
        break;
    case RIGID_TRANSFORM:
        m_warpMathFunction = &rigidTransformMath;
        break;
    default:
        m_warpMathFunction = 0;
        break;
    }
}

KisWarpTransformWorker::~KisWarpTransformWorker()
{
}

struct KisWarpTransformWorker::FunctionTransformOp
{
    FunctionTransformOp(KisWarpTransformWorker::WarpMathFunction function,
                        const PkVector<PkPointF> &p,
                        const PkVector<PkPointF> &q,
                        qreal alpha)
        : m_function(function),
          m_p(p),
          m_q(q),
          m_alpha(alpha)
    {
    }

    PkPointF operator() (const PkPointF &pt) const {
        return m_function(pt, m_p, m_q, m_alpha);
    }

    KisWarpTransformWorker::WarpMathFunction m_function;
    const PkVector<PkPointF> &m_p;
    const PkVector<PkPointF> &m_q;
    qreal m_alpha;
};

void KisWarpTransformWorker::run(KisPaintDeviceSP srcDev, KisPaintDeviceSP dstDev)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(*srcDev->colorSpace() == *dstDev->colorSpace());

    if (!m_warpMathFunction ||
        m_origPoint.isEmpty() ||
        m_origPoint.size() != m_transfPoint.size()) {

        return;
    }

    if (m_origPoint.size() == 1) {
        dstDev->makeCloneFromRough(srcDev, srcDev->extent());
        PkPointF translate(PkPointF(srcDev->x(), srcDev->y()) + m_transfPoint[0] - m_origPoint[0]);
        dstDev->moveTo(translate.toPoint());
        return;
    }

    const PkRect srcBounds = srcDev->region().boundingRect();

    dstDev->clear();

    const int pixelPrecision = 8;

    FunctionTransformOp functionOp(m_warpMathFunction, m_origPoint, m_transfPoint, m_alpha);
    GridIterationTools::PaintDevicePolygonOp polygonOp(srcDev, dstDev);
    /**
     * The lazy rects copying is currently explicitly  disabled for Warp transform.
     * To activate it we need:
     *
     * 1) A proper detection with GridIterationTools::canProcessRectsInRandomOrder()
     * 2) A unittest to test this mode
     */
    polygonOp.setCanMergeRects(false);
    GridIterationTools::processGrid(polygonOp, functionOp,
                                    srcBounds, pixelPrecision);
    polygonOp.finalize();
}

#include "krita_utils.h"

PkRect KisWarpTransformWorker::approxChangeRect(const PkRect &rc)
{
    const qreal margin = 0.05;

    FunctionTransformOp functionOp(m_warpMathFunction, m_origPoint, m_transfPoint, m_alpha);
    PkRect resultRect = KisAlgebra2D::approximateRectWithPointTransform(rc, functionOp);

    return KisAlgebra2D::blowRect(resultRect, margin);
}

PkRect KisWarpTransformWorker::approxNeedRect(const PkRect &rc, const PkRect &fullBounds)
{
    Q_UNUSED(rc);
    return fullBounds;
}

PkImage KisWarpTransformWorker::transformImage(WarpType warpType,
                                               const PkVector<PkPointF> &origPoint,
                                               const PkVector<PkPointF> &transfPoint,
                                               qreal alpha,
                                               const PkImage& srcImage,
                                               const PkPointF &srcImageOffset,
                                               PkPointF *newOffset)
{
    KIS_ASSERT_RECOVER(srcImage.format() == PkImage::Format_ARGB32) {
        return PkImage();
    }

    WarpMathFunction warpMathFunction = &rigidTransformMath;

    switch (warpType) {
    case AFFINE_TRANSFORM:
        warpMathFunction = &affineTransformMath;
        break;
    case SIMILITUDE_TRANSFORM:
        warpMathFunction = &similitudeTransformMath;
        break;
    case RIGID_TRANSFORM:
        warpMathFunction = &rigidTransformMath;
        break;
    default:
        KIS_ASSERT_RECOVER(0 && "Unknown warp mode") { return PkImage(); }
    }

    if (!warpMathFunction ||
        origPoint.isEmpty() ||
        origPoint.size() != transfPoint.size()) {

        return srcImage;
    }

    if (origPoint.size() == 1) {
        *newOffset = srcImageOffset + (transfPoint[0] - origPoint[0]).toPoint();
        return srcImage;
    }

    FunctionTransformOp functionOp(warpMathFunction, origPoint, transfPoint, alpha);

    const PkRectF srcBounds = PkRectF(srcImageOffset, srcImage.size());
    PkRectF dstBounds;

    {
        PkPolygonF testPoints;
        testPoints << srcBounds.topLeft();
        testPoints << srcBounds.topRight();
        testPoints << srcBounds.bottomRight();
        testPoints << srcBounds.bottomLeft();
        testPoints << srcBounds.topLeft();

        PkPolygonF::iterator it = testPoints.begin() + 1;

        while (it != testPoints.end()) {
            it = testPoints.insert(it, 0.5 * (*it + *(it - 1)));
            it += 2;
        }

        it = testPoints.begin();

        while (it != testPoints.end()) {
            *it = functionOp(*it);
            ++it;
        }

        dstBounds = testPoints.boundingRect();
    }

    PkPointF dstImageOffset = dstBounds.topLeft();
    *newOffset = dstImageOffset;

    PkRect dstBoundsI = dstBounds.toAlignedRect();
    PkImage dstImage(dstBoundsI.size(), srcImage.format());
    dstImage.fill(0);

    const int pixelPrecision = 32;
    GridIterationTools::PkImagePolygonOp polygonOp(srcImage, dstImage, srcImageOffset, dstImageOffset);
    /**
     * The lazy rects copying is currently explicitly  disabled for Warp transform.
     * To activate it we need:
     *
     * 1) A proper detection with GridIterationTools::canProcessRectsInRandomOrder()
     * 2) A unittest to test this mode
     */
    polygonOp.setCanMergeRects(false);
    GridIterationTools::processGrid(polygonOp, functionOp, srcBounds.toAlignedRect(), pixelPrecision);
    polygonOp.finalize();

    return dstImage;
}
