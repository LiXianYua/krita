#include "PkThreadPool.h"

PkThreadPool::PkThreadPool(int maxThreadCount) : m_maxThreadCount(maxThreadCount) {
    std::lock_guard<std::mutex> lock(m_mutex);
    resizeWorkersLocked(m_maxThreadCount);
}

PkThreadPool::~PkThreadPool() {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_stopping = true;
    }
    m_taskAvailable.notify_all();
    for (auto& t : m_workers) {
        if (t.joinable()) t.join();
    }
}

void PkThreadPool::resizeWorkersLocked(int count) {
    while (static_cast<int>(m_workers.size()) < count) {
        m_workers.emplace_back(&PkThreadPool::workerLoop, this);
    }
    // 缩容：只减少目标线程数，多出的 worker 在下次任务耗尽后随
    // m_stopping 统一回收；保留范围内的真实调用点从未在有任务运行时
    // 缩容线程数（KisUpdaterContext::setThreadsLimit 的前置断言
    // 要求"当前没有任务在跑"），本任务遵循同一前提，不做运行时抢占式
    // 缩容。
}

void PkThreadPool::setMaxThreadCount(int count) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_maxThreadCount = count;
    resizeWorkersLocked(count);
}

int PkThreadPool::maxThreadCount() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_maxThreadCount;
}

void PkThreadPool::start(PkRunnable* runnable) {
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_queue.push_back(runnable);
    }
    m_taskAvailable.notify_one();
}

void PkThreadPool::waitForDone() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_idle.wait(lock, [this] { return m_queue.empty() && m_busyCount == 0; });
}

void PkThreadPool::workerLoop() {
    while (true) {
        PkRunnable* task = nullptr;
        {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_taskAvailable.wait(lock, [this] {
                return m_stopping || !m_queue.empty();
            });
            if (m_stopping && m_queue.empty()) return;
            task = m_queue.front();
            m_queue.pop_front();
            ++m_busyCount;
        }

        task->run();
        bool autoDelete = task->autoDelete();
        if (autoDelete) delete task;

        {
            std::lock_guard<std::mutex> lock(m_mutex);
            --m_busyCount;
            if (m_queue.empty() && m_busyCount == 0) {
                m_idle.notify_all();
            }
        }
    }
}
