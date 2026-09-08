/*
 *  SPDX-FileCopyrightText: 2012 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_SMOOTHING_OPTIONS_H
#define KIS_SMOOTHING_OPTIONS_H

#include <PkGlobal.h>
#include <PkObject.h>
#include <PkScopedPointer.h>
#include <PkSharedPointer.h>
#include <PkSignalCompat.h>
#include <kritacanvas_export.h>


class KRITACANVAS_EXPORT KisSmoothingOptions : public PkObject
{
public:
    enum SmoothingType {
        NO_SMOOTHING = 0,
        SIMPLE_SMOOTHING,
        WEIGHTED_SMOOTHING,
        STABILIZER,
        PIXEL_PERFECT
    };

public:

    KisSmoothingOptions(bool useSavedSmoothing = true);
    ~KisSmoothingOptions() override;

    SmoothingType smoothingType() const;
    void setSmoothingType(SmoothingType value);

    qreal smoothnessDistanceMin() const;
    void setSmoothnessDistanceMin(qreal value);

    qreal smoothnessDistanceMax() const;
    void setSmoothnessDistanceMax(qreal value);

    bool smoothnessDistanceKeepAspectRatio() const;
    void setSmoothnessDistanceKeepAspectRatio(bool value);

    qreal tailAggressiveness() const;
    void setTailAggressiveness(qreal value);

    bool smoothPressure() const;
    void setSmoothPressure(bool value);

    bool useScalableDistance() const;
    void setUseScalableDistance(bool value);

    qreal delayDistance() const;
    void setDelayDistance(qreal value);

    void setUseDelayDistance(bool value);
    bool useDelayDistance() const;

    void setFinishStabilizedCurve(bool value);
    bool finishStabilizedCurve() const;

    void setStabilizeSensors(bool value);
    bool stabilizeSensors() const;

signals:
    void sigSmoothingTypeChanged();

private:
    void slotWriteConfig();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

typedef PkSharedPointer<KisSmoothingOptions> KisSmoothingOptionsSP;

#endif // KIS_SMOOTHING_OPTIONS_H
