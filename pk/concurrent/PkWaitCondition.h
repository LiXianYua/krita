#pragma once
#include <condition_variable>
#include "PkMutex.h"

// 替代 <QWaitCondition>。方法面 = kis_updater_context.cpp 实测用到的
// wait(QMutex*)/wakeAll()（wakeOne 保留范围内 0 调用点，实现成本为零，
// 一并给出以保持接口完整，reviewer 可判定是否算范围蔓延）。
//
// Qt 语义：wait(QMutex* lockedMutex) 要求调用方已持有该锁，函数内部
// 解锁-等待-重新加锁，返回时锁仍由调用方持有。用 std::condition_variable_any
// + std::unique_lock<PkMutex>(adopt_lock) 表达"这个锁已经被锁上了，
// 由我来接管解锁/重新加锁"；release() 让 unique_lock 析构时不做多余的
// unlock（调用方的 PkMutexLocker 还认为自己持有锁，返回后确实又是锁着的）。
class PkWaitCondition {
public:
    void wait(PkMutex* mutex) {
        std::unique_lock<PkMutex> lock(*mutex, std::adopt_lock);
        m_cv.wait(lock);
        lock.release();
    }
    void wakeOne() { m_cv.notify_one(); }
    void wakeAll() { m_cv.notify_all(); }

private:
    std::condition_variable_any m_cv;
};
