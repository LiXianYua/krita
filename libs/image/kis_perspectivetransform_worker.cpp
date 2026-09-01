/*
 * This file is part of Krita
 *
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2009 Edward Apap <schumifer@hotmail.com>
 *  SPDX-FileCopyrightText: 2010 Marc Pegon <pe.marc@free.fr>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "kis_perspectivetransform_worker.h"

#include <PkMatrix4x4.h>
#include <PkPainterPath.h>
#include <PkPolygon.h>
#include <PkTransform.h>
#include <PkVectorND.h>

#include <KoUpdater.h>
#include <KoColor.h>
#include <KoCompositeOpRegistry.h>

#include "kis_paint_device.h"
#include "kis_perspective_math.h"
#include "kis_random_accessor_ng.h"
#include "kis_random_sub_accessor.h"
#include "kis_selection.h"
#include <kis_iterator_ng.h>
#include "krita_utils.h"
#include "kis_progress_update_helper.h"
#include "kis_painter.h"
#include "kis_image.h"
#include "kis_algebra_2d.h"

namespace {

PkPointF lineSegmentIntersection(const PkPointF &p1, const PkPointF &p2,
                                 const PkPointF &a, const PkPointF &b)
{
    const PkPointF d1 = p2 - p1;
    const PkPointF d2 = b - a;
    const qreal denominator = d1.x() * d2.y() - d1.y() * d2.x();
    if (qAbs(denominator) < 1e-12) return p1;

    const PkPointF difference = a - p1;
    const qreal t = (difference.x() * d2.y() - difference.y() * d2.x()) / denominator;
    return PkPointF(p1.x() + t * d1.x(), p1.y() + t * d1.y());
}

PkPolygonF intersectPolygon(const PkPolygonF &subject, const PkPolygonF &clip)
{
    if (subject.isEmpty() || clip.size() < 3) {
        return PkPolygonF();
    }

    PkPointF centroid;
    for (const PkPointF &point : clip) {
        centroid += point;
    }
    centroid *= 1.0 / clip.size();

    PkPolygonF input = subject;
    for (int i = 0; i < clip.size(); ++i) {
        if (input.isEmpty()) break;

        const PkPointF &edgeStart = clip[i];
        const PkPointF &edgeEnd = clip[(i + 1) % clip.size()];
        const PkPointF edgeVector = edgeEnd - edgeStart;
        const qreal centroidSide = edgeVector.x() * (centroid.y() - edgeStart.y())
            - edgeVector.y() * (centroid.x() - edgeStart.x());
        const bool insideIsNonNegative = centroidSide >= 0.0;

        PkPolygonF output;
        PkPointF previous = input[input.size() - 1];
        qreal previousSide = edgeVector.x() * (previous.y() - edgeStart.y())
            - edgeVector.y() * (previous.x() - edgeStart.x());
        bool previousInside = insideIsNonNegative ? previousSide >= 0.0 : previousSide <= 0.0;

        for (const PkPointF &current : input) {
            const qreal currentSide = edgeVector.x() * (current.y() - edgeStart.y())
                - edgeVector.y() * (current.x() - edgeStart.x());
            const bool currentInside = insideIsNonNegative ? currentSide >= 0.0 : currentSide <= 0.0;

            if (currentInside) {
                if (!previousInside) {
                    output << lineSegmentIntersection(previous, current, edgeStart, edgeEnd);
                }
                output << current;
            } else if (previousInside) {
                output << lineSegmentIntersection(previous, current, edgeStart, edgeEnd);
            }

            previous = current;
            previousInside = currentInside;
        }
        input = output;
    }

    return input;
}

} // namespace


KisPerspectiveTransformWorker::KisPerspectiveTransformWorker(KisPaintDeviceSP dev, PkPointF center, double aX, double aY, double distance, bool cropDst, KoUpdaterPtr progress)
        : m_dev(dev), m_progressUpdater(progress), m_cropDst(cropDst)

{
    PkMatrix4x4 m;
    m.rotate(180. * aX / M_PI, PkVector3D(1, 0, 0));
    m.rotate(180. * aY / M_PI, PkVector3D(0, 1, 0));

    PkTransform project = m.toTransform(distance);
    PkTransform t = PkTransform::fromTranslate(center.x(), center.y());

    PkTransform forwardTransform = t.inverted() * project * t;

    init(forwardTransform);
}

KisPerspectiveTransformWorker::KisPerspectiveTransformWorker(KisPaintDeviceSP dev, const PkTransform &transform, bool cropDst, KoUpdaterPtr progress)
    : m_dev(dev), m_progressUpdater(progress), m_cropDst(cropDst)
{
    init(transform);
}

void KisPerspectiveTransformWorker::fillParams(const PkRectF &srcRect,
                                               const PkRect &dstBaseClipRect,
                                               KisRegion *dstRegion,
                                               PkPolygonF *dstClipPolygon)
{
    PkPolygonF bounds = srcRect;
    PkPolygonF newBounds = m_forwardTransform.map(bounds);

    PkRectF clipRect = dstBaseClipRect;

    if (!m_cropDst) {
        clipRect |= srcRect;
        clipRect = KisAlgebra2D::blowRect(clipRect, 3.0);
    }

    newBounds = intersectPolygon(newBounds, PkPolygonF(clipRect));
    PkPainterPath path;
    path.addPolygon(newBounds);
    *dstRegion = KritaUtils::splitPath(path);
    *dstClipPolygon = newBounds;
}

void KisPerspectiveTransformWorker::init(const PkTransform &transform)
{
    m_isIdentity = transform.isIdentity();
    m_isTranslating = transform.type() == PkTransform::TxTranslate;

    m_forwardTransform = transform;
    m_backwardTransform = transform.inverted();

    if (m_dev) {
        m_srcRect = kisGrowRect(m_dev->exactBounds(), 1.0);

        PkPolygonF dstClipPolygonUnused;

        fillParams(m_srcRect,
                   m_dev->defaultBounds()->bounds(),
                   &m_dstRegion,
                   &dstClipPolygonUnused);
    }
}

KisPerspectiveTransformWorker::~KisPerspectiveTransformWorker()
{
}

void KisPerspectiveTransformWorker::setForwardTransform(const PkTransform &transform)
{
    init(transform);
}


struct BilinearWrapper
{
    using SrcAccessorSP = KisRandomSubAccessorSP;

    BilinearWrapper(KisPaintDeviceSP device)
        : m_accessor(device->createRandomSubAccessor())
    {
    }

    void samplePixel(const PkPointF &pt, quint8 *dst) {
        m_accessor->moveTo(pt.x(), pt.y());
        m_accessor->sampledOldRawData(dst);
    }

    KisRandomSubAccessorSP m_accessor;
};

struct NearestNeighbourWrapper
{
    using SrcAccessorSP = KisRandomAccessorSP;

    NearestNeighbourWrapper(KisPaintDeviceSP device)
        : m_accessor(device->createRandomConstAccessorNG()),
          m_pixelSize(device->pixelSize())
    {
    }

    void samplePixel(const PkPointF &pt, quint8 *dst) {
        m_accessor->moveTo(qRound(pt.x()), qRound(pt.y()));
        memcpy(dst, m_accessor->oldRawData(), m_pixelSize);
    }

    KisRandomConstAccessorSP m_accessor;
    int m_pixelSize;
};

template <class SrcAccessorWrapper>
void KisPerspectiveTransformWorker::runImpl()
{
    KIS_ASSERT_RECOVER_RETURN(m_dev);

    if (m_isIdentity) return;

    // TODO: check if this optimization is possible. The only blocking issue might be if
    //       some other thread also accesses this device (which should not be the case,
    //       theoretically
    //
    // if (m_isTranslating) {
    //     m_dev->moveTo(m_dev->offset() + QPoint(qRound(m_forwardTransform.dx()), qRound(m_forwardTransform.dy())));
    //     return;
    // }

    KisPaintDeviceSP cloneDevice = new KisPaintDevice(*m_dev.data());

    // Clear the destination device, since all the tiles are already
    // shared with cloneDevice
    m_dev->clear();

    KIS_ASSERT_RECOVER_NOOP(!m_isIdentity);

    KisProgressUpdateHelper progressHelper(m_progressUpdater, 100, m_dstRegion.rectCount());

    SrcAccessorWrapper srcAcc(cloneDevice);
    KisRandomAccessorSP accessor = m_dev->createRandomAccessorNG();

    for (const PkRect &rect : m_dstRegion.rects()) {
        for (int y = rect.y(); y < rect.y() + rect.height(); ++y) {
            for (int x = rect.x(); x < rect.x() + rect.width(); ++x) {

                PkPointF dstPoint(x, y);
                PkPointF srcPoint = m_backwardTransform.map(dstPoint);

                if (m_srcRect.contains(srcPoint)) {
                    accessor->moveTo(dstPoint.x(), dstPoint.y());
                    srcAcc.samplePixel(srcPoint, accessor->rawData());
                }
            }
        }
        progressHelper.step();
    }
}

void KisPerspectiveTransformWorker::run(SampleType sampleType)
{
    if (sampleType == Bilinear) {
        runImpl<BilinearWrapper>();
    } else {
        runImpl<NearestNeighbourWrapper>();
    }
}

void KisPerspectiveTransformWorker::runPartialDst(KisPaintDeviceSP srcDev,
                                                  KisPaintDeviceSP dstDev,
                                                  const PkRect &dstRect)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(srcDev->pixelSize() == dstDev->pixelSize());
    KIS_SAFE_ASSERT_RECOVER_NOOP(*srcDev->colorSpace() == *dstDev->colorSpace());

    PkRectF srcClipRect = kisGrowRect(srcDev->exactBounds(), 1) | srcDev->defaultBounds()->imageBorderRect();
    if (srcClipRect.isEmpty()) return;

    if (m_isIdentity || (m_isTranslating && !m_forceSubPixelTranslation)) {
        KisPainter gc(dstDev);
        gc.setCompositeOpId(COMPOSITE_COPY);
        gc.bitBlt(dstRect.topLeft(), srcDev, m_backwardTransform.mapRect(dstRect));
    } else {
        KisProgressUpdateHelper progressHelper(m_progressUpdater, 100, dstRect.height());

        KisRandomSubAccessorSP srcAcc = srcDev->createRandomSubAccessor();
        KisRandomAccessorSP accessor = dstDev->createRandomAccessorNG();

        for (int y = dstRect.y(); y < dstRect.y() + dstRect.height(); ++y) {
            for (int x = dstRect.x(); x < dstRect.x() + dstRect.width(); ++x) {

                PkPointF dstPoint(x, y);
                PkPointF srcPoint = m_backwardTransform.map(dstPoint);

                if (srcClipRect.contains(srcPoint) || srcDev->defaultBounds()->wrapAroundMode()) {
                    accessor->moveTo(dstPoint.x(), dstPoint.y());
                    srcAcc->moveTo(srcPoint.x(), srcPoint.y());
                    srcAcc->sampledOldRawData(accessor->rawData());
                }
            }
            progressHelper.step();
        }
    }
}

PkTransform KisPerspectiveTransformWorker::forwardTransform() const
{
    return m_forwardTransform;
}

PkTransform KisPerspectiveTransformWorker::backwardTransform() const
{
    return m_backwardTransform;
}

bool KisPerspectiveTransformWorker::forceSubPixelTranslation() const
{
    return m_forceSubPixelTranslation;
}

void KisPerspectiveTransformWorker::setForceSubPixelTranslation(bool value)
{
    m_forceSubPixelTranslation = value;
}
