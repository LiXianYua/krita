/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisSafeBlockingQueueConnectionProxy.h"

#include <PkThread.h>
#include <KisBusyWaitBroker.h>

void KisSafeBlockingQueueConnectionProxyPrivate::passBlockingSignalSafely(FunctionToSignalProxy &source, SignalToFunctionProxy &destination)
{
    if (PkThread::currentThreadId() == PkThread::mainThreadId() ||
        KisBusyWaitBroker::instance()->guiThreadIsWaitingForBetterWeather()) {

        destination.start();
    } else {
        source.start();
    }
}

/**
 * NOTE (R-30): the source->destination connection in the header is a Pk
 * BlockingQueued connection, delivered through the destination thread's
 * PkThreadCallQueue. Contract for the GUI (destination) thread:
 *  1. it MUST call PkThreadCallQueue::warmUpCurrentThread() before publishing
 *     its thread id anywhere (in particular before this proxy's objects are
 *     moveToThread()'d to PkThread::mainThreadId());
 *  2. it MUST keep pumping via PkThreadCallQueue::processPendingCalls() in its
 *     event loop, otherwise a BlockingQueued post from a worker thread would
 *     never be drained and the worker would block forever.
 * If the queue discards the post (destination thread exited), the worker's
 * postBlocking() wakes with PkCallAbandonedException instead of deadlocking.
 */
void KisSafeBlockingQueueConnectionProxyPrivate::initProxyObject(PkObject *object)
{
    object->moveToThread(PkThread::mainThreadId());
}
