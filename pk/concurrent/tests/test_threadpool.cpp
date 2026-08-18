#include "test_threadpool.h"
#include "../PkThreadPool.h"
#include "../PkThread.h"
#include "../PkWaitCondition.h"
#include "../PkMutex.h"
#include <atomic>
#include <vector>
#include <thread>
#include <chrono>

class CountingJob : public PkRunnable {
public:
    explicit CountingJob(std::atomic<int>& counter) : m_counter(counter) {}
    void run() override { ++m_counter; }
private:
    std::atomic<int>& m_counter;
};

void PkThreadPoolSelfTest::testIdealThreadCount()
{
    PK_VERIFY(PkThread::idealThreadCount() >= 0);
}

void PkThreadPoolSelfTest::testMaxThreadCount()
{
    PkThreadPool pool(3);
    PK_COMPARE(pool.maxThreadCount(), 3);
    pool.setMaxThreadCount(5);
    PK_COMPARE(pool.maxThreadCount(), 5);
}

void PkThreadPoolSelfTest::testRunAllJobsAutoDelete()
{
    PkThreadPool pool(4);
    std::atomic<int> counter{0};
    for (int i = 0; i < 100; ++i) {
        pool.start(new CountingJob(counter)); // autoDelete 默认 true
    }
    pool.waitForDone();
    PK_COMPARE(counter.load(), 100);
}

void PkThreadPoolSelfTest::testRunJobsNoAutoDelete()
{
    // 复现 kis_lockless_stack_test.cpp::runStressTest 的
    // setAutoDelete(false) 用法：调用方自己 delete。
    PkThreadPool pool(2);
    std::atomic<int> counter{0};
    std::vector<CountingJob*> jobs;
    for (int i = 0; i < 10; ++i) {
        auto* job = new CountingJob(counter);
        job->setAutoDelete(false);
        jobs.push_back(job);
        pool.start(job);
    }
    pool.waitForDone();
    PK_COMPARE(counter.load(), 10);
    for (auto* job : jobs) delete job;
}

void PkThreadPoolSelfTest::testMoreJobsThanThreadsQueue()
{
    // maxThreadCount < 提交数：验证排队而不是拒绝/丢弃。
    PkThreadPool pool(2);
    std::atomic<int> counter{0};
    for (int i = 0; i < 50; ++i) {
        pool.start(new CountingJob(counter));
    }
    pool.waitForDone();
    PK_COMPARE(counter.load(), 50);
}

void PkThreadPoolSelfTest::testWaitConditionWakeAll()
{
    PkMutex mutex;
    PkWaitCondition cond;
    std::atomic<int> waitingCount{0};
    std::atomic<int> wokenCount{0};

    std::vector<std::thread> waiters;
    for (int i = 0; i < 4; ++i) {
        waiters.emplace_back([&] {
            PkMutexLocker locker(&mutex);
            waitingCount.fetch_add(1, std::memory_order_relaxed);
            cond.wait(locker.mutex());
            wokenCount.fetch_add(1, std::memory_order_relaxed);
        });
    }

    // Bounded poll until all 4 waiter threads have at least incremented
    // waitingCount (they may not have entered cond.wait() yet at this point).
    for (int i = 0; i < 200 && waitingCount.load(std::memory_order_relaxed) < 4; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    PK_COMPARE(waitingCount.load(), 4);

    // Synchronization point that eliminates the missed-wakeup race: a waiter
    // thread only releases `mutex` from *inside* PkWaitCondition::wait() (the
    // atomic unlock-and-block operation). If any waiter thread has incremented
    // waitingCount but not yet called cond.wait(), it still holds `mutex`.
    // Acquiring `mutex` here therefore blocks until every waiter thread has
    // genuinely entered the wait — proving none of them can still miss the
    // upcoming wakeAll().
    {
        PkMutexLocker sync(&mutex);
    }

    cond.wakeAll();
    for (auto& t : waiters) t.join();
    PK_COMPARE(wokenCount.load(), 4);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_threadpool.inc"

int run_threadpool_tests(int argc, char **argv)
{
    PkThreadPoolSelfTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
