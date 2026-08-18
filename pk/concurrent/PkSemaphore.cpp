#include "PkSemaphore.h"
#include <mutex>
#include <condition_variable>
#include <chrono>

struct PkSemaphore::Impl {
    std::mutex mutex;
    std::condition_variable cv;
    int count;
    explicit Impl(int n) : count(n) {}
};

PkSemaphore::PkSemaphore(int n) : m_impl(new Impl(n)) {}
PkSemaphore::~PkSemaphore() { delete m_impl; }

void PkSemaphore::acquire() {
    std::unique_lock<std::mutex> lock(m_impl->mutex);
    m_impl->cv.wait(lock, [this] { return m_impl->count > 0; });
    --m_impl->count;
}

bool PkSemaphore::tryAcquire(int n, int timeoutMs) {
    std::unique_lock<std::mutex> lock(m_impl->mutex);
    if (timeoutMs < 0) {
        m_impl->cv.wait(lock, [this, n] { return m_impl->count >= n; });
        m_impl->count -= n;
        return true;
    }
    if (timeoutMs == 0) {
        if (m_impl->count >= n) {
            m_impl->count -= n;
            return true;
        }
        return false;
    }
    bool ok = m_impl->cv.wait_for(lock, std::chrono::milliseconds(timeoutMs),
                                   [this, n] { return m_impl->count >= n; });
    if (ok) m_impl->count -= n;
    return ok;
}

void PkSemaphore::release() {
    {
        std::lock_guard<std::mutex> lock(m_impl->mutex);
        ++m_impl->count;
    }
    m_impl->cv.notify_all();
}
