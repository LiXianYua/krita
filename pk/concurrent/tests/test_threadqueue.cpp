#include "test_threadqueue.h"
#include "../PkThread.h"
#include "../PkThreadCallQueue.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>

void PkThreadCallQueueSelfTest::testConcurrentReadDuringMainThreadRegistrationNoTornRead()
{
    // 复现代码评审发现的数据竞争：`registerMainThread()` 旧实现用
    // `std::atomic<bool> registered` + 裸字段 `PkThreadId id`——胜出线程
    // `registered.exchange(true)` 让 registered 对其他线程立即可见为
    // true，但 `id` 还没写完；这个窗口期内别的线程读 `mainThreadId()`
    // 是对非原子字段的数据竞争（UB，可能读到默认值或撕裂值）。
    //
    // 这里不测"多个不同线程并发抢注册"——那种场景下"后来者"本来就是
    // 不同线程，按 API 契约（换一个不同线程调用是编程错误）天然会触发
    // assert 中止，不是本次要修的竞争。真正要测的是"唯一一次合法注册"
    // 与"并发读者"之间的竞争，所以只有一个线程（本测试函数所在的主
    // 测试线程）调用 registerMainThread()，若干读者线程并发调用
    // mainThreadId() 采样，检验采到的值只可能是"未注册的默认值 PkThreadId{}"
    // 或"注册线程写入的最终真实 id"两者之一，不会读到中间态。
    //
    // 必须是进程内第一次真正的 registerMainThread() 调用：本测试排在
    // testMainThreadRegistration 之前（见 .h），保证调用发生时全局单例
    // 还处于"未注册"状态，才有"未注册→已注册"这条窗口可观察。
    std::atomic<bool> stop{false};
    std::atomic<bool> registerDone{false};
    std::atomic<int> tornReadCount{0};
    PkThreadId registrarId{};

    const int kReaderThreads = 8;
    std::vector<std::thread> readers;
    readers.reserve(kReaderThreads);
    for (int i = 0; i < kReaderThreads; ++i) {
        readers.emplace_back([&]{
            while (!stop.load()) {
                PkThreadId observed = PkThread::mainThreadId();
                // registerDone 是 std::atomic<bool>，这里的 load() 与注册线程
                // 那边的 store() 构成 release-acquire：一旦看见 registerDone
                // 为 true，registrarId 的写入（在 store 之前完成）对本线程
                // 必然可见，读 registrarId 本身不再有竞争。
                if (registerDone.load()) {
                    if (observed != PkThreadId{} && observed != registrarId) {
                        tornReadCount++;
                    }
                }
            }
        });
    }

    registrarId = PkThread::currentThreadId();
    PkThread::registerMainThread();
    registerDone = true;

    // 注册完成后再多跑一小段时间，让读者线程在"registered 已可见"之后
    // 继续采样，扩大捕获竞争窗口的机会。
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    stop = true;
    for (auto& t : readers) { t.join(); }

    PK_COMPARE(tornReadCount.load(), 0);
    PK_VERIFY(PkThread::mainThreadId() == registrarId);
}

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
