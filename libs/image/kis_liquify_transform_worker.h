/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LIQUIFY_TRANSFORM_WORKER_H
#define __KIS_LIQUIFY_TRANSFORM_WORKER_H

#include <PkScopedPointer.h>
#include <PkRect.h>
#include <PkPoint.h>
#include <PkSize.h>
#include <PkTransform.h>
#include <PkVector.h>
#include <PkImage.h>
#include <PkXmlElement.h>
#include <boost/operators.hpp>

#include <kritaimage_export.h>
#include <kis_types.h>

class KRITAIMAGE_EXPORT KisLiquifyTransformWorker : boost::equality_comparable<KisLiquifyTransformWorker>
{
public:
    KisLiquifyTransformWorker(const PkRect &srcBounds,
                              KoUpdater *progress,
                              int pixelPrecision = 8);

    KisLiquifyTransformWorker(const KisLiquifyTransformWorker &rhs);

    ~KisLiquifyTransformWorker();

    bool operator==(const KisLiquifyTransformWorker &other) const;
    bool isIdentity() const;


    int pointToIndex(const PkPoint &cellPt);
    PkSize gridSize() const;

    void translatePoints(const PkPointF &base,
                         const PkPointF &offset,
                         qreal sigma,
                         bool useWashMode,
                         qreal flow);

    void scalePoints(const PkPointF &base,
                     qreal scale,
                     qreal sigma,
                     bool useWashMode,
                     qreal flow);

    void rotatePoints(const PkPointF &base,
                      qreal angle,
                      qreal sigma,
                      bool useWashMode,
                      qreal flow);

    void undoPoints(const PkPointF &base,
                    qreal amount,
                    qreal sigma);

    const PkVector<PkPointF>& originalPoints() const;
    PkVector<PkPointF>& transformedPoints();

    void run(KisPaintDeviceSP srcDevice, KisPaintDeviceSP dstDevice);
    PkImage runOnImage(const PkImage &srcImage,
                       const PkPointF &srcImageOffset,
                       const PkTransform &imageToThumbTransform,
                       PkPointF *newOffset);

    void toXML(PkXmlElement *e) const;
    static KisLiquifyTransformWorker* fromXML(const PkXmlElement &e);

    void translate(const PkPointF &offset);
    void translateDstSpace(const PkPointF &offset);

    PkRect approxChangeRect(const PkRect &rc);
    PkRect approxNeedRect(const PkRect &rc, const PkRect &fullBounds);
    PkRectF accumulatedStrokesBounds() const;

    void transformSrcAndDst(const PkTransform &t);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_LIQUIFY_TRANSFORM_WORKER_H */
