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
    std::atomic<bool> writerAcquired{false};
    std::thread writerThread;

    {
        PkReadLocker locker(&lock);

        // Inside the read locker scope, the read lock should be held.
        // Spawn a thread that tries to acquire a write lock.
        writerThread = std::thread([&]{
            lock.lockForWrite();
            writerAcquired.store(true);
            lock.unlock();
        });

        // Give the writer a bounded time to try acquisition
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // The writer should be blocked because we hold the read lock
        PK_VERIFY(!writerAcquired.load());
    }
    // RAII destructor has released the read lock

    // Join the writer thread - it should now be able to acquire
    writerThread.join();

    // Verify the writer did eventually acquire
    PK_VERIFY(writerAcquired.load());
}

void TestReadWriteLock::writeLockerRaii()
{
    PkReadWriteLock lock;
    std::atomic<bool> readerAcquired{false};
    std::thread readerThread;

    {
        PkWriteLocker locker(&lock);

        // Inside the write locker scope, the write lock should be held.
        // Spawn a thread that tries to acquire a read lock.
        readerThread = std::thread([&]{
            lock.lockForRead();
            readerAcquired.store(true);
            lock.unlock();
        });

        // Give the reader a bounded time to try acquisition
        std::this_thread::sleep_for(std::chrono::milliseconds(50));

        // The reader should be blocked because we hold the write lock
        PK_VERIFY(!readerAcquired.load());
    }
    // RAII destructor has released the write lock

    // Join the reader thread - it should now be able to acquire
    readerThread.join();

    // Verify the reader did eventually acquire
    PK_VERIFY(readerAcquired.load());
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

    // After relock, a write lock should be blocked by the active read lock
    std::atomic<bool> writerAcquired{false};
    std::thread writerThread([&]{
        lock.lockForWrite();
        writerAcquired.store(true);
        lock.unlock();
    });

    // Give the writer thread time to attempt acquisition
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // At this point, the writer should NOT have acquired the lock yet
    // because the read locker still holds it
    PK_VERIFY(!writerAcquired.load());

    // Now release the read lock
    locker.unlock();

    // Join the writer thread - it should now succeed and complete
    writerThread.join();

    // Verify the writer did eventually acquire
    PK_VERIFY(writerAcquired.load());
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

    // After relock, both read and write locks should be blocked
    std::atomic<bool> readerAcquired{false};
    std::thread readerThread([&]{
        lock.lockForRead();
        readerAcquired.store(true);
        lock.unlock();
    });

    // Give the reader thread time to attempt acquisition
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // At this point, the reader should NOT have acquired the lock yet
    // because the write locker still holds it
    PK_VERIFY(!readerAcquired.load());

    // Now release the write lock
    locker.unlock();

    // Join the reader thread - it should now succeed and complete
    readerThread.join();

    // Verify the reader did eventually acquire
    PK_VERIFY(readerAcquired.load());
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
