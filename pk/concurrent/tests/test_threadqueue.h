#pragma once
#include <QObject>      // → pk/test/compat/QObject，提供 QObject/Q_OBJECT/Q_SLOTS
#include <PkTest.h>

class PkThreadCallQueueSelfTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    // 必须排在 testMainThreadRegistration 之前：本测试要成为进程内第一次
    // 真正的 registerMainThread() 调用，才能观察"未注册→已注册"这个窗口
    // 期本身；如果排在后面，全局单例已经被后者定型，就测不到这条竞争了
    // （详见 .cpp 里的注释）。
    void testConcurrentReadDuringMainThreadRegistrationNoTornRead();
    void testCurrentThreadIdDiffersAcrossThreads();
    void testMainThreadRegistration();
    void testPostDefersToTargetThreadPump();
    void testPostRoutesToRealWorkerThread();
    void testPostBlockingWaitsForExecution();
};
