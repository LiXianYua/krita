#pragma once
#include <mutex>

// 替代 <QMutex>。保留范围内零 QMutex::Recursive 构造；kis_updater_context.cpp:220
// 用 std::unique_lock<QMutex> 配 std::try_to_lock，要求 Lockable 概念的小写
// try_lock()——这是 Qt 5.14+ 给 QMutex 加的 STL 兼容面，这里原样对齐。
// tryLock()（驼峰式）：真实调用点 libs/image/kis_image_animation_interface.cpp:499
// `return m_d->frameGenerationLock.tryLock();`（成员声明见同文件 73 行
// `QMutex frameGenerationLock;`）。R-10 final review 更正：此前文档记的
// "零 tryLock() 调用"是假前提。Qt 的驼峰式 tryLock() 与 C++ 标准的小写
// try_lock() 语义相同（非阻塞尝试，成功返回 true），直接转发。
class PkMutex {
public:
    void lock() { m_mutex.lock(); }
    void unlock() { m_mutex.unlock(); }
    bool try_lock() { return m_mutex.try_lock(); }
    bool tryLock() { return try_lock(); }

private:
    friend class PkWaitCondition;
    std::mutex m_mutex;
};

// 替代 <QMutexLocker>。RAII；mutex() 供 QWaitCondition::wait(QMutex*) 场景
// 取回底层指针（kis_updater_context.cpp:214）。
class PkMutexLocker {
public:
    explicit PkMutexLocker(PkMutex* mutex) : m_mutex(mutex), m_locked(false) {
        relock();
    }
    ~PkMutexLocker() { unlock(); }

    PkMutexLocker(const PkMutexLocker&) = delete;
    PkMutexLocker& operator=(const PkMutexLocker&) = delete;

    void unlock() {
        if (m_locked) {
            m_mutex->unlock();
            m_locked = false;
        }
    }
    void relock() {
        if (!m_locked && m_mutex) {
            m_mutex->lock();
            m_locked = true;
        }
    }
    PkMutex* mutex() const { return m_mutex; }

private:
    PkMutex* m_mutex;
    bool m_locked;
};
