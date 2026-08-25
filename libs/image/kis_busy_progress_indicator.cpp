/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_busy_progress_indicator.h"

#include <PkTimer.h>
#include <PkAtomic.h>

#include "KoProgressProxy.h"


struct KisBusyProgressIndicator::Private
{
    Private()
        : timer(new PkTimer())
        {}

    PkTimer *timer {nullptr}; // owned by KisBusyProgressIndicator (deleted in dtor)
    int numEmptyTicks {0};
    PkAtomicInt numUpdates;
    PkAtomicInt timerStarted;
    KoProgressProxy *progressProxy {nullptr};

    bool isStarted {false};

    void startProgressReport()
    {
        if (!progressProxy) {
            return;
        }
        isStarted = true;
        progressProxy->setRange(0, 0);
    }

    void stopProgressReport()
    {
        if (!isStarted || !progressProxy) {
            return;
        }
        progressProxy->setRange(0, 100);
        progressProxy->setValue(100);
        isStarted = false;
    }
};


KisBusyProgressIndicator::KisBusyProgressIndicator(KoProgressProxy *progressProxy)
    : m_d(new Private())
{
    PkObject::connect(this, &KisBusyProgressIndicator::sigStartTimer,
                      this, &KisBusyProgressIndicator::slotStartTimer);
    m_d->progressProxy = progressProxy;
}

KisBusyProgressIndicator::~KisBusyProgressIndicator()
{
    m_d->timer->stop();
    delete m_d->timer;
    m_d->timer = nullptr;
    m_d->stopProgressReport();
}

void KisBusyProgressIndicator::prepareDestroying()
{
    m_d->progressProxy = 0;
}

void KisBusyProgressIndicator::timerFinished()
{
    int value = m_d->numUpdates.fetchAndStoreOrdered(0);

    if (!value) {
        m_d->numEmptyTicks++;

        if (m_d->numEmptyTicks > 2) {
            m_d->timerStarted = 0;
            m_d->timer->stop();
            m_d->stopProgressReport();
        }
    } else {
        m_d->numEmptyTicks = 0;
    }
}

void KisBusyProgressIndicator::update()
{
    m_d->numUpdates.ref();

    if (!m_d->timerStarted) {
        sigStartTimer();
    }
}

void KisBusyProgressIndicator::slotStartTimer()
{
    m_d->timerStarted.ref();
    m_d->timer->start(std::chrono::milliseconds(200), [this]() { timerFinished(); });
    m_d->startProgressReport();
}
