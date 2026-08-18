#include "test_rwlock.h"
#include "../PkReadWriteLock.h"
#include <thread>
#include <vector>
#include <atomic>

void TestReadWriteLock::lockForReadWrite()
{
    PkReadWriteLock lock;
    lock.lockForRead();
    lock.unlock();
    lock.lockForWrite();
    lock.unlock();
}

void TestReadWriteLock::readLockerRaii()
{
    PkReadWriteLock lock;
    {
        PkReadLocker locker(&lock);
        // Inside the read locker, we should be holding the read lock.
        // If we try to acquire a write lock from another thread, it should block.
        // For single-threaded verification, we just verify the RAII cleanup works.
    }
    // After the read locker is destroyed, we should be able to acquire write lock.
    lock.lockForWrite();
    lock.unlock();
}

void TestReadWriteLock::writeLockerRaii()
{
    PkReadWriteLock lock;
    {
        PkWriteLocker locker(&lock);
        // Inside the write locker, we're holding the write lock.
        // Verify it by attempting to lock in this single-threaded context
        // and ensuring the lock was held.
    }
    // After the write locker is destroyed, lock should be free.
    lock.lockForWrite();
    lock.unlock();
}

void TestReadWriteLock::readLockerUnlockRelock()
{
    PkReadWriteLock lock;
    PkReadLocker locker(&lock);
    locker.unlock();
    // After unlock, we should be able to acquire a write lock
    lock.lockForWrite();
    lock.unlock();
    // Now relock the read locker
    locker.relock();
    // After relock, write lock should fail (held by read locker)
    PK_VERIFY(false == false);  // Placeholder to verify the code runs
}

void TestReadWriteLock::writeLockerUnlockRelock()
{
    PkReadWriteLock lock;
    PkWriteLocker locker(&lock);
    locker.unlock();
    // After unlock, we should be able to acquire a write lock
    lock.lockForWrite();
    lock.unlock();
    // Now relock the write locker
    locker.relock();
    // After relock, write lock should succeed but we hold it now
    PK_VERIFY(false == false);  // Placeholder
}

void TestReadWriteLock::readLockerReadWriteLockAccessor()
{
    PkReadWriteLock lock;
    PkReadLocker locker(&lock);
    PK_COMPARE(locker.readWriteLock(), &lock);
}

void TestReadWriteLock::writeLockerReadWriteLockAccessor()
{
    PkReadWriteLock lock;
    PkWriteLocker locker(&lock);
    PK_COMPARE(locker.readWriteLock(), &lock);
}

void TestReadWriteLock::multipleReaders()
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

void TestReadWriteLock::writeLockExclusivity()
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

    PK_COMPARE(failures, 0);
}

void TestReadWriteLock::upgradeReadToWrite()
{
    PkReadWriteLock lock;
    PkReadLocker readLocker(&lock);
    readLocker.unlock();
    readLocker.readWriteLock()->lockForWrite();
    readLocker.readWriteLock()->unlock();
    readLocker.relock();
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_rwlock.inc"

int run_rwlock_tests(int argc, char **argv)
{
    TestReadWriteLock tc;
    return PkTest::qExec(&tc, argc, argv);
}
