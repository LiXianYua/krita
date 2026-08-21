/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include "KoProgressUpdater.h"

#include <PkString.h>
#include <PkThread.h>
#include <PkEventLoop.h>
#include <PkList.h>
#include <PkMutex.h>

#include <algorithm>
#include <cmath>

#include "KoUpdaterPrivate_p.h"
#include "KoUpdater.h"
#include "KoProgressProxy.h"

#include "kis_signal_compressor.h"

#include <kis_debug.h>

class KoProgressUpdater::Private
{
public:

    Private(KoProgressUpdater *_q, KoProgressProxy *proxy, PkPointer<KoUpdater> parentUpdater, Mode _mode)
        : q(_q)
        , parentProgressProxy(proxy)
        , parentUpdater(parentUpdater)
        , mode(_mode)
        , updateCompressor(new KisSignalCompressor(250, KisSignalCompressor::FIRST_ACTIVE, q))
        , canceled(false)
    {
    }

    KoProgressUpdater *q;

private:
    KoProgressProxy *parentProgressProxy;
    PkPointer<KoUpdater> parentUpdater;

public:
    Mode mode;
    int currentProgress = 0;
    bool isUndefinedState = false;
    KisSignalCompressor *updateCompressor;
    PkList<PkPointer<KoUpdaterPrivate> > subtasks;
    bool canceled;
    int updateInterval = 250; // ms, 4 updates per second should be enough
    bool autoNestNames = false;
    PkString taskName;
    int taskMax = 99;
    bool isStarted = false;

    PkMutex mutex;

    void updateParentText();
    void clearState();

    KoProgressProxy* progressProxy() {
        return parentUpdater ? parentUpdater : parentProgressProxy;
    }
};

// NOTE: do not make the KoProgressUpdater object part of the PkObject
// hierarchy. Do not make KoProgressProxy its parent (note that KoProgressProxy
// is not necessarily castable to PkObject ). This prevents proper functioning
// of progress reporting in multi-threaded environments.
KoProgressUpdater::KoProgressUpdater(KoProgressProxy *progressProxy, Mode mode)
    : d (new Private(this, progressProxy, 0, mode))
{
    KIS_ASSERT_RECOVER_RETURN(progressProxy);
    PkObject::connect(d->updateCompressor, &KisSignalCompressor::timeout,
                      this, &KoProgressUpdater::updateUi);
    PkObject::connect(this, &KoProgressUpdater::triggerUpdateAsynchronously,
                      d->updateCompressor, &KisSignalCompressor::start);
    Q_EMIT triggerUpdateAsynchronously();
}

KoProgressUpdater::KoProgressUpdater(PkPointer<KoUpdater> updater)
    : d (new Private(this, 0, updater, Unthreaded))
{
    KIS_ASSERT_RECOVER_RETURN(updater);
    PkObject::connect(d->updateCompressor, &KisSignalCompressor::timeout,
                      this, &KoProgressUpdater::updateUi);
    PkObject::connect(this, &KoProgressUpdater::triggerUpdateAsynchronously,
                      d->updateCompressor, &KisSignalCompressor::start);
    Q_EMIT triggerUpdateAsynchronously();
}

KoProgressUpdater::~KoProgressUpdater()
{
    if (d->progressProxy()) {
        d->progressProxy()->setRange(0, d->taskMax);
        d->progressProxy()->setValue(d->progressProxy()->maximum());
    }

    // make sure to stop the timer to avoid accessing
    // the data we are going to delete right now
    d->updateCompressor->stop();

    for (PkPointer<KoUpdaterPrivate> updater : d->subtasks) {
        delete updater.data();
    }
    d->subtasks.clear();

    delete d;
}

void KoProgressUpdater::start(int range, const PkString &text)
{
    {
        PkMutexLocker l(&d->mutex);
        d->clearState();
        d->taskName = text;
        d->taskMax = range - 1;
        d->isStarted = true;
        d->currentProgress = 0;
    }

    Q_EMIT triggerUpdateAsynchronously();
}

PkPointer<KoUpdater> KoProgressUpdater::startSubtask(int weight,
                                                    const PkString &name,
                                                    bool isPersistent)
{
    if (!d->isStarted) {
        // lazy initialization for intermediate proxies
        start();
    }

    KoUpdaterPrivate *p = new KoUpdaterPrivate(weight, name, isPersistent);

    {
        PkMutexLocker l(&d->mutex);
        d->subtasks.append(p);
    }
    PkObject::connect(p, &KoUpdaterPrivate::sigUpdated,
                      this, &KoProgressUpdater::update);
    PkObject::connect(p, &KoUpdaterPrivate::sigCancelled,
                      this, &KoProgressUpdater::cancel);

    PkPointer<KoUpdater> updater = p->connectedUpdater();

    Q_EMIT triggerUpdateAsynchronously();
    return updater;
}

void KoProgressUpdater::removePersistentSubtask(PkPointer<KoUpdater> updater)
{
    {
        PkMutexLocker l(&d->mutex);

        for (auto it = d->subtasks.begin(); it != d->subtasks.end();) {
            if ((*it)->connectedUpdater() != updater) {
                ++it;
            } else {
                KIS_SAFE_ASSERT_RECOVER_NOOP((*it)->isPersistent());
                (*it)->deleteLater();
                it = d->subtasks.erase(it);
                break;
            }
        }
    }

    Q_EMIT triggerUpdateAsynchronously();
}

void KoProgressUpdater::cancel()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(PkThread::currentThreadId() == this->thread());

    PkList<PkPointer<KoUpdaterPrivate> > subtasks;

    {
        PkMutexLocker l(&d->mutex);
        subtasks = d->subtasks;
    }

    for (PkPointer<KoUpdaterPrivate> updater : subtasks) {
        if (!updater) continue;

        updater->setProgress(100);
        updater->setInterrupted(true);
    }
    d->canceled = true;

    Q_EMIT triggerUpdateAsynchronously();
}

void KoProgressUpdater::update()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(PkThread::currentThreadId() == this->thread());

    if (d->mode == Unthreaded) {
        PkEventLoop::processEvents();
    }

    d->updateCompressor->start();
}

void KoProgressUpdater::updateUi()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(PkThread::currentThreadId() == this->thread());

    // This function runs in the app main thread. All the progress
    // updates arrive at the KoUpdaterPrivate instances through
    // queued connections, so until we relinquish control to the
    // event loop, the progress values cannot change, and that
    // won't happen until we return from this function (which is
    // triggered by a timer)

    {
        PkMutexLocker l(&d->mutex);

        if (!d->subtasks.isEmpty()) {
            int totalProgress = 0;
            int totalWeight = 0;
            d->isUndefinedState = false;

            const PkList<PkPointer<KoUpdaterPrivate> > subtasks = d->subtasks;
            for (PkPointer<KoUpdaterPrivate> updater : subtasks) {
                if (updater->interrupted()) {
                    d->currentProgress = -1;
                    break;
                }

                if (!updater->hasValidRange()) {
                    totalWeight = 0;
                    totalProgress = 0;
                    d->isUndefinedState = true;
                    break;
                }

                if (updater->isPersistent() && updater->isCompleted()) {
                    continue;
                }

                const int progress = std::clamp(updater->progress(), 0, 100);
                totalProgress += progress * updater->weight();
                totalWeight += updater->weight();
            }

            const int progressPercent = totalWeight > 0 ? totalProgress / totalWeight : -1;

            d->currentProgress =
                    d->taskMax == 99 ?
                        progressPercent :
                        static_cast<int>(std::lround(double(progressPercent) * d->taskMax / 99.0));
        }

    }

    if (d->progressProxy()) {
        if (!d->isUndefinedState) {
            d->progressProxy()->setRange(0, d->taskMax);

            if (d->currentProgress == -1) {
                d->currentProgress = d->progressProxy()->maximum();
            }

            if (d->currentProgress >= d->progressProxy()->maximum()) {
                {
                    PkMutexLocker l(&d->mutex);
                    d->clearState();
                }
                d->progressProxy()->setRange(0, d->taskMax);
                d->progressProxy()->setValue(d->progressProxy()->maximum());
            } else {
                d->progressProxy()->setValue(d->currentProgress);
            }
        } else {
            d->progressProxy()->setRange(0,0);
            d->progressProxy()->setValue(0);
        }

        d->updateParentText();
    }
}

void KoProgressUpdater::Private::updateParentText()
{
    if (!progressProxy()) return;

    PkString actionName = taskName;

    if (autoNestNames) {
        for (PkPointer<KoUpdaterPrivate> updater : subtasks) {

            if (updater->isPersistent() && updater->isCompleted()) {
                continue;
            }

            if (updater->progress() < 100) {
                const PkString subTaskName = updater->mergedSubTaskName();

                if (!subTaskName.isEmpty()) {
                    if (actionName.isEmpty()) {
                        actionName = subTaskName;
                    } else {
                        actionName = PkString("%1: %2").arg(actionName).arg(subTaskName);
                    }
                }
                break;
            }
        }
        progressProxy()->setAutoNestedName(actionName);
    } else {
        progressProxy()->setFormat(actionName);
    }

}

void KoProgressUpdater::Private::clearState()
{
    for (auto it = subtasks.begin(); it != subtasks.end();) {
        if (!(*it)->isPersistent()) {
            (*it)->deleteLater();
            it = subtasks.erase(it);
        } else {
            if ((*it)->interrupted()) {
                (*it)->setInterrupted(false);
            }
            ++it;
        }
    }

    canceled = false;
}

bool KoProgressUpdater::interrupted() const
{
    return d->canceled;
}

void KoProgressUpdater::setUpdateInterval(int ms)
{
    d->updateCompressor->setDelay(ms);
}

int KoProgressUpdater::updateInterval() const
{
    return d->updateCompressor->delay();
}

void KoProgressUpdater::setAutoNestNames(bool value)
{
    d->autoNestNames = value;
}

bool KoProgressUpdater::autoNestNames() const
{
    return d->autoNestNames;
}
