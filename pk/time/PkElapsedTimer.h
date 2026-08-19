#ifndef PK_TIME_PKELAPSEDTIMER_H
#define PK_TIME_PKELAPSEDTIMER_H

#include <chrono>
#include <cstdint>

// QElapsedTimer 的 std::chrono::steady_clock 对应物。R-16 Task 1（`pk/time`）。
//
// API 面严格按 docs/superpowers/plans/R-16.md「真实 API 用量表」QElapsedTimer
// 那张表——保留范围内只有这 6 个 API 有真实调用点：
//   start() / restart() / elapsed() / nsecsElapsed() / isValid() / invalidate()
// （`hasExpired()`/`msecsTo()`/`secsTo()`/`msecsSinceReference()`/比较运算符/
// `clockType()`/`isMonotonic()` 零调用点，判据①不做。）
//
// 语义按同文档「探针结果」实测钉死（探针原始输出见
// docs/superpowers/plans/R-16-probe/probe_time_output.txt）：
//   - 默认构造 isValid()==false
//     （探针：`QElapsedTimer() isValid before start() = false`）
//   - start() 之后 isValid()==true，elapsed() 立即约 0
//     （探针：`after start(): isValid = true` / `elapsed() right after start() ... = 0`）
//   - 真实睡眠 20ms 后 elapsed()==20（ms 量级）、nsecsElapsed()==20081563（ns 量级，
//     与 elapsed()*1e6 同数量级，精度更细）
//     （探针：`elapsed() after 20ms sleep = 20` / `nsecsElapsed() after 20ms sleep (ns) = 20081563`）
//   - restart() 真的把计时基点归零，不是仅返回旧的 elapsed 值而不重置
//     （探针：`elapsed() right after restart() following 5ms sleep (should be ~0, not ~5) = 0`）
//   - invalidate() 之后 isValid()==false
//     （探针：`after invalidate(): isValid = false`）
//
// 选用 std::chrono::steady_clock 而不是 system_clock：QElapsedTimer 文档要求的
// "不受系统时间调整影响的单调计时"正是 steady_clock 的契约（system_clock 用于
// PkDateTime，是 R-16 Task 2 的事，不在本类型范围）。
//
// 哨兵设计：不额外放一个 bool 有效位，直接借用 std::chrono 自己的
// TimePoint::min() 作"从未 start() / 已 invalidate()"的哨兵值——与真 Qt
// QElapsedTimer 内部用 t1=t2=0x8000000000000000 作哨兵是同一种思路（见
// qelapsedtimer.h:59-63），只是不用手写魔法常量，借用标准库已经提供的
// "该类型能表示的最小值"。
class PkElapsedTimer
{
public:
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    constexpr PkElapsedTimer() noexcept : m_start(TimePoint::min()) {}

    // 把计时基点设为"现在"，使 isValid() 变 true、elapsed() 从这一刻起计数。
    void start() noexcept { m_start = Clock::now(); }

    // 语义同 QElapsedTimer::restart()：返回"重置前的 elapsed()"（毫秒），
    // 同时真的把计时基点挪到"现在"（归零逻辑——这是本类型的变异注入点之一：
    // 如果这一行被漏掉或改成条件执行，restart() 之后 elapsed() 会残留旧值
    // 而不是归零，被 tests/test_elapsed_timer.cpp 的 restartResetsElapsedToZero
    // 用例捕获）。
    std::int64_t restart() noexcept
    {
        const std::int64_t result = elapsed();
        m_start = Clock::now();
        return result;
    }

    // elapsed() 由 nsecsElapsed() 除以纳秒/毫秒换算系数 1'000'000 得到，而不是
    // 各自独立调用 duration_cast<milliseconds>——这样"elapsed() ≈ nsecsElapsed()
    // 数量级差 1e6"这条关系在实现里就是硬约束，不会因为两条 duration_cast 各自
    // 取整方式不同而漂移。这个 1'000'000 是本类型的变异注入点之一：换成错误的
    // 系数（例如 1000，少三个数量级）会被
    // tests/test_elapsed_timer.cpp 的 nsecsToMillisecondsConversionRelation
    // 用例捕获。
    std::int64_t elapsed() const noexcept
    {
        return nsecsElapsed() / 1000000;
    }

    std::int64_t nsecsElapsed() const noexcept
    {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - m_start).count();
    }

    // 哨兵判定：m_start 是否仍处在"从未 start() / 已 invalidate()"的哨兵值
    // TimePoint::min()。这是本类型的变异注入点之一：把 `!=` 改成 `==`（或反过来
    // 让 invalidate() 之后仍判 true）会被
    // tests/test_elapsed_timer.cpp 的 defaultConstructedIsInvalid /
    // invalidateMakesInvalid 两个用例捕获。
    bool isValid() const noexcept { return m_start != TimePoint::min(); }

    void invalidate() noexcept { m_start = TimePoint::min(); }

private:
    TimePoint m_start;
};

#endif // PK_TIME_PKELAPSEDTIMER_H
