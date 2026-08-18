#include "../PkObject.h"
#include "../PkConnect.h"
#include "test_util.h"
#include "../../concurrent/PkThread.h"
#include "../../concurrent/PkThreadCallQueue.h"
#include <thread>
#include <chrono>
#include <atomic>

namespace {
struct ThreadSender : PkObject {
    void sig() { PkObject::activateSignal(this, PkMemberFnKey::from(&ThreadSender::sig)); }
};
struct ThreadReceiver : PkObject {
    std::atomic<int> got{0};
    PkThreadId execThread{};
    void onSig() { got++; execThread = PkThread::currentThreadId(); }
};
}

// 1. 同线程 Auto：立即同步执行（对齐探针实验1，回归既有行为）
static void test_same_thread_auto_is_direct()
{
    ThreadSender s; ThreadReceiver r;
    PkObject::connect(&s, &ThreadSender::sig, &r, &ThreadReceiver::onSig);
    s.sig();
    _expect(r.got.load() == 1, "same-thread Auto connection executes synchronously");
}

// 1b. 同线程 Unique：dispatch 与 Auto 等价（设计决定 1）——connect 期用
// PkConnectionType::Unique 建连接，emit 期立即同步执行，行为跟 test 1 一致。
// 只补同线程场景：Unique 的核心是"连接期去重"，dispatch 等价 Auto 这一点
// 用一个场景验证即可，不需要把 5 个探针场景对 Unique 全部重跑一遍。
static void test_same_thread_unique_dispatches_like_auto()
{
    ThreadSender s; ThreadReceiver r;
    PkObject::connect(&s, &ThreadSender::sig, &r, &ThreadReceiver::onSig, PkConnectionType::Unique);
    s.sig();
    _expect(r.got.load() == 1, "same-thread Unique connection dispatches like Auto (executes synchronously)");
}

// 2. 跨线程 Auto：emit 后不会立即执行，要接收方线程 pump 才执行（对齐探针实验2）
static void test_cross_thread_auto_defers_to_pump()
{
    ThreadSender s;
    ThreadReceiver r;
    std::atomic<bool> workerIdReady{false};
    std::atomic<bool> moveDone{false};
    std::atomic<bool> stop{false};
    PkThreadId workerId{};
    std::thread worker([&]{
        // 只报到（拿到自己的线程 id），还不碰 r——`r` 是主线程构造的对象，
        // `moveToThread()` 只应该从它*当前*所在的线程调用（PkObject.h 自己
        // 文档规定的契约，M-5：修复前这里由 worker 线程越权调用，违反了
        // 这条契约本身，靠 workerReady 的 release/acquire 侥幸安全）。
        workerId = PkThread::currentThreadId();
        // 先"预热"一次 processPendingCalls()：建立本线程在 registry 里的
        // C-1 修复"已触达"标记。此刻外界还不知道 workerId（下面才置
        // workerIdReady），队列必然为空，预热是无害 no-op。这一步必须严格
        // 发生在 workerIdReady=true 之前——否则主线程后面的 post() 与本
        // 线程"第一次调用 processPendingCalls()"之间会有真实竞争：C-1 修
        // 复里"线程首次 pump 丢弃陈旧条目"的逻辑可能把主线程马上要投递的
        // 合法调用当成陈旧条目丢掉（这正是加上这段预热前，本测试在 M-5
        // 重排等待顺序后实测暴露出的真实失败，不是假设）。
        PkThreadCallQueue::processPendingCalls();
        workerIdReady = true;
        while (!moveDone.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        while (!stop.load()) {
            PkThreadCallQueue::processPendingCalls();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    while (!workerIdReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    // 主线程是 r 当前所在的线程，由它自己调用 moveToThread()，遵守契约。
    r.moveToThread(workerId);
    moveDone = true;

    PkObject::connect(&s, &ThreadSender::sig, &r, &ThreadReceiver::onSig);
    s.sig();
    _expect(r.got.load() == 0, "cross-thread Auto connection does not execute immediately");

    for (int i = 0; i < 200 && r.got.load() == 0; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    _expect(r.got.load() == 1, "cross-thread Auto connection executes once the receiver thread pumps");
    _expect(r.execThread == PkThread::currentThreadId() ? false : true,
            "slot ran on the receiver's thread, not the emitting thread");
    stop = true;
    worker.join();
}

// 3. 显式 Queued 即使同线程也不立即执行（对齐探针实验6）
static void test_same_thread_explicit_queued_defers()
{
    ThreadSender s; ThreadReceiver r;
    PkObject::connect(&s, &ThreadSender::sig, &r, &ThreadReceiver::onSig, PkConnectionType::Queued);
    s.sig();
    _expect(r.got.load() == 0, "explicit Queued connection does not execute immediately even same-thread");
    int n = PkThreadCallQueue::processPendingCalls();
    _expect(n == 1, "processPendingCalls drains the queued call");
    _expect(r.got.load() == 1, "queued call executed after explicit pump");
}

// 4. 跨线程 BlockingQueued：emit 阻塞直到目标线程执行完（对齐探针实验3）
static void test_cross_thread_blocking_queued_waits()
{
    ThreadSender s;
    ThreadReceiver r;
    std::atomic<bool> workerIdReady{false};
    std::atomic<bool> moveDone{false};
    std::atomic<bool> stop{false};
    PkThreadId workerId{};
    std::thread worker([&]{
        // 同 test_cross_thread_auto_defers_to_pump 的 M-5 修复：只报到，
        // 不越权调用 moveToThread()；同样先预热一次 processPendingCalls()
        // 再置 workerIdReady，理由见那个测试里的注释（C-1 修复引入的
        // "首次 pump 丢弃陈旧条目"与后面主线程的 post() 之间的竞争）。
        workerId = PkThread::currentThreadId();
        PkThreadCallQueue::processPendingCalls();
        workerIdReady = true;
        while (!moveDone.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
        while (!stop.load()) {
            PkThreadCallQueue::processPendingCalls();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    while (!workerIdReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }
    r.moveToThread(workerId);
    moveDone = true;

    PkObject::connect(&s, &ThreadSender::sig, &r, &ThreadReceiver::onSig, PkConnectionType::BlockingQueued);
    s.sig();
    _expect(r.got.load() == 1, "BlockingQueued emit returns only after the target thread executed the slot");

    stop = true;
    worker.join();
}

// 5. disconnect 之后再 pump：已排队但已断开的调用静默丢弃（不执行、不崩溃）
static void test_disconnected_queued_call_is_dropped()
{
    ThreadSender s; ThreadReceiver r;
    PkConnection c = PkObject::connect(&s, &ThreadSender::sig, &r, &ThreadReceiver::onSig, PkConnectionType::Queued);
    s.sig();
    PkObject::disconnect(c);
    int n = PkThreadCallQueue::processPendingCalls();
    _expect(n == 1, "processPendingCalls still dequeues the entry (count reflects dequeued, not delivered)");
    _expect(r.got.load() == 0, "disconnected-before-pump call is silently dropped, slot not invoked");
}

void run_thread_tests()
{
    test_same_thread_auto_is_direct();
    test_same_thread_unique_dispatches_like_auto();
    test_cross_thread_auto_defers_to_pump();
    test_same_thread_explicit_queued_defers();
    test_cross_thread_blocking_queued_waits();
    test_disconnected_queued_call_is_dropped();
}
