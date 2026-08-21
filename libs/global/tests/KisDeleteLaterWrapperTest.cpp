/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "KisDeleteLaterWrapper.h"

#include <PkEventLoop.h>
#include <PkThread.h>
#include <PkThreadCallQueue.h>

#include <thread>

namespace {

struct DestructionProbe {
    explicit DestructionProbe(bool *destroyed, PkThreadId *destructionThread)
        : destroyed(destroyed), destructionThread(destructionThread)
    {
    }

    ~DestructionProbe()
    {
        *destroyed = true;
        *destructionThread = PkThread::currentThreadId();
    }

    bool *destroyed;
    PkThreadId *destructionThread;
};

}

int main()
{
    PkThread::registerMainThread();
    const PkThreadId warmedMainThread = PkThreadCallQueue::warmUpCurrentThread();
    if (warmedMainThread != PkThread::mainThreadId()) {
        return 1;
    }

    bool destroyed = false;
    PkThreadId destructionThread{};
    PkThreadId workerThread{};
    KisDeleteLaterWrapper<DestructionProbe *> *wrapper = nullptr;
    std::thread worker([&] {
        workerThread = PkThread::currentThreadId();
        wrapper = makeKisDeleteLaterWrapper(new DestructionProbe(&destroyed, &destructionThread));
    });
    worker.join();

    if (!wrapper || wrapper->thread() != PkThread::mainThreadId() ||
            wrapper->thread() == workerThread) {
        delete wrapper;
        return 2;
    }

    wrapper->deleteLater();
    if (destroyed) {
        return 3;
    }

    if (PkEventLoop::processEvents() <= 0 || !destroyed ||
            destructionThread != PkThread::mainThreadId()) {
        return 4;
    }

    return 0;
}
