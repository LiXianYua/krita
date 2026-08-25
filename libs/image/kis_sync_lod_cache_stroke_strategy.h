/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SYNC_LOD_CACHE_STROKE_STRATEGY_H
#define __KIS_SYNC_LOD_CACHE_STROKE_STRATEGY_H

#include "kritaimage_export.h"
#include <KisRunnableBasedStrokeStrategy.h>

#include <PkScopedPointer.h>

class KisUpdatesFacade;

class KRITAIMAGE_EXPORT KisSyncLodCacheStrokeStrategy : public KisRunnableBasedStrokeStrategy
{
public:
    KisSyncLodCacheStrokeStrategy(KisImageWSP image, bool forgettable);
    ~KisSyncLodCacheStrokeStrategy() override;

    static PkList<KisStrokeJobData*> createJobsData(KisImageWSP image);

    static void createJobsData(PkVector<KisStrokeJobData *> &jobs, KisNodeSP imageRoot, KisUpdatesFacade *updatesFacade, int levelOfDetail, KisPaintDeviceList extraDevices = {});

private:
    void initStrokeCallback() override;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_SYNC_LOD_CACHE_STROKE_STRATEGY_H */
