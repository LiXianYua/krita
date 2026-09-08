/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_CAGE_TRANSFORM_WORKER_H
#define __KIS_CAGE_TRANSFORM_WORKER_H

#include <PkScopedPointer.h>
#include <PkPoint.h>
#include <PkPolygon.h>
#include <PkRect.h>
#include <PkVector.h>
#include <kritaimage_export.h>
#include <kis_types.h>

#include <PkImage.h>

class KRITAIMAGE_EXPORT KisCageTransformWorker
{
public:
    KisCageTransformWorker(const PkRect &deviceNonDefaultRegion,
                           const PkVector<PkPointF> &origCage,
                           KoUpdater *progress,
                           int pixelPrecision = 8);

    KisCageTransformWorker(const PkImage &srcImage,
                           const PkPointF &srcImageOffset,
                           const PkVector<PkPointF> &origCage,
                           KoUpdater *progress,
                           int pixelPrecision = 8);

    ~KisCageTransformWorker();

    void prepareTransform();
    void setTransformedCage(const PkVector<PkPointF> &transformedCage);
    void run(KisPaintDeviceSP srcDevice, KisPaintDeviceSP dstDevice);

    PkRect approxChangeRect(const PkRect &rc);
    PkRect approxNeedRect(const PkRect &rc, const PkRect &fullBounds);

    PkImage runOnImage(PkPointF *newOffset);

private:
    friend class KisCageTransformWorkerTest;
    static void compositeImages(PkImage *destination,
                                const PkImage &source,
                                const PkPointF &sourceOffset,
                                const PkPointF &destinationOffset,
                                const PkPolygonF &originalCage,
                                const PkImage &transformedImage);

    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_CAGE_TRANSFORM_WORKER_H */
