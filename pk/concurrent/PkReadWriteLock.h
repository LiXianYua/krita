#pragma once
#include <shared_mutex>
#include <atomic>

// 替代 <QReadWriteLock>。保留范围内零 Recursive 构造、零 tryLockFor*()
// 调用点。两个方法都不实现——实测零真实调用点，范围判据排除。
//
// unlock() 单一入口要分辨"当前是读锁还是写锁"：写锁互斥（同一时刻只有
// 一个线程能处于写模式），所以 m_writeLocked 不会有并发写竞争；用
// exchange 在 unlock() 里原子地"读取并清零"，读者线程看到的必然是
// false（读写互斥保证不会有读者与写者同时持锁）。
class PkReadWriteLock {
public:
    void lockForRead() { m_mutex.lock_shared(); }
    void lockForWrite() {
        m_mutex.lock();
        m_writeLocked.store(true, std::memory_order_relaxed);
    }
    void unlock() {
        if (m_writeLocked.exchange(false, std::memory_order_relaxed)) {
            m_mutex.unlock();
        } else {
            m_mutex.unlock_shared();
        }
    }

private:
    std::shared_mutex m_mutex;
    std::atomic<bool> m_writeLocked{false};
};

// 替代 <QReadLocker>。readWriteLock()/relock() 供
// KisUpgradeToWriteLocker.h 这种"先放读锁再升级写锁"的模式使用：
//   m_locker->unlock();
//   m_locker->readWriteLock()->lockForWrite();
//   ...
//   m_locker->readWriteLock()->unlock();
//   m_locker->relock();
class PkReadLocker {
public:
    explicit PkReadLocker(PkReadWriteLock* lock) : m_lock(lock), m_locked(false) {
        relock();
    }
    ~PkReadLocker() { unlock(); }
    PkReadLocker(const PkReadLocker&) = delete;
    PkReadLocker& operator=(const PkReadLocker&) = delete;

    void unlock() {
        if (m_locked) { m_lock->unlock(); m_locked = false; }
    }
    void relock() {
        if (!m_locked && m_lock) { m_lock->lockForRead(); m_locked = true; }
    }
    PkReadWriteLock* readWriteLock() const { return m_lock; }

private:
    PkReadWriteLock* m_lock;
    bool m_locked;
};

// 替代 <QWriteLocker>。
class PkWriteLocker {
public:
    explicit PkWriteLocker(PkReadWriteLock* lock) : m_lock(lock), m_locked(false) {
        relock();
    }
    ~PkWriteLocker() { unlock(); }
    PkWriteLocker(const PkWriteLocker&) = delete;
    PkWriteLocker& operator=(const PkWriteLocker&) = delete;

    void unlock() {
        if (m_locked) { m_lock->unlock(); m_locked = false; }
    }
    void relock() {
        if (!m_locked && m_lock) { m_lock->lockForWrite(); m_locked = true; }
    }
    PkReadWriteLock* readWriteLock() const { return m_lock; }

private:
    PkReadWriteLock* m_lock;
    bool m_locked;
};
