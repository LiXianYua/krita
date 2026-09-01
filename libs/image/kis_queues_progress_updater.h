/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_QUEUES_PROGRESS_UPDATER_H
#define __KIS_QUEUES_PROGRESS_UPDATER_H

#include <PkObject.h>
#include <PkSignalCompat.h>
#include "kritaimage_export.h"

class KoProgressProxy;


class KRITAIMAGE_EXPORT KisQueuesProgressUpdater : public PkShellObject
{
public:
    KisQueuesProgressUpdater(KoProgressProxy *progressProxy, PkObject *parent = 0);
    ~KisQueuesProgressUpdater() override;

    void updateProgress(int queueSizeMetric, const PkString &jobName);
    void hide();

private:
    void startTicking();
    void stopTicking();
    void timerTicked();

signals:
    void sigStartTicking();
    void sigStopTicking();

private:
    void startDelayElapsed();

    struct Private;
    Private * const m_d;
};

#endif /* __KIS_QUEUES_PROGRESS_UPDATER_H */
