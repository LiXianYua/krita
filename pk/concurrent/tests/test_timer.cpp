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
    std::this_thread::sleep_for(25ms);
    PkThreadCallQueue::processPendingCalls();
    timer.stop();
    PK_VERIFY(calls.load() >= 2);
}

#include "pk_binder_test_timer.inc"

int run_timer_tests(int argc, char **argv)
{
    TestTimer tc;
    return PkTest::qExec(&tc, argc, argv);
}
