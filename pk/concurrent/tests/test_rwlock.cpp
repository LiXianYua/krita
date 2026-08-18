#include <PkTest.h>
#include "../PkReadWriteLock.h"
#include <thread>
#include <vector>
#include <atomic>

bool rwTestsPassed = true;

void test_lock_for_read_write()
{
    PkReadWriteLock lock;
    lock.lockForRead();
    lock.unlock();
    lock.lockForWrite();
    lock.unlock();
}

void test_try_lock_for_read()
{
    PkReadWriteLock lock;
    if (!lock.tryLockForRead()) {
        PkTest::qFail("tryLockForRead() should succeed on unlocked lock", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
    lock.unlock();
}

void test_try_lock_for_write()
{
    PkReadWriteLock lock;
    if (!lock.tryLockForWrite()) {
        PkTest::qFail("tryLockForWrite() should succeed on unlocked lock", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
    lock.unlock();
}

void test_read_locker_raii()
{
    PkReadWriteLock lock;
    {
        PkReadLocker locker(&lock);
        if (!lock.tryLockForRead()) {
            // Multiple readers should be allowed
            lock.unlock();
        } else {
            lock.unlock();
        }
    }
    if (!lock.tryLockForWrite()) {
        PkTest::qFail("lock should be fully unlocked after read locker destroyed", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
    lock.unlock();
}

void test_write_locker_raii()
{
    PkReadWriteLock lock;
    {
        PkWriteLocker locker(&lock);
        if (lock.tryLockForRead()) {
            lock.unlock();
            PkTest::qFail("write locker should block readers", __FILE__, __LINE__);
            rwTestsPassed = false;
            return;
        }
        if (lock.tryLockForWrite()) {
            lock.unlock();
            PkTest::qFail("write locker should be exclusive", __FILE__, __LINE__);
            rwTestsPassed = false;
            return;
        }
    }
    if (!lock.tryLockForWrite()) {
        PkTest::qFail("lock should be unlocked after write locker destroyed", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
    lock.unlock();
}

void test_read_locker_unlock_relock()
{
    PkReadWriteLock lock;
    PkReadLocker locker(&lock);
    locker.unlock();
    if (!lock.tryLockForWrite()) {
        PkTest::qFail("lock should be unlocked after reader.unlock()", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
    lock.unlock();
    locker.relock();
    if (lock.tryLockForWrite()) {
        lock.unlock();
        PkTest::qFail("reader.relock() should acquire the read lock", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
}

void test_write_locker_unlock_relock()
{
    PkReadWriteLock lock;
    PkWriteLocker locker(&lock);
    locker.unlock();
    if (!lock.tryLockForWrite()) {
        PkTest::qFail("lock should be unlocked after writer.unlock()", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
    lock.unlock();
    locker.relock();
    if (lock.tryLockForWrite()) {
        lock.unlock();
        PkTest::qFail("writer.relock() should acquire the write lock", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
}

void test_read_locker_read_write_lock_accessor()
{
    PkReadWriteLock lock;
    PkReadLocker locker(&lock);
    if (locker.readWriteLock() != &lock) {
        PkTest::qFail("readWriteLock() accessor should return the original pointer", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
}

void test_write_locker_read_write_lock_accessor()
{
    PkReadWriteLock lock;
    PkWriteLocker locker(&lock);
    if (locker.readWriteLock() != &lock) {
        PkTest::qFail("readWriteLock() accessor should return the original pointer", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
}

void test_multiple_readers()
{
    PkReadWriteLock lock;
    std::atomic<int> readersActive{0};
    std::vector<std::thread> threads;
    for (int i = 0; i < 4; ++i) {
        threads.emplace_back([&]{
            PkReadLocker locker(&lock);
            readersActive++;
            // Multiple readers should exist simultaneously
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            readersActive--;
        });
    }
    for (auto& t : threads) {
        t.join();
    }
}

void test_write_lock_exclusivity()
{
    PkReadWriteLock lock;
    std::atomic<bool> writerActive{false};
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;

    // One writer thread
    threads.emplace_back([&]{
        for (int i = 0; i < 10; ++i) {
            PkWriteLocker locker(&lock);
            if (writerActive.exchange(true)) {
                failures++;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            writerActive.store(false);
        }
    });

    // Multiple reader threads
    for (int i = 0; i < 3; ++i) {
        threads.emplace_back([&]{
            for (int j = 0; j < 10; ++j) {
                PkReadLocker locker(&lock);
                if (writerActive.load()) {
                    failures++;
                }
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    if (failures > 0) {
        PkTest::qFail("write lock should be exclusive", __FILE__, __LINE__);
        rwTestsPassed = false;
        return;
    }
}

void test_upgrade_read_to_write()
{
    PkReadWriteLock lock;
    PkReadLocker readLocker(&lock);
    readLocker.unlock();
    readLocker.readWriteLock()->lockForWrite();
    readLocker.readWriteLock()->unlock();
    readLocker.relock();
}

int run_rwlock_tests(int argc, char **argv)
{
    (void)argc;
    (void)argv;
    rwTestsPassed = true;

    test_lock_for_read_write();
    test_try_lock_for_read();
    test_try_lock_for_write();
    test_read_locker_raii();
    test_write_locker_raii();
    test_read_locker_unlock_relock();
    test_write_locker_unlock_relock();
    test_read_locker_read_write_lock_accessor();
    test_write_locker_read_write_lock_accessor();
    test_multiple_readers();
    test_write_lock_exclusivity();
    test_upgrade_read_to_write();

    return rwTestsPassed ? 0 : 1;
}
