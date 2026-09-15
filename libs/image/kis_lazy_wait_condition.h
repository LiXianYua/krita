/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LAZY_WAIT_CONDITION_H
#define __KIS_LAZY_WAIT_CONDITION_H

#include <PkMutex.h>
#include <chrono>
#include <climits>
#include <condition_variable>

/**
 * This class is used for catching a particular condition met.
 * We call it "lazy", because the decision about the condition is made
 * by the waiting thread itself. The other thread, "wakingup" one,
 * just points the former one the moments of time when the condition
 * *might* have been satisfied. This creates some limitations for
 * the condition (see a note in the end of the text).
 *
 * Usage pattern:
 *
 * Waiting thread:
 *
 * KisLazyWaitCondition condition;
 * condition.initWaiting();   // (1)
 * while(!checkSatisfied()) {
 *     condition.wait();
 * }
 * condition.endWaiting();     // (2)
 *
 *
 * Wakingup thread:
 *
 * if(checkMightSatisfied()) {
 *     condition.wakeAll();
 * }
 *
 * If the condition is met and reported, it is guaranteed that
 * all the threads, those are currently running between
 * lines (1) and (2) will be waken up and leave the loop.
 *
 * NOTE:
 * The condition checkSatisfied() must not change it's state, until
 * all the waiting threads leave the waiting loop. This requirement
 * must be guaranteed by the user of this class
 */

class KisLazyWaitCondition
{
public:
    KisLazyWaitCondition()
        : m_waitCounter(0),
          m_wakeupCounter(0)
    {
    }

    void initWaiting() {
        PkMutexLocker locker(&m_mutex);
        if(!m_waitCounter) {
            m_wakeupCounter = 0;
        }

        m_waitCounter++;
    }

    void endWaiting() {
        PkMutexLocker locker(&m_mutex);
        m_waitCounter--;
    }

    bool wait(unsigned long time = ULONG_MAX) {
        PkMutexLocker locker(&m_mutex);
        bool result = true;
        if(!m_wakeupCounter) {
            // R-80 修复：此前的写法把带超时的 wait 降级成了 PkWaitCondition 的
            // 单参 wait（永久等待），理由是「带真实超时的消费方在壳闭包外」——
            // 该前提是错的：libs/image/tests/kis_update_scheduler_test.cpp:278-315
            // 有 5 处 wait(50)，全部落在本 target 内。后果是 testLazyWaitCondition
            // 真死等，ctest 200 s 超时（R-3）。
            //
            // 语义按上游 Krita 原样恢复：time == ULONG_MAX 时不设期限（唯一
            // 生产消费者 kis_update_scheduler.cpp:56 走这条），否则到点返回 false。
            // 上游是 QWaitCondition::wait(QMutex*, unsigned long)，其对超时的
            // 返回值 = 「被唤醒 true / 超时 false」，这里用谓词版 wait_for 对齐。
            if (time == ULONG_MAX) {
                m_condition.wait(*locker.mutex());
            } else {
                std::unique_lock<PkMutex> lock(*locker.mutex(), std::adopt_lock);
                result = m_condition.wait_for(lock,
                                              std::chrono::milliseconds(time),
                                              [this] { return m_wakeupCounter > 0; });
                lock.release();
            }
        }
        if(result) {
            m_wakeupCounter--;
        }
        return result;
    }

    void wakeAll() {
        if(!m_waitCounter) return;

        PkMutexLocker locker(&m_mutex);
        if(m_waitCounter) {
            m_wakeupCounter += m_waitCounter;
            m_condition.notify_all();
        }
    }

    bool isSomeoneWaiting() {
        return m_waitCounter;
    }

private:
    PkMutex m_mutex;
    std::condition_variable_any m_condition;
    volatile int m_waitCounter;
    int m_wakeupCounter;
};

#endif /* __KIS_LAZY_WAIT_CONDITION_H */
