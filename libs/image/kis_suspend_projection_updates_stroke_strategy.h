/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SUSPEND_PROJECTION_UPDATES_STROKE_STRATEGY_H
#define __KIS_SUSPEND_PROJECTION_UPDATES_STROKE_STRATEGY_H

#include <KisRunnableBasedStrokeStrategy.h>

#include <PkScopedPointer.h>
#include "kis_projection_updates_filter.h"

class KisSuspendProjectionUpdatesStrokeStrategy : public KisRunnableBasedStrokeStrategy
{
public:
    struct SuspendUpdatesFilterInterface : public KisProjectionUpdatesFilter
    {
        virtual void addExplicitUIUpdateRect(const PkRect &rc) = 0;
    };

    struct SharedData {
        KisProjectionUpdatesFilterCookie installedFilterCookie = {};
    };
    using SharedDataSP = PkSharedPointer<SharedData>;

public:
    KisSuspendProjectionUpdatesStrokeStrategy(KisImageWSP image, bool suspend, SharedDataSP sharedData);
    ~KisSuspendProjectionUpdatesStrokeStrategy() override;

    static PkList<KisStrokeJobData*> createSuspendJobsData(KisImageWSP image);
    static PkList<KisStrokeJobData*> createResumeJobsData(KisImageWSP image);
    static SharedDataSP createSharedData();

private:
    void initStrokeCallback() override;
    void doStrokeCallback(KisStrokeJobData *data) override;
    void cancelStrokeCallback() override;

    void suspendStrokeCallback() override;
    void resumeStrokeCallback() override;

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_SUSPEND_PROJECTION_UPDATES_STROKE_STRATEGY_H */
