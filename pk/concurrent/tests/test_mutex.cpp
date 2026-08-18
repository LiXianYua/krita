#include <PkTest.h>
#include "../PkMutex.h"
#include <thread>
#include <vector>

// Simple helper for assertions
bool testsPassed = true;

void test_lock_unlock()
{
    PkMutex m;
    m.lock();
    m.unlock();
    // If we reach here without deadlock, the test passes
}

void test_try_lock()
{
    PkMutex m;
    if (!m.try_lock()) {
        PkTest::qFail("try_lock() should succeed on unlocked mutex", __FILE__, __LINE__);
        testsPassed = false;
        return;
    }
    m.unlock();
}

void test_mutex_locker_raii()
{
    PkMutex m;
    {
        PkMutexLocker locker(&m);
        if (m.try_lock()) {
            m.unlock();
            PkTest::qFail("locker should hold the lock", __FILE__, __LINE__);
            testsPassed = false;
            return;
        }
    }
    if (!m.try_lock()) {
        PkTest::qFail("mutex should be unlocked after locker destroyed", __FILE__, __LINE__);
        testsPassed = false;
        return;
    }
    m.unlock();
}

void test_mutex_locker_unlock_relock()
{
    PkMutex m;
    PkMutexLocker locker(&m);
    locker.unlock();
    if (!m.try_lock()) {
        PkTest::qFail("mutex should be unlocked after locker.unlock()", __FILE__, __LINE__);
        testsPassed = false;
        return;
    }
    m.unlock();
    locker.relock();
    if (m.try_lock()) {
        m.unlock();
        PkTest::qFail("locker.relock() should acquire the lock", __FILE__, __LINE__);
        testsPassed = false;
        return;
    }
}

void test_mutex_locker_mutex_accessor()
{
    PkMutex m;
    PkMutexLocker locker(&m);
    if (locker.mutex() != &m) {
        PkTest::qFail("mutex() accessor should return the original pointer", __FILE__, __LINE__);
        testsPassed = false;
        return;
    }
}

void test_concurrent_increment()
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
    if (counter != 80000) {
        PkTest::qFail("concurrent counter increment failed", __FILE__, __LINE__);
        testsPassed = false;
        return;
    }
}

int run_mutex_tests(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    testsPassed = true;

    test_lock_unlock();
    test_try_lock();
    test_mutex_locker_raii();
    test_mutex_locker_unlock_relock();
    test_mutex_locker_mutex_accessor();
    test_concurrent_increment();

    return testsPassed ? 0 : 1;
}
