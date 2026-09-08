/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_PAINTING_INFORMATION_BUILDER_H
#define __KIS_PAINTING_INFORMATION_BUILDER_H

#include <PkObject.h>
#include <PkScopedPointer.h>
#include <pk/geometry/PkPoint.h>

#include <PkVector.h>

#include "kis_types.h"
#include "kritacanvas_export.h"
#include <brushengine/kis_paint_information.h>

class KoPointerEvent;
class KisToolFreehand;
class KisCoordinatesConverter;
class KisSpeedSmoother;
class KoCanvasResourceProvider;

class KRITACANVAS_EXPORT KisPaintingInformationBuilder : public PkObject
{
public:
    KisPaintingInformationBuilder();
    ~KisPaintingInformationBuilder() override;

    KisPaintInformation startStroke(KoPointerEvent *event, int timeElapsed, const KoCanvasResourceProvider *manager);

    KisPaintInformation continueStroke(KoPointerEvent *event,
                                       int timeElapsed);

    KisPaintInformation hover(const PkPointF &imagePoint,
                              const KoPointerEvent *event,
                              bool isStrokeStarted);

    qreal pressureToCurve(qreal pressure);

    void reset();

protected:
    void updateSettings();

    virtual PkPointF adjustDocumentPoint(const PkPointF &point, const PkPointF &startPoint);
    virtual PkPointF documentToImage(const PkPointF &point);
    virtual PkPointF imageToDocument(const PkPointF &point);
    virtual PkPointF imageToView(const PkPointF &point);
    virtual qreal calculatePerspective(const PkPointF &documentPoint);

    virtual qreal canvasRotation() const;
    virtual bool canvasMirroredX() const;
    virtual bool canvasMirroredY() const;

private:

    KisPaintInformation createPaintingInformation(KoPointerEvent *event,
                                                  int timeElapsed);

    /**
     * Defines how many discrete samples are stored in a precomputed array
     * of different pressures.
     */
    static const int LEVEL_OF_PRESSURE_RESOLUTION;

private:
    PkVector<qreal> m_pressureSamples;
    PkPointF m_startPoint;
    PkScopedPointer<KisSpeedSmoother> m_speedSmoother;
    bool m_pressureDisabled;
    int m_maxAllowedSpeedValue = 30;
    qreal m_tiltDirectionOffset = 0;  // [0, 360) degrees
};

class KRITACANVAS_EXPORT KisConverterPaintingInformationBuilder : public KisPaintingInformationBuilder
{
public:
    KisConverterPaintingInformationBuilder(const KisCoordinatesConverter *converter);

protected:
    PkPointF documentToImage(const PkPointF &point) override;
    PkPointF imageToDocument(const PkPointF &point) override;
    PkPointF imageToView(const PkPointF &point) override;

    qreal canvasRotation() const override;
    bool canvasMirroredX() const override;
    bool canvasMirroredY() const override;

private:
    const KisCoordinatesConverter *m_converter;
};

class KRITACANVAS_EXPORT KisToolFreehandPaintingInformationBuilder : public KisPaintingInformationBuilder
{
public:
    KisToolFreehandPaintingInformationBuilder(KisToolFreehand *tool);

protected:
    PkPointF documentToImage(const PkPointF &point) override;
    PkPointF imageToDocument(const PkPointF &point) override;
    PkPointF imageToView(const PkPointF &point) override;
    PkPointF adjustDocumentPoint(const PkPointF &point, const PkPointF &startPoint) override;
    qreal calculatePerspective(const PkPointF &documentPoint) override;

    qreal canvasRotation() const override;
    bool canvasMirroredX() const override;
    bool canvasMirroredY() const override;

private:
    KisToolFreehand *m_tool;
};

#endif /* __KIS_PAINTING_INFORMATION_BUILDER_H */
