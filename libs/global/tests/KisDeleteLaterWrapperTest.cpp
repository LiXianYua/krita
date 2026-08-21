/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "KisDeleteLaterWrapper.h"

#include <PkEventLoop.h>
#include <PkThread.h>
#include <PkThreadCallQueue.h>

namespace {

struct DestructionProbe {
    explicit DestructionProbe(bool *destroyed)
        : destroyed(destroyed)
    {
    }

    ~DestructionProbe()
    {
        *destroyed = true;
    }

    bool *destroyed;
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
    auto *wrapper = makeKisDeleteLaterWrapper(new DestructionProbe(&destroyed));
    if (wrapper->thread() != PkThread::mainThreadId()) {
        delete wrapper;
        return 2;
    }

    wrapper->deleteLater();
    if (destroyed) {
        return 3;
    }

    if (PkEventLoop::processEvents() <= 0 || !destroyed) {
        return 4;
    }

    return 0;
}
