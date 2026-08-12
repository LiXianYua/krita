#include "test_backend_concurrency.h"
#include "../PkLogBackend.h"

#include <atomic>
#include <exception>
#include <string>
#include <thread>
#include <vector>

namespace {

// C++17 没有 std::barrier（C++20 才有）。手写一个自旋屏障：每个线程到达后
// 自增计数，忙等直到全部线程都到齐才同时放行——目的是让"第一次使用"真的
// 撞在同一时刻，而不是碰运气般地交错。
class SpinBarrier
{
public:
    explicit SpinBarrier(int total) : _total(total) {}

    void arriveAndWait()
    {
        _arrived.fetch_add(1, std::memory_order_acq_rel);
        while (_arrived.load(std::memory_order_acquire) < _total) {
            std::this_thread::yield();
        }
    }

private:
    std::atomic<int> _arrived{0};
    const int _total;
};

} // namespace

// task-9 缺陷：PkLogEnsureLogger 的 spdlog::get() 落空 →
// spdlog::stderr_color_mt() 建，这两步不是原子的。多个线程第一次用到
// 同一个从未注册过的分类名、又几乎同时撞进来时，都会看到 spdlog::get()
// 落空，都去建同名 logger——spdlog 的 registry 对重名 register 会
// throw spdlog_ex，未捕获的话在工作线程里直接 std::terminate（SIGABRT）。
//
// 这里每个线程内 catch 住异常、计数，把"进程级崩溃"转成可断言的失败——
// 测试本身不应该带崩整个 test_pklog 套件（catch 之后 std::terminate 不会
// 被触发，即使修复前的代码在竞态下确实会抛）。pklog 的使用者对 spdlog
// 一无所知这条设计原则在这里同样适用：只 catch std::exception，不 include
// spdlog 的头。
void PkLogBackendConcurrencyTest::testConcurrentFirstUseOfCategoryDoesNotThrow()
{
    // 整个测试进程里从未出现过的分类名——保证这是它真正的"第一次使用"，
    // 竞态窗口才有意义。
    const std::string categoryName = "pklog.test.concurrency.first_use";

    constexpr int kThreadCount = 16;
    SpinBarrier barrier(kThreadCount);
    std::atomic<int> exceptionCount{0};

    std::vector<std::thread> threads;
    threads.reserve(kThreadCount);
    for (int i = 0; i < kThreadCount; ++i) {
        threads.emplace_back([&barrier, &categoryName, &exceptionCount]() {
            barrier.arriveAndWait();
            try {
                PkLogEnsureLogger(categoryName.c_str(), PkLogDebug);
            } catch (const std::exception &) {
                exceptionCount.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }
    for (auto &t : threads) {
        t.join();
    }

    // 修复前：16 个线程几乎必然撞出至少一次 "logger already exists" 异常。
    // 修复后：PkLogEnsureLogger 内部原子化，0 次异常。
    PK_COMPARE(exceptionCount.load(), 0);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_backend_concurrency.inc"

int run_backend_concurrency_tests(int argc, char **argv)
{
    PkLogBackendConcurrencyTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
