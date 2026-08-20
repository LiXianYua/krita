#include "test_timer.h"
#include "PkTimer.h"
#include "PkThreadCallQueue.h"
#include <atomic>
#include <chrono>
#include <thread>

using namespace std::chrono_literals;

void TestTimer::callbackWaitsForExplicitPump()
{
    PkThreadCallQueue::warmUpCurrentThread();
    std::atomic<int> calls{0};
    PkTimer timer;
    timer.start(5ms, [&] { ++calls; }, true);
    std::this_thread::sleep_for(20ms);
    PK_COMPARE(calls.load(), 0);
    PK_VERIFY(PkThreadCallQueue::processPendingCalls() >= 1);
    PK_COMPARE(calls.load(), 1);
}

void TestTimer::stopAndDestructionCancelCallbacks()
{
    std::atomic<int> calls{0};
    {
        PkTimer timer;
        timer.start(30ms, [&] { ++calls; });
        timer.stop();
    }
    std::this_thread::sleep_for(40ms);
    PkThreadCallQueue::processPendingCalls();
    PK_COMPARE(calls.load(), 0);
}

void TestTimer::repeatingTimerPostsMoreThanOnce()
{
    std::atomic<int> calls{0};
    PkTimer timer;
    timer.start(5ms, [&] { ++calls; });
    std::this_thread::sleep_for(50ms);
    PkThreadCallQueue::processPendingCalls();
    timer.stop();
    const int stoppedCount = calls.load();
    PK_VERIFY(stoppedCount >= 2);
    std::this_thread::sleep_for(20ms);
    PkThreadCallQueue::processPendingCalls();
    PK_COMPARE(calls.load(), stoppedCount);
}

void TestTimer::zeroIntervalRepeatingTimerKeepsOneCallbackOutstanding()
{
    PkThreadCallQueue::warmUpCurrentThread();
    std::atomic<int> calls{0};
    PkTimer timer;
    timer.start(0ms, [&] { ++calls; });
    std::this_thread::sleep_for(2ms);
    PK_COMPARE(PkThreadCallQueue::pendingCount(), std::size_t(1));
    PK_COMPARE(PkThreadCallQueue::processPendingCalls(), 1);
    PK_COMPARE(calls.load(), 1);
    std::this_thread::sleep_for(2ms);
    PK_COMPARE(PkThreadCallQueue::pendingCount(), std::size_t(1));
    timer.stop();
    PkThreadCallQueue::processPendingCalls();
    PK_COMPARE(calls.load(), 1);
}

void TestTimer::negativeIntervalRepeatingTimerClampsToZero()
{
    PkThreadCallQueue::warmUpCurrentThread();
    std::atomic<int> calls{0};
    PkTimer timer;
    timer.start(-5ms, [&] { ++calls; });
    std::this_thread::sleep_for(2ms);
    PK_COMPARE(PkThreadCallQueue::pendingCount(), std::size_t(1));
    PK_COMPARE(PkThreadCallQueue::processPendingCalls(), 1);
    PK_COMPARE(calls.load(), 1);
    std::this_thread::sleep_for(2ms);
    PK_COMPARE(PkThreadCallQueue::pendingCount(), std::size_t(1));
    timer.stop();
    PkThreadCallQueue::processPendingCalls();
    PK_COMPARE(calls.load(), 1);
}

void TestTimer::warmedTargetThreadReceivesCallback()
{
    std::atomic<bool> ready{false};
    std::atomic<bool> pump{false};
    std::atomic<bool> done{false};
    std::atomic<bool> onTarget{false};
    PkThreadId target;
    std::thread receiver([&] {
        PkThreadCallQueue::warmUpCurrentThread();
        target = PkThread::currentThreadId();
        ready = true;
        while (!pump.load()) std::this_thread::yield();
        PkThreadCallQueue::processPendingCalls();
        done = true;
    });
    while (!ready.load()) std::this_thread::yield();
    PkTimer timer(target);
    timer.start(5ms, [&] { onTarget = PkThread::currentThreadId() == target; }, true);
    std::this_thread::sleep_for(20ms);
    pump = true;
    receiver.join();
    PK_VERIFY(done.load());
    PK_VERIFY(onTarget.load());
}

void TestTimer::queuedCallbackIsCancelledByStopAndDestruction()
{
    PkThreadCallQueue::warmUpCurrentThread();
    std::atomic<int> calls{0};
    {
        PkTimer timer;
        timer.start(5ms, [&] { ++calls; }, true);
        std::this_thread::sleep_for(20ms); // callback is already in the queue
        timer.stop();
    }
    PkThreadCallQueue::processPendingCalls();
    PK_COMPARE(calls.load(), 0);
}

void TestTimer::queuedCallbackIsCancelledByDestructionAlone()
{
    PkThreadCallQueue::warmUpCurrentThread();
    std::atomic<int> calls{0};
    {
        PkTimer timer;
        timer.start(5ms, [&] { ++calls; }, true);
        std::this_thread::sleep_for(20ms); // callback is already in the queue
    } // destruction, without an explicit stop(), invalidates queued delivery
    PkThreadCallQueue::processPendingCalls();
    PK_COMPARE(calls.load(), 0);
}

#include "pk_binder_test_timer.inc"

int run_timer_tests(int argc, char **argv)
{
    TestTimer tc;
    return PkTest::qExec(&tc, argc, argv);
}
