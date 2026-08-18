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
    // 预热（final whole-branch review NEW-I1 修复后必须加）：本测试是这个
    // 二进制里主测试线程第一次真正触达 PkThreadCallQueue 的入口（前面两个
    // 测试是同线程 Direct 分派，压根不进队列；test_cross_thread_auto_
    // defers_to_pump 里预热过的是 worker 线程，不是主线程自己）。NEW-I1
    // 修复把"首次触达清空陈旧条目"的判定从 post() 移到了只有
    // processPendingCalls() 里才会做——下面"emit 触发同线程 post 给自己、
    // 再 processPendingCalls()"这个顺序，如果不先预热，会被
    // processPendingCalls() 自己那次"首次触达"的丢弃逻辑把刚刚排进去的
    // 调用当陈旧条目连带清空（PkThreadCallQueue.h 类头注释"预热"那段讲的
    // 正是这条）。这里用一次空队列上的 no-op processPendingCalls() 提前
    // 把这个一次性判定消耗掉，不影响本测试要验证的"Queued 不立即执行"
    // 这条核心语义。
    PkThreadCallQueue::processPendingCalls();

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

// 6. fix-wave re-review NEW-I3：故意制造 moveToThread() 与 activateSignal
// 读 thread() 之间的并发访问，给 TSan 一个能检测到的并发模式。M-5 把两个
// 跨线程测试的 moveToThread() 从 worker 线程挪回主线程、加了 moveDone
// 屏障之后，恰好消灭了触发 I-1 那条 TSan 报告的并发写模式——I-1 的修复
// （m_thread 改成 std::atomic<PkThreadId>）从此在 run_tsan.sh 里零回归
// 覆盖：把 m_thread 改回非原子类型，run_tsan.sh 依旧干净。这里专门补一个
// *故意*不遵守"receiver 只能从自己所在线程调用 moveToThread()"这条约定的
// 用例——一个线程反复 moveToThread()，另一个线程同时通过 activateSignal
// （真实 emit 路径）反复读 receiver->thread()。
//
// 功能上这条路径本身是安全的（std::atomic<PkThreadId> 保证不会有撕裂读/
// 数据竞争意义上的 UB），所以断言只有一条："能正常跑完、不崩溃"——这个
// 用例存在的唯一目的是给 TSan 一个可检测的并发访问模式，不是验证某个
// 具体的调度结果（两个线程的 id 几乎不可能相等，activateSignal 因此几乎
// 总是选 Queued 分支，槽函数是否真的被执行不是本用例关心的事）。
static void test_concurrent_moveToThread_and_activateSignal_read_is_safe()
{
    ThreadSender s;
    ThreadReceiver r;
    PkObject::connect(&s, &ThreadSender::sig, &r, &ThreadReceiver::onSig);

    std::atomic<bool> stop{false};
    std::thread mover([&]{
        PkThreadId self = PkThread::currentThreadId();
        while (!stop.load()) {
            r.moveToThread(self);
        }
    });
    std::thread emitter([&]{
        while (!stop.load()) {
            s.sig();
        }
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    stop = true;
    mover.join();
    emitter.join();

    _expect(true, "concurrent moveToThread()/activateSignal thread() read "
                  "completes without crashing (TSan regression probe for "
                  "I-1's atomic m_thread fix)");
}

void run_thread_tests()
{
    test_same_thread_auto_is_direct();
    test_same_thread_unique_dispatches_like_auto();
    test_cross_thread_auto_defers_to_pump();
    test_same_thread_explicit_queued_defers();
    test_cross_thread_blocking_queued_waits();
    test_disconnected_queued_call_is_dropped();
    test_concurrent_moveToThread_and_activateSignal_read_is_safe();
}
