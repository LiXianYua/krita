/*
 *  SPDX-FileCopyrightText: 2006 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2010 Marc Pegon <pe.marc@free.fr>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_PERSPECTIVETRANSFORM_WORKER_H
#define KIS_PERSPECTIVETRANSFORM_WORKER_H

#include "kis_types.h"
#include "kritaimage_export.h"

#include <PkPolygon.h>
#include <PkRect.h>
#include <PkTransform.h>
#include <KisRegion.h>
#include <KoUpdater.h>


class KRITAIMAGE_EXPORT KisPerspectiveTransformWorker
{
public:
    KisPerspectiveTransformWorker(KisPaintDeviceSP dev, PkPointF center, double aX, double aY, double distance, bool cropDst, KoUpdaterPtr progress);
    KisPerspectiveTransformWorker(KisPaintDeviceSP dev, const PkTransform &transform, bool cropDst, KoUpdaterPtr progress);

    ~KisPerspectiveTransformWorker();

    enum SampleType {
        NearestNeighbour = 0,
        Bilinear
    };

    void run(SampleType sampleType = Bilinear);
    void runPartialDst(KisPaintDeviceSP srcDev,
                       KisPaintDeviceSP dstDev,
                       const PkRect &dstRect);

    void setForwardTransform(const PkTransform &transform);

    PkTransform forwardTransform() const;
    PkTransform backwardTransform() const;

    bool forceSubPixelTranslation() const;
    void setForceSubPixelTranslation(bool value);

private:
    void init(const PkTransform &transform);

    void fillParams(const PkRectF &srcRect,
                    const PkRect &dstBaseClipRect,
                    KisRegion *dstRegion,
                    PkPolygonF *dstClipPolygon);

    template <class SrcAccessorPolicy>
    void runImpl();

private:
    KisPaintDeviceSP m_dev;
    KoUpdaterPtr m_progressUpdater;
    KisRegion m_dstRegion;
    PkRectF m_srcRect;
    PkTransform m_backwardTransform;
    PkTransform m_forwardTransform;
    bool m_isIdentity;
    bool m_isTranslating;
    bool m_cropDst;
    bool m_forceSubPixelTranslation {false};
};

#endif
