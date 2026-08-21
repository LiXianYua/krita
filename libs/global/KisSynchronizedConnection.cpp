/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisSynchronizedConnection.h"

#include <PkThread.h>
#include <PkThreadCallQueue.h>

#include <kis_assert.h>

struct KisSynchronizedConnectionState
{
    bool enableAutoModeForUnittests = false;
};

struct KisBarrierCallbackContainer
{
    std::function<void()> callback;
};

static KisSynchronizedConnectionState *s_state()
{
    static KisSynchronizedConnectionState instance;
    return &instance;
}
static KisBarrierCallbackContainer *s_barrier()
{
    static KisBarrierCallbackContainer instance;
    return &instance;
}

/************************************************************************/
/*            KisSynchronizedConnectionBase                             */
/************************************************************************/

void KisSynchronizedConnectionBase::registerSynchronizedEventBarrier(std::function<void ()> callback)
{
    KIS_SAFE_ASSERT_RECOVER_NOOP(!s_barrier()->callback);
    s_barrier()->callback = callback;
}

void KisSynchronizedConnectionBase::setAutoModeForUnittestsEnabled(bool value)
{
    s_state()->enableAutoModeForUnittests = value;
}

bool KisSynchronizedConnectionBase::isAutoModeForUnittestsEnabled()
{
    return s_state()->enableAutoModeForUnittests;
}

void KisSynchronizedConnectionBase::forceDeliverAllSynchronizedEvents()
{
    // 抽干当前线程队列里排队中的同步连接投递，然后执行屏障回调。
    PkThreadCallQueue::processPendingCalls();
    KIS_SAFE_ASSERT_RECOVER_RETURN(s_barrier()->callback);
    s_barrier()->callback();
}

void KisSynchronizedConnectionBase::postEvent()
{
    if (s_state()->enableAutoModeForUnittests &&
            PkThread::currentThreadId() == this->thread()) {
        if (s_barrier()->callback) {
            s_barrier()->callback();
        }
        deliverEventToReceiver();
    } else {
        // R-30 迁移：postEvent 的跨线程投递改用 PkThreadCallQueue。
        // 目标线程必须安装显式 pump（PkThreadCallQueue::processPendingCalls）
        // 才会真正执行——pump 所有权归 F-00，本任务只迁移投递机制。
        PkThreadCallQueue::post(this->thread(), [this]() {
            deliverEventToReceiver();
        });
    }
}
