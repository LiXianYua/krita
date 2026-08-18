#include "test_mutex.h"
#include "../PkMutex.h"
#include <thread>
#include <vector>
#include <atomic>

void TestMutex::lockUnlock()
{
    PkMutex m;
    m.lock();
    m.unlock();
    // If we reach here without deadlock, the test passes
}

void TestMutex::tryLock()
{
    PkMutex m;
    PK_VERIFY(m.try_lock());
    m.unlock();
}

void TestMutex::tryLockCamelCase()
{
    // 真实调用点 libs/image/kis_image_animation_interface.cpp:499 用驼峰式
    // tryLock()（Qt 语义），验证成功/失败两种情况，正面证明它是
    // try_lock() 的转发，而不是一个空壳。
    PkMutex m;
    PK_VERIFY(m.tryLock());
    m.unlock();

    std::atomic<bool> contenderSucceeded{false};
    {
        PkMutexLocker locker(&m);
        std::thread t([&]{
            contenderSucceeded.store(m.tryLock());
        });
        t.join();
    }
    PK_VERIFY(!contenderSucceeded.load());
}

void TestMutex::mutexLockerRaii()
{
    PkMutex m;
    {
        PkMutexLocker locker(&m);
        PK_VERIFY(!m.try_lock());
    }
    PK_VERIFY(m.try_lock());
    m.unlock();
}

void TestMutex::mutexLockerUnlockRelock()
{
    PkMutex m;
    PkMutexLocker locker(&m);
    locker.unlock();
    PK_VERIFY(m.try_lock());
    m.unlock();
    locker.relock();
    PK_VERIFY(!m.try_lock());
}

void TestMutex::mutexLockerMutexAccessor()
{
    PkMutex m;
    PkMutexLocker locker(&m);
    PK_COMPARE(locker.mutex(), &m);
}

void TestMutex::concurrentIncrement()
{
    PkMutex m;
    int counter = 0;
    std::vector<std::thread> threads;
    for (int i = 0; i < 8; ++i) {
        threads.emplace_back([&]{
            for (int j = 0; j < 10000; ++j) {
                PkMutexLocker locker(&m);
                ++counter;
            }
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    PK_COMPARE(counter, 80000);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_mutex.inc"

int run_mutex_tests(int argc, char **argv)
{
    TestMutex tc;
    return PkTest::qExec(&tc, argc, argv);
}
