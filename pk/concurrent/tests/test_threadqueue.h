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

    // final whole-branch review C-1：线程 id 复用导致陈旧调用被无关新线程执行。
    void testFirstPumpDiscardsPreexistingStaleEntries();
    void testStaleCallNeverExecutedOnReusedThreadIdBestEffort();

    // final whole-branch review C-2：postBlocking 槽函数抛异常导致发射线程永久阻塞。
    void testPostBlockingWakesEmitterAndRethrowsOnSlotException();
    void testProcessPendingCallsContinuesAfterExceptionInBatch();

    // fix-wave re-review NEW-C1：postBlocking 投给一个还没预热过的线程，
    // 被 C-1 的"首次触达丢弃陈旧条目"清空时，发射线程曾经永久挂起。
    void testPostBlockingWakesAndReportsAbandonedWhenDiscardedByFirstPump();
    void testPostBlockingWakesWhenTargetThreadExitsWithoutPumping();

    // fix-wave re-review NEW-I1：post() 曾经错误地清空调用者自己的入站队列。
    void testPostDoesNotDiscardOwnInboundQueueOnOutboundPost();

    void testWarmUpReturnsCurrentThreadIdAfterDiscardingStaleEntries();
    void testProcessEventsProcessesOneSnapshotOnly();
    void testExecUntilPumpsUntilPredicateIsSatisfied();
};
