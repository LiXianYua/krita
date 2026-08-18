#include "test_threadqueue.h"
#include "../PkThread.h"
#include "../PkThreadCallQueue.h"
#include <atomic>
#include <chrono>
#include <thread>

void PkThreadCallQueueSelfTest::testCurrentThreadIdDiffersAcrossThreads()
{
    PkThreadId main_id = PkThread::currentThreadId();
    PkThreadId worker_id{};
    std::thread t([&]{ worker_id = PkThread::currentThreadId(); });
    t.join();
    PK_VERIFY(main_id != worker_id);
    PK_VERIFY(PkThread::currentThreadId() == main_id);
}

void PkThreadCallQueueSelfTest::testMainThreadRegistration()
{
    PkThread::registerMainThread();
    PK_VERIFY(PkThread::mainThreadId() == PkThread::currentThreadId());
}

void PkThreadCallQueueSelfTest::testPostDefersToTargetThreadPump()
{
    // 模拟"跨线程 Queued"：post 到当前线程自己的队列，post 后不会自动执行，
    // 只有显式 processPendingCalls() 才执行——这是探针实验2/6 实测出的语义。
    PkThreadId me = PkThread::currentThreadId();
    std::atomic<int> called{0};
    PkThreadCallQueue::post(me, [&called]{ called++; });
    PK_VERIFY(called.load() == 0);
    PK_COMPARE(PkThreadCallQueue::pendingCount(), static_cast<size_t>(1));
    int n = PkThreadCallQueue::processPendingCalls();
    PK_COMPARE(n, 1);
    PK_VERIFY(called.load() == 1);
    PK_COMPARE(PkThreadCallQueue::pendingCount(), static_cast<size_t>(0));
}

void PkThreadCallQueueSelfTest::testPostRoutesToRealWorkerThread()
{
    std::atomic<int> called{0};
    std::atomic<bool> workerReady{false};
    std::atomic<bool> stop{false};
    PkThreadId workerId{};
    std::thread worker([&]{
        workerId = PkThread::currentThreadId();
        workerReady = true;
        while (!stop.load()) {
            PkThreadCallQueue::processPendingCalls();
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    });
    while (!workerReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    PkThreadCallQueue::post(workerId, [&called]{ called++; });
    // 轮询等待 worker 的 pump 循环执行到（本测试自己的等待逻辑，不代表投递
    // 原语本身有轮询——post/processPendingCalls 都是 O(1) 队列操作）。
    for (int i = 0; i < 200 && called.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    PK_VERIFY(called.load() == 1);
    stop = true;
    worker.join();
}

void PkThreadCallQueueSelfTest::testPostBlockingWaitsForExecution()
{
    std::atomic<bool> workerReady{false};
    std::atomic<bool> stop{false};
    PkThreadId workerId{};
    std::thread worker([&]{
        workerId = PkThread::currentThreadId();
        workerReady = true;
        while (!stop.load()) {
            PkThreadCallQueue::processPendingCalls();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    while (!workerReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

    std::atomic<int> called{0};
    PkThreadCallQueue::postBlocking(workerId, [&called]{ called++; });
    PK_VERIFY(called.load() == 1);

    stop = true;
    worker.join();
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_threadqueue.inc"

int run_threadqueue_tests(int argc, char **argv)
{
    PkThreadCallQueueSelfTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
