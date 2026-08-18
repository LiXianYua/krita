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

// 2. 跨线程 Auto：emit 后不会立即执行，要接收方线程 pump 才执行（对齐探针实验2）
static void test_cross_thread_auto_defers_to_pump()
{
    ThreadSender s;
    ThreadReceiver r;
    std::atomic<bool> workerReady{false};
    std::atomic<bool> stop{false};
    std::thread worker([&]{
        r.moveToThread(PkThread::currentThreadId());
        workerReady = true;
        while (!stop.load()) {
            PkThreadCallQueue::processPendingCalls();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    while (!workerReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

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
    std::atomic<bool> workerReady{false};
    std::atomic<bool> stop{false};
    std::thread worker([&]{
        r.moveToThread(PkThread::currentThreadId());
        workerReady = true;
        while (!stop.load()) {
            PkThreadCallQueue::processPendingCalls();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });
    while (!workerReady.load()) { std::this_thread::sleep_for(std::chrono::milliseconds(1)); }

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
    test_cross_thread_auto_defers_to_pump();
    test_same_thread_explicit_queued_defers();
    test_cross_thread_blocking_queued_waits();
    test_disconnected_queued_call_is_dropped();
}
