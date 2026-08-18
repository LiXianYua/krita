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
    //
    // 预热（final whole-branch review NEW-I1 修复后必须加）：这是本测试
    // 文件里主测试线程第一次真正触达 PkThreadCallQueue 的入口。NEW-I1
    // 修复把"首次触达清空陈旧条目"的判定从 post() 移到了只有
    // processPendingCalls() 里才会做——所以下面"先 post 给自己、再
    // processPendingCalls()"这个顺序，如果不先预热，会被 processPendingCalls()
    // 自己那次"首次触达"的丢弃逻辑把刚刚 post 进去的条目当陈旧条目连带
    // 清空（PkThreadCallQueue.h 类头注释里"预热"那段讲的正是这条）。这里
    // 用一次空队列上的 no-op processPendingCalls() 提前把这个一次性判定
    // 消耗掉，不影响本测试要验证的"post 之后不会立即执行"这条核心语义。
    PkThreadCallQueue::processPendingCalls();

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

void PkThreadCallQueueSelfTest::testPostBlockingWakesAndReportsAbandonedWhenDiscardedByFirstPump()
{
    // fix-wave re-review NEW-C1 复现①：postBlocking 投给一个还没预热过的
    // 线程，在它触达之前，模拟"这个线程的这次触达把陈旧条目清空"这个动作
    // 发生（C-1 的核心行为，见 testFirstPumpDiscardsPreexistingStaleEntries）
    // ——验证发射线程被正确唤醒（不是永久挂起），且能感知到调用被丢弃而
    // 不是正常执行。
    PkThreadId workerId{};
    std::atomic<bool> workerIdReady{false};
    std::atomic<bool> mayPump{false};

    std::thread worker([&]{
        workerId = PkThread::currentThreadId();
        workerIdReady = true;
        while (!mayPump.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        // worker 的第一次触达：C-1 的丢弃逻辑会把此刻已经排队的调用
        // （下面 postBlocking 投的那次）当成陈旧条目清空，不会执行它。
        PkThreadCallQueue::processPendingCalls();
    });
    while (!workerIdReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

    std::atomic<int> called{0};
    std::atomic<bool> caughtAbandoned{false};
    std::atomic<bool> emitterReturned{false};
    std::thread emitter([&]{
        try {
            PkThreadCallQueue::postBlocking(workerId, [&called]{ called++; });
        } catch (const PkCallAbandonedException&) {
            caughtAbandoned = true;
        }
        emitterReturned = true;
    });

    // 给 postBlocking 一点时间把闭包真正排进 worker 的队列，再放行 worker
    // 触达——制造"闭包已在队列里、worker 还没触达过"这个前提窗口。
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    mayPump = true;

    // 发射线程必须被正确唤醒，不能永久挂起——用带上限的轮询代替裸
    // join()：如果修复失效（回归到永久挂起），本用例在这里超时失败，
    // 而不是把整个测试进程一起拖死（emitter 线程随后 detach，不再 join
    // 一个可能永久阻塞的线程）。
    for (int i = 0; i < 500 && !emitterReturned.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!emitterReturned.load()) {
        emitter.detach();
        worker.join();
        PK_FAIL("postBlocking never woke the emitting thread after the call "
                 "was discarded on the target's first touch (still hanging "
                 "after 5s)");
        return;
    }
    emitter.join();
    worker.join();

    PK_VERIFY(caughtAbandoned.load());
    PK_VERIFY(called.load() == 0);
}

void PkThreadCallQueueSelfTest::testPostBlockingWakesWhenTargetThreadExitsWithoutPumping()
{
    // fix-wave re-review NEW-C1 复现②：worker 线程已经预热过、正常处于
    // pump 循环里，但在收到最后一次 postBlocking 投递之后就直接退出，
    // 再也没有机会调用 processPendingCalls() 把它排干——验证发射线程被
    // 唤醒（不是永久卡住）。走的是 ThreadRegistryGuard 析构时的
    // erase()，跟①"首次触达"那次构造时的 erase() 是同一套丢弃感知机制，
    // 二者共用 postBlocking() 里同一个 releaseOnDestroy。
    PkThreadId workerId{};
    std::atomic<bool> workerReady{false};
    std::atomic<bool> mayExit{false};

    std::thread worker([&]{
        workerId = PkThread::currentThreadId();
        // 预热 + 建立"这个线程活跃过"的既成事实，之后不再 pump，直接等
        // 信号退出——模拟"收到最后一条投递之前就已经决定不再消费队列"的
        // 收尾路径。
        PkThreadCallQueue::processPendingCalls();
        workerReady = true;
        while (!mayExit.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    });
    while (!workerReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

    std::atomic<int> called{0};
    std::atomic<bool> caughtAbandoned{false};
    std::atomic<bool> emitterReturned{false};
    std::thread emitter([&]{
        try {
            PkThreadCallQueue::postBlocking(workerId, [&called]{ called++; });
        } catch (const PkCallAbandonedException&) {
            caughtAbandoned = true;
        }
        emitterReturned = true;
    });

    // 给 postBlocking 一点时间把闭包排进 worker 的队列，再放行 worker
    // 退出——制造"闭包已经在队列里、worker 从此再也不会去读它"这个前提
    // 窗口。
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    mayExit = true;
    worker.join();

    for (int i = 0; i < 500 && !emitterReturned.load(); ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!emitterReturned.load()) {
        emitter.detach();
        PK_FAIL("postBlocking never woke the emitting thread after the "
                 "target thread exited with the call still queued (still "
                 "hanging after 5s)");
        return;
    }
    emitter.join();

    PK_VERIFY(caughtAbandoned.load());
    PK_VERIFY(called.load() == 0);
}

void PkThreadCallQueueSelfTest::testPostDoesNotDiscardOwnInboundQueueOnOutboundPost()
{
    // fix-wave re-review NEW-I1 复现：post() 曾经错误地对"调用者自己的
    // 线程 id"做首次触达判定——线程 T 从未触达过队列系统，此刻别人已经
    // 合法投给它一条调用；T 的第一个动作是往别的线程 post()（这正是
    // activateSignal 做跨线程 queued 投递时会做的事）。修复前这一步会把
    // T 自己的入站队列当成陈旧条目清空；修复后不会。
    //
    // 用 pendingCount()（只读，不触发任何丢弃逻辑）直接观察 T 自己队列的
    // 大小，刻意绕开"processPendingCalls() 首次触达本身也会丢弃陈旧条目"
    // 这条独立的、必须靠"预热"规避的既有 C-1 行为——那条要求预热才能
    // 保证送达，跟 NEW-I1 是两件不同的事，混进同一个断言会让这个用例既
    // 测不准 NEW-I1、也测不准 C-1（见 PkThreadCallQueue.h 类头注释"预热"
    // 那段）。
    PkThreadId mainId = PkThread::currentThreadId();
    std::atomic<bool> workerIdReady{false};
    std::atomic<bool> entryQueued{false};
    PkThreadId workerId{};
    std::size_t observedPendingAfterOutboundPost = 999;

    std::thread worker([&]{
        workerId = PkThread::currentThreadId();
        workerIdReady = true;
        while (!entryQueued.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        // worker 从未触达过队列系统。它的第一个动作是往别的线程（主测试
        // 线程）post()。
        PkThreadCallQueue::post(mainId, []{});
        // 只读观察，不触发任何丢弃逻辑。
        observedPendingAfterOutboundPost = PkThreadCallQueue::pendingCount();
        // 清理：这是 worker 第一次真正调用 processPendingCalls()，按 C-1
        // 既有语义会把此刻队列里的条目（主线程投的那条 no-op）当陈旧条目
        // 一并清空——这正是"未预热"必须付出的代价（见 PkThreadCallQueue.h
        // 类头注释"预热"那段），跟本用例要验证的 NEW-I1 无关，这里只是
        // 借它把 registry 里 workerId 名下的残留条目清掉，不留垃圾给后续
        // 用例；上面的断言已经在这一步之前完成采样，不受影响。
        PkThreadCallQueue::processPendingCalls();
    });
    while (!workerIdReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

    // 别人合法投给 worker 的入站条目，在 worker 碰队列系统之前已经排进去。
    PkThreadCallQueue::post(workerId, []{});
    entryQueued = true;

    worker.join();

    PK_COMPARE(observedPendingAfterOutboundPost, static_cast<std::size_t>(1));

    // 清理 worker 投给主线程自己的那条 no-op 调用，不留垃圾给后续用例。
    PkThreadCallQueue::processPendingCalls();
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_threadqueue.inc"

int run_threadqueue_tests(int argc, char **argv)
{
    PkThreadCallQueueSelfTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
