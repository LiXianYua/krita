#pragma once

// 替代 <QSemaphore>。D-24 保 C++17，std::counting_semaphore 是 C++20，
// 自写 mutex+condition_variable 版（Qt替代品选型.md §6.8「决定：自写」）。
// 方法面 = 保留范围内实测 3 个：acquire()/release()/tryAcquire(int,int)，
// timeoutMs 唯一实测取值 -1（Qt 语义：负数=无限等待）。
class PkSemaphore {
public:
    explicit PkSemaphore(int n = 0);
    ~PkSemaphore();

    PkSemaphore(const PkSemaphore&) = delete;
    PkSemaphore& operator=(const PkSemaphore&) = delete;

    void acquire();
    // timeoutMs < 0：无限等待（等价 acquire()，Qt 语义）
    // timeoutMs == 0：立即返回，不等待
    // timeoutMs > 0：最多等待这么多毫秒
    bool tryAcquire(int n, int timeoutMs);
    void release();

private:
    struct Impl;
    Impl* m_impl;
};
