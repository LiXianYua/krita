#include "test_threadqueue.h"
#include "../PkThread.h"
#include "../PkThreadCallQueue.h"
#include <atomic>
#include <chrono>
#include <thread>
#include <vector>
#include <stdexcept>
#include <string>

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
        // 先"预热"一次 processPendingCalls()，建立本线程在 registry 里的
        // C-1 修复"已触达"标记——此刻外界还不知道 workerId（workerReady
        // 还没置 true），队列必然为空，是无害 no-op。这一步必须严格发生在
        // workerReady=true 之前，否则主线程的 post() 与本线程"第一次调用
        // processPendingCalls()"之间会有真实竞争，C-1 修复的"首次 pump
        // 丢弃陈旧条目"逻辑可能把这个合法调用当成陈旧条目丢掉。
        PkThreadCallQueue::processPendingCalls();
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
        // 预热理由同 testPostRoutesToRealWorkerThread。
        PkThreadCallQueue::processPendingCalls();
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

void PkThreadCallQueueSelfTest::testFirstPumpDiscardsPreexistingStaleEntries()
{
    // C-1 修复验证（不依赖真实的 std::thread::id 复用——那个不保证发生，
    // 见下面的 …BestEffort 测试）：直接构造"某个线程从未调用过
    // processPendingCalls()，但 registry 里已经有它 id 对应的陈旧条目"这个
    // 前提条件——worker 线程只报到自己的 id（不 pump），主线程用这个 id
    // 提前 post 一次模拟"陈旧调用"，再放行 worker 进入它人生第一次
    // processPendingCalls()。断言：这次调用不会执行那个模拟的陈旧调用，
    // 而是被清空丢弃——这正是修复的核心行为（第一次 pump 先自证清白）。
    std::atomic<bool> workerIdReady{false};
    std::atomic<bool> mayPump{false};
    std::atomic<int> called{0};
    PkThreadId workerId{};
    int firstPumpCount = -1;

    std::thread worker([&]{
        workerId = PkThread::currentThreadId();
        workerIdReady = true;
        while (!mayPump.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        firstPumpCount = PkThreadCallQueue::processPendingCalls();
    });
    while (!workerIdReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

    // 模拟陈旧调用：worker 从未主动触达过队列系统，此刻 registry 里已经有
    // 它 id 对应的条目（由本测试主线程冒充"上一个用过这个 id 的线程的
    // 遗留投递者"）。
    PkThreadCallQueue::post(workerId, [&called]{ called++; });

    mayPump = true;
    worker.join();

    PK_COMPARE(firstPumpCount, 0);
    PK_VERIFY(called.load() == 0);
}

void PkThreadCallQueueSelfTest::testStaleCallNeverExecutedOnReusedThreadIdBestEffort()
{
    // 尽力而为复现 C-1 原始 bug 的真实触发路径：post 给一个已经 join 过的
    // 线程 id，然后依次起新线程调 processPendingCalls()，直到命中系统真的
    // 复用了那个 id（std::thread::id 复用完全由 OS/pthread 实现决定，不
    // 保证在这个测试进程里发生）。修复前：命中复用的第一个新线程会执行
    // 这个陈旧调用；修复后：即使复用发生，陈旧调用也已经在 registry 层面
    // 被丢弃，staleExecuted 恒为 0——真正确定性的正确性断言在上面的
    // testFirstPumpDiscardsPreexistingStaleEntries()，这个测试只是尽力
    // 对真实复现路径也做一次交叉验证。
    PkThreadId deadId{};
    {
        std::thread t([&]{ deadId = PkThread::currentThreadId(); });
        t.join();
    }
    std::atomic<int> staleExecuted{0};
    PkThreadCallQueue::post(deadId, [&staleExecuted]{ staleExecuted++; });

    const int kAttempts = 200;
    bool reused = false;
    for (int i = 0; i < kAttempts && !reused; ++i) {
        PkThreadId newId{};
        int n = -1;
        std::thread t([&]{
            newId = PkThread::currentThreadId();
            n = PkThreadCallQueue::processPendingCalls();
        });
        t.join();
        if (newId == deadId) {
            reused = true;
            PK_COMPARE(n, 0);
        }
    }
    PK_VERIFY(staleExecuted.load() == 0);
    // 不对 reused 做断言——命中与否都不影响上面 staleExecuted==0 这条核心
    // 结论，reused==false 只说明这次运行没有观测到系统复用 id，不是修复失败。
}

void PkThreadCallQueueSelfTest::testPostBlockingWakesEmitterAndRethrowsOnSlotException()
{
    // C-2 修复验证：postBlocking 里 fn() 抛异常，发射线程确实被唤醒（不是
    // 永久挂起——这个测试函数本身能跑到断言那一行，就是"没有挂死在
    // done->acquire() 里"的证据）；本实现选择"重新抛出"语义，断言调用方
    // 确实收到了异常，而不是被吞掉装作成功。
    std::atomic<bool> workerReady{false};
    std::atomic<bool> stop{false};
    PkThreadId workerId{};
    std::thread worker([&]{
        workerId = PkThread::currentThreadId();
        // 预热理由同 testPostRoutesToRealWorkerThread。
        PkThreadCallQueue::processPendingCalls();
        workerReady = true;
        while (!stop.load()) {
            PkThreadCallQueue::processPendingCalls();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    while (!workerReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

    bool caught = false;
    try {
        PkThreadCallQueue::postBlocking(workerId, []{ throw std::runtime_error("boom"); });
    } catch (const std::runtime_error& e) {
        caught = true;
        PK_VERIFY(std::string(e.what()) == "boom");
    }
    PK_VERIFY(caught);

    stop = true;
    worker.join();
}

void PkThreadCallQueueSelfTest::testProcessPendingCallsContinuesAfterExceptionInBatch()
{
    // processPendingCalls() 批量执行时，某一个调用抛异常，验证同批次其余
    // 调用仍然被执行（不是被跳过）。
    PkThreadId me = PkThread::currentThreadId();
    std::atomic<int> before{0};
    std::atomic<int> after{0};
    PkThreadCallQueue::post(me, [&before]{ before++; });
    PkThreadCallQueue::post(me, []{ throw std::runtime_error("mid-batch"); });
    PkThreadCallQueue::post(me, [&after]{ after++; });

    int n = PkThreadCallQueue::processPendingCalls();
    PK_COMPARE(n, 3);
    PK_VERIFY(before.load() == 1);
    PK_VERIFY(after.load() == 1);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_threadqueue.inc"

int run_threadqueue_tests(int argc, char **argv)
{
    PkThreadCallQueueSelfTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
