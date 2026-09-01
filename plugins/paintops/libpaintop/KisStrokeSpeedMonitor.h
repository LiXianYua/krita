/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISSTROKESPEEDMONITOR_H
#define KISSTROKESPEEDMONITOR_H

#include <PkObject.h>
#include <PkScopedPointer.h>
#include <PkSignalCompat.h>

#include "kis_types.h"
#include <kritapaintop_export.h>

class PAINTOP_EXPORT KisStrokeSpeedMonitor : public PkObject
{
public:
    KisStrokeSpeedMonitor();
    ~KisStrokeSpeedMonitor();

    static KisStrokeSpeedMonitor* instance();

    bool haveStrokeSpeedMeasurement() const;

    void notifyStrokeFinished(qreal cursorSpeed, qreal renderingSpeed, qreal fps, KisPaintOpPresetSP preset);


    PkString lastPresetName() const;
    qreal lastPresetSize() const;

    qreal lastCursorSpeed() const;
    qreal lastRenderingSpeed() const;
    qreal lastFps() const;
    bool lastStrokeSaturated() const;

    qreal avgCursorSpeed() const;
    qreal avgRenderingSpeed() const;
    qreal avgFps() const;


signals:
    void sigStatsUpdated();

public:
    void setHaveStrokeSpeedMeasurement(bool value);

private:
    void resetAccumulatedValues();

    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif // KISSTROKESPEEDMONITOR_H
