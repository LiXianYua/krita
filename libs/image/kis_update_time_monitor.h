/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_UPDATE_TIME_MONITOR_H
#define __KIS_UPDATE_TIME_MONITOR_H

#include "kritaimage_export.h"
#include "kis_types.h"


#include <PkVector.h>
class PkPointF;
class PkRect;


class KRITAIMAGE_EXPORT KisUpdateTimeMonitor
{
public:
    KisUpdateTimeMonitor();
    ~KisUpdateTimeMonitor();
    static KisUpdateTimeMonitor* instance();

    void startStrokeMeasure();
    void endStrokeMeasure();
    void reportPaintOpPreset(KisPaintOpPresetSP preset);

    void reportMouseMove(const PkPointF &pos);
    void printValues();

    void reportJobStarted(void *key);
    void reportJobFinished(void *key, const PkVector<PkRect> &rects);
    void reportUpdateFinished(const PkRect &rect);


private:
    struct Private;
    Private * const m_d;
};

#endif /* __KIS_UPDATE_TIME_MONITOR_H */
