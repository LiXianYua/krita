/*
 *  SPDX-FileCopyrightText: 2016 Alvin Wong <alvinhochun-at-gmail-com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_STABILIZER_DELAYED_PAINT_HELPER_H
#define KIS_STABILIZER_DELAYED_PAINT_HELPER_H

#include <PkElapsedTimer.h>
#include <PkQueue.h>
#include <PkTimer.h>
#include <PkVector.h>

#include <functional>

#include "kis_paint_information.h"
#include "kritacanvas_export.h"

class KRITACANVAS_EXPORT KisStabilizerDelayedPaintHelper
{
    struct TimedPaintInfo
    {
        int elapsedTime;
        KisPaintInformation paintInfo;
        TimedPaintInfo(int elapsedTime, KisPaintInformation paintInfo);
    };

    PkTimer m_paintTimer;
    PkQueue<TimedPaintInfo> m_paintQueue;
    int m_lastPendingTime {0};
    int m_lastPaintTime {0};
    PkElapsedTimer m_elapsedTimer;

    // Callbacks
    std::function<void(const KisPaintInformation &, const KisPaintInformation &)> m_paintLine;
    std::function<void()> m_requestUpdateOutline;

public:
    KisStabilizerDelayedPaintHelper();
    ~KisStabilizerDelayedPaintHelper() = default;

    bool running() const {
        return m_paintTimer.isActive();
    }

    bool hasLastPaintInformation() const {
        return !m_paintQueue.isEmpty();
    }

    KisPaintInformation lastPaintInformation() const {
        // Please call hasLastPaintInformation before this
        return m_paintQueue.head().paintInfo;
    }

    void setPaintLineCallback(std::function<void(const KisPaintInformation &, const KisPaintInformation &)> paintLine) {
        m_paintLine = paintLine;
    }

    void setUpdateOutlineCallback(std::function<void()> requestUpdateOutline) {
        m_requestUpdateOutline = requestUpdateOutline;
    }

    void start(const KisPaintInformation &firstPaintInfo);
    void update(const PkVector<KisPaintInformation> &newPaintInfos);
    void paintSome();
    void end();
    void cancel();

private:
    void stabilizerDelayedPaintTimer();
};

#endif // KIS_STABILIZER_DELAYED_PAINT_HELPER_H
