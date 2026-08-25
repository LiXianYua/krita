/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_queues_progress_updater.h"

#include <PkMutex.h>
#include <PkTimer.h>
#include <KoProgressProxy.h>


struct Q_DECL_HIDDEN KisQueuesProgressUpdater::Private
{
    Private()
        : queueSizeMetric(0)
        , initialQueueSizeMetric(0)
        , progressProxy(0)
        , tickingRequested(false)
    {
    }

    PkMutex mutex;
    PkTimer timer;
    PkTimer startDelayTimer;

    int queueSizeMetric;
    int initialQueueSizeMetric;
    PkString jobName;

    KoProgressProxy *progressProxy;

    bool tickingRequested;

    static const int TIMER_INTERVAL = 500;
    static const int PROGRESS_DELAY = 1000;
};


KisQueuesProgressUpdater::KisQueuesProgressUpdater(KoProgressProxy *progressProxy, PkObject *parent)
    : PkShellObject(parent),
      m_d(new Private())
{
    m_d->progressProxy = progressProxy;

    PkObject::connect(this, &KisQueuesProgressUpdater::sigStartTicking,
                      this, &KisQueuesProgressUpdater::startTicking, PkConnectionType::Queued);
    PkObject::connect(this, &KisQueuesProgressUpdater::sigStopTicking,
                      this, &KisQueuesProgressUpdater::stopTicking, PkConnectionType::Queued);
}

KisQueuesProgressUpdater::~KisQueuesProgressUpdater()
{
    delete m_d;
}

void KisQueuesProgressUpdater::updateProgress(int queueSizeMetric, const PkString &jobName)
{
    PkMutexLocker locker(&m_d->mutex);

    m_d->queueSizeMetric = queueSizeMetric;

    if (queueSizeMetric &&
        (jobName != m_d->jobName ||
         m_d->queueSizeMetric > m_d->initialQueueSizeMetric)) {

        m_d->jobName = jobName;
        m_d->initialQueueSizeMetric = m_d->queueSizeMetric;
    }

    if (m_d->queueSizeMetric && !m_d->tickingRequested) {

        m_d->tickingRequested = true;
        Q_EMIT sigStartTicking();

    } else if (!m_d->queueSizeMetric && m_d->tickingRequested) {

        m_d->initialQueueSizeMetric = 0;
        m_d->jobName = PkString();
        m_d->tickingRequested = false;
        Q_EMIT sigStopTicking();
    }
}

void KisQueuesProgressUpdater::hide()
{
    updateProgress(0, "");
}

void KisQueuesProgressUpdater::startTicking()
{
    m_d->startDelayTimer.start(std::chrono::milliseconds(Private::PROGRESS_DELAY),
                               [this]() { startDelayElapsed(); },
                               true);
}

void KisQueuesProgressUpdater::startDelayElapsed()
{
    m_d->timer.start(std::chrono::milliseconds(Private::TIMER_INTERVAL),
                     [this]() { timerTicked(); },
                     false);
    timerTicked();
}

void KisQueuesProgressUpdater::stopTicking()
{
    m_d->startDelayTimer.stop();
    m_d->timer.stop();
    timerTicked();
}

void KisQueuesProgressUpdater::timerTicked()
{
    PkMutexLocker locker(&m_d->mutex);

    if (!m_d->initialQueueSizeMetric) {
        m_d->progressProxy->setRange(0, 100);
        m_d->progressProxy->setValue(100);
        m_d->progressProxy->setFormat("%p%");
    } else {
        m_d->progressProxy->setRange(0, m_d->initialQueueSizeMetric);
        m_d->progressProxy->setValue(m_d->initialQueueSizeMetric - m_d->queueSizeMetric);
        m_d->progressProxy->setFormat(m_d->jobName);
    }
}
