/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISMASKEDPAINTINGSTROKEDATA_H
#define KISMASKEDPAINTINGSTROKEDATA_H

#include <kritapaintop_export.h>

#include <PkVector.h>
#include <PkSharedPointer.h>

class KisFreehandStrokeInfo;
class KisPaintInformation;
class KisDistanceInformation;
class PkPointF;
class PkRectF;
class PkRect;
class PkPainterPath;
class PkPen;
class KoColor;
class KisRunnableStrokeJobData;

class KisPaintOpPreset;
typedef PkSharedPointer<KisPaintOpPreset> KisPaintOpPresetSP;


class PAINTOP_EXPORT KisMaskedFreehandStrokePainter
{
public:
    KisMaskedFreehandStrokePainter(KisFreehandStrokeInfo *strokeData, KisFreehandStrokeInfo *maskData);

    // painter overrides

    KisPaintOpPresetSP preset() const;

    void paintAt(const KisPaintInformation& pi);

    void paintLine(const KisPaintInformation &pi1,
                   const KisPaintInformation &pi2);

    void paintBezierCurve(const KisPaintInformation &pi1,
                          const PkPointF &control1,
                          const PkPointF &control2,
                          const KisPaintInformation &pi2);

    void paintPolyline(const PkVector<PkPointF> &points,
                       int index = 0, int numPoints = -1);

    void paintPolygon(const PkVector<PkPointF> &points);
    void paintRect(const PkRectF &rect);
    void paintEllipse(const PkRectF &rect);
    void paintPainterPath(const PkPainterPath& path);

    void drawPainterPath(const PkPainterPath& path, const PkPen& pen);
    void drawAndFillPainterPath(const PkPainterPath& path, const PkPen& pen, const KoColor &customColor);

    // paintop overrides

    std::pair<int, bool> doAsynchronousUpdate(PkVector<KisRunnableStrokeJobData*> &jobs);
    bool hasDirtyRegion() const;
    PkVector<PkRect> takeDirtyRegion();

    bool hasMasking() const;

private:
    template <class Func>
    inline void applyToAllPainters(Func func);

private:
    KisFreehandStrokeInfo *m_stroke = 0;
    KisFreehandStrokeInfo *m_mask = 0;
};

#endif // KISMASKEDPAINTINGSTROKEDATA_H
