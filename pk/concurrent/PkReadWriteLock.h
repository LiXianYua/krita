#pragma once
#include <shared_mutex>
#include <atomic>

// 替代 <QReadWriteLock>。
//
// tryLockForRead()/tryLockForWrite()：真实调用点见
// libs/image/tiles3/kis_tile_data_store.cc:269、kis_tile_data.cc:210、
// kis_tile_hash_table_p.h:423（均在 QReadWriteLock 成员 m_swapLock/m_lock 上，
// 声明见 kis_tile_data_interface.h:281）。R-10 final review 更正：此前 Task 1
// review 阶段以"零真实调用点"为由删除过这两个方法，该前提是假的。
//
// RecursionMode：kis_tile_hash_table_p.h:17,30 两处都以
// `m_lock(QReadWriteLock::NonRecursive)` 构造，零真实调用点传 Recursive——
// 所以这里只接受构造参数、不实现真正的递归读写锁语义（std::shared_mutex
// 本就不支持递归；默认构造的行为已经是 NonRecursive 语义）。
//
// unlock() 单一入口要分辨"当前是读锁还是写锁"：写锁互斥（同一时刻只有
// 一个线程能处于写模式），所以 m_writeLocked 不会有并发写竞争；用
// exchange 在 unlock() 里原子地"读取并清零"，读者线程看到的必然是
// false（读写互斥保证不会有读者与写者同时持锁）。tryLockForWrite() 成功后
// 同样要设置 m_writeLocked = true，否则 unlock() 会误判成读锁释放。
class PkReadWriteLock {
public:
    // Qt 语义：NonRecursive 是唯一真实用到的取值（见上方注释）。Recursive
    // 只是接受该枚举值以便通过编译，不提供真正的递归语义——保留范围内没有
    // 调用点传它。
    enum RecursionMode { NonRecursive, Recursive };

    explicit PkReadWriteLock(RecursionMode mode = NonRecursive) {
        (void)mode; // 唯一真实取值 NonRecursive 下行为等同默认构造
    }

    void lockForRead() { m_mutex.lock_shared(); }
    void lockForWrite() {
        m_mutex.lock();
        m_writeLocked.store(true, std::memory_order_relaxed);
    }
    // Qt 语义：非阻塞尝试。成功（拿到锁）返回 true，会阻塞则返回 false。
    bool tryLockForRead() { return m_mutex.try_lock_shared(); }
    bool tryLockForWrite() {
        if (m_mutex.try_lock()) {
            m_writeLocked.store(true, std::memory_order_relaxed);
            return true;
        }
        return false;
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
