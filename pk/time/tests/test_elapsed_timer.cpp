#include "test_elapsed_timer.h"
#include "../PkElapsedTimer.h"

#include <chrono>
#include <thread>

namespace {
void sleepMs(int ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
} // namespace

void TestElapsedTimer::defaultConstructedIsInvalid()
{
    // 探针：`QElapsedTimer() isValid before start() = false`
    PkElapsedTimer t;
    PK_VERIFY(!t.isValid());
}

void TestElapsedTimer::startMakesValidAndElapsedNearZero()
{
    // 探针：`after start(): isValid = true`
    // 探针：`elapsed() right after start() (ms, should be tiny) = 0`
    PkElapsedTimer t;
    t.start();
    PK_VERIFY(t.isValid());
    PK_VERIFY(t.elapsed() >= 0);
    PK_VERIFY(t.elapsed() < 50); // 冗余防抖动，探针实测值是 0
}

void TestElapsedTimer::elapsedIsMonotonicAfterRealSleep()
{
    // 覆盖 "start/elapsed 单调性（真实 sleep 前后）"：睡眠前后两次采样，
    // 后一次必须严格更大——不是原地不动，也不会倒退。
    // 探针：`elapsed() after 20ms sleep = 20`
    PkElapsedTimer t;
    t.start();
    const std::int64_t before = t.elapsed();
    sleepMs(20);
    const std::int64_t after = t.elapsed();
    PK_VERIFY(after > before);
    PK_VERIFY(after >= 15); // 冗余防抖动，探针实测值是 20
}

void TestElapsedTimer::nsecsElapsedGrowsAfterRealSleep()
{
    // 探针：`nsecsElapsed() after 20ms sleep (ns) = 20081563`
    PkElapsedTimer t;
    t.start();
    const std::int64_t before = t.nsecsElapsed();
    sleepMs(20);
    const std::int64_t after = t.nsecsElapsed();
    PK_VERIFY(after > before);
    PK_VERIFY(after >= 15000000); // 15ms 折算成 ns 的下界，冗余防抖动
}

void TestElapsedTimer::nsecsToMillisecondsConversionRelation()
{
    // 纳秒/毫秒换算关系：ns 取样发生在 ms 取样之后（同线程顺序执行），
    // 单调时钟下只会更大不会更小；两者的数量级关系是 ns ≈ ms * 1e6
    // （探针：20ms 睡眠后 elapsed()==20、nsecsElapsed()==20081563，两者一致）。
    // 上界给 5ms 冗余，盖住两次取样之间的真实时间差 + 取整误差。
    PkElapsedTimer t;
    t.start();
    sleepMs(20);
    const std::int64_t ms = t.elapsed();
    const std::int64_t ns = t.nsecsElapsed();
    PK_VERIFY(ms >= 15);
    PK_VERIFY(ns >= ms * 1000000);
    PK_VERIFY(ns < (ms + 5) * 1000000);
}

void TestElapsedTimer::restartResetsElapsedToZero()
{
    // 探针：`elapsed() right after restart() following 5ms sleep
    //         (should be ~0, not ~5) = 0`——确认 restart() 真的把计时基点归零，
    // 不是仅返回旧值但不重置。
    PkElapsedTimer t;
    t.start();
    sleepMs(5);
    const std::int64_t previousElapsed = t.restart();
    PK_VERIFY(previousElapsed >= 3); // 重置前 elapsed() 的下界，冗余防抖动
    PK_VERIFY(t.elapsed() < 5);      // 归零后立刻取值，必须远小于 5（不是残留的 ~5）
}

void TestElapsedTimer::invalidateMakesInvalid()
{
    // 探针：`after invalidate(): isValid = false`
    PkElapsedTimer t;
    t.start();
    PK_VERIFY(t.isValid());
    t.invalidate();
    PK_VERIFY(!t.isValid());
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_elapsed_timer.inc"

int run_elapsed_timer_tests(int argc, char **argv)
{
    TestElapsedTimer tc;
    return PkTest::qExec(&tc, argc, argv);
}
