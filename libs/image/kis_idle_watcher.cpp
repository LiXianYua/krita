/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_idle_watcher.h"

#include "kis_image.h"
#include "kis_signal_auto_connection.h"
#include "kis_signal_compressor.h"

#include <PkTimer.h>
#include <chrono>


struct KisIdleWatcher::Private
{
    static const int IDLE_CHECK_COUNT = 4; /* ticks */

    Private(int delay, KisIdleWatcher *q)
        : imageModifiedCompressor(delay,
                                  KisSignalCompressor::POSTPONE, q),
          idleCheckCounter(0),
          q(q),
          idleCheckDelay(delay)
    {
    }

    // PkTimer 无单发定时信号，改用回调式 API：
    // 每次 start() 都带上固定间隔 + 回调（对齐 Qt 侧 singleShot=true + start()）。
    void startIdleCheckTimer()
    {
        idleCheckTimer.start(std::chrono::milliseconds(idleCheckDelay),
                             [this]() { q->slotIdleCheckTick(); }, true);
    }

    KisSignalAutoConnectionsStore connectionsStore;
    PkVector<KisImageWSP> trackedImages;

    KisSignalCompressor imageModifiedCompressor;

    PkTimer idleCheckTimer;
    KisIdleWatcher *q;
    int idleCheckDelay;

    /**
     * We wait until the counter reaches IDLE_CHECK_COUNT, then consider the
     * image to be really "idle". If the counter is negative, it means that
     * "no delay" update is triggered, which disables counting and the event
     * is triggered on the next non-busy tick.
     */
    int idleCheckCounter;
};

KisIdleWatcher::KisIdleWatcher(int delay, PkObject *parent)
    : PkShellObject(parent), m_d(new Private(delay, this))
{
    PkObject::connect(&m_d->imageModifiedCompressor, &KisSignalCompressor::timeout,
                      this, &KisIdleWatcher::startIdleCheck);
}

KisIdleWatcher::~KisIdleWatcher()
{
}

bool KisIdleWatcher::isIdle() const
{
    bool idle = true;

    Q_FOREACH (KisImageSP image, m_d->trackedImages) {
        if (!image) continue;

        if (!image->isIdle()) {
            idle = false;
            break;
        }
    }

    return idle;
}

bool KisIdleWatcher::isCounting() const
{
    return m_d->idleCheckTimer.isActive();
}

void KisIdleWatcher::setTrackedImages(const PkVector<KisImageSP> &images)
{
    m_d->connectionsStore.clear();
    m_d->trackedImages.clear();

    Q_FOREACH (KisImageSP image, images) {
        if (image) {
            m_d->trackedImages << image;
            m_d->connectionsStore.addConnection(image.data(), &KisImage::sigImageModified,
                                                this, &KisIdleWatcher::slotImageModified);

            m_d->connectionsStore.addConnection(image.data(), &KisImage::sigIsolatedModeChanged,
                                                this, &KisIdleWatcher::slotImageModified);
        }
    }
}

void KisIdleWatcher::setTrackedImage(KisImageSP image)
{
    PkVector<KisImageSP> images;
    images << image;
    setTrackedImages(images);
}

void KisIdleWatcher::restartCountdown()
{
    stopIdleCheck();
    m_d->imageModifiedCompressor.start();
}

void KisIdleWatcher::triggerCountdownNoDelay()
{
    stopIdleCheck();
    m_d->idleCheckCounter = -1;
    m_d->startIdleCheckTimer();
}

void KisIdleWatcher::slotImageModified()
{
    if (m_d->idleCheckCounter >= 0) {
        restartCountdown();
    }
    imageModified();
}

void KisIdleWatcher::startIdleCheck()
{
    m_d->idleCheckCounter = 0;
    m_d->startIdleCheckTimer();
}

void KisIdleWatcher::stopIdleCheck()
{
    m_d->idleCheckTimer.stop();
    m_d->idleCheckCounter = 0;
}

void KisIdleWatcher::slotIdleCheckTick()
{
    if (isIdle()) {
        if (m_d->idleCheckCounter < 0 ||
            m_d->idleCheckCounter >= Private::IDLE_CHECK_COUNT) {

            stopIdleCheck();
            if (!m_d->trackedImages.isEmpty()) {
                startedIdleMode();
            }
        } else {
            m_d->idleCheckCounter++;
            m_d->startIdleCheckTimer();
        }
    } else {
        if (m_d->idleCheckCounter >= 0) {
            restartCountdown();
        } else {
            m_d->startIdleCheckTimer();
        }
    }
}
