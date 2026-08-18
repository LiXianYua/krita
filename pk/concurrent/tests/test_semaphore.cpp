#include "test_semaphore.h"
#include "../PkSemaphore.h"
#include <atomic>
#include <chrono>
#include <thread>

void PkSemaphoreSelfTest::testAcquireReleaseBlocks()
{
    // 复现 kis_tile_data_pooler.cc 里 acquire() 无参、阻塞版的语义：
    // count 耗尽后第二个 acquire() 必须真的阻塞，release() 之后才能被唤醒——
    // 不是"调用返回了什么"，是"另一个线程真的被挡住了"。
    PkSemaphore sem(1);
    sem.acquire(); // count 1 -> 0

    std::atomic<bool> acquired{false};
    std::thread t([&] {
        sem.acquire(); // 应阻塞，直到主线程 release()
        acquired = true;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    PK_VERIFY(!acquired.load());

    sem.release(); // 唤醒 t
    t.join();
    PK_VERIFY(acquired.load());
}

void PkSemaphoreSelfTest::testTryAcquireImmediate()
{
    // timeoutMs == 0：立即返回，不等待。
    PkSemaphore sem(0);
    PK_VERIFY(!sem.tryAcquire(1, 0));
    sem.release();
    PK_VERIFY(sem.tryAcquire(1, 0));
}

void PkSemaphoreSelfTest::testTryAcquireTimeout()
{
    // timeoutMs > 0 且 count 一直不够：证明它真的等了至少这么久才超时返回，
    // 不是立刻返回 false。
    PkSemaphore sem(0);
    auto start = std::chrono::steady_clock::now();
    bool ok = sem.tryAcquire(1, 50);
    auto elapsed = std::chrono::steady_clock::now() - start;
    PK_VERIFY(!ok);
    PK_VERIFY(elapsed >= std::chrono::milliseconds(45)); // 允许少量调度误差
}

void PkSemaphoreSelfTest::testTryAcquireNegativeTimeoutBlocksForever()
{
    // 复现真实调用点 kis_tile_data_swapper.cpp:75 的
    // tryAcquire(1, TIMEOUT)，TIMEOUT == -1，Qt 语义等价 acquire()：
    // 负超时无限等待，release() 之前不能返回。
    PkSemaphore sem(0);
    std::atomic<bool> acquired{false};
    std::thread t([&] {
        bool ok = sem.tryAcquire(1, -1);
        acquired = ok;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    PK_VERIFY(!acquired.load());

    sem.release();
    t.join();
    PK_VERIFY(acquired.load());
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_semaphore.inc"

int run_semaphore_tests(int argc, char **argv)
{
    PkSemaphoreSelfTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
