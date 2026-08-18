#include "PkThreadCallQueue.h"
#include "PkMutex.h"
#include "PkSemaphore.h"
#include <deque>
#include <map>
#include <memory>

namespace {
struct Registry {
    PkMutex mutex;
    std::map<PkThreadId, std::deque<std::function<void()>>> queues;
};
Registry& registry() {
    static Registry r;
    return r;
}
}

void PkThreadCallQueue::post(PkThreadId target, std::function<void()> fn)
{
    Registry& r = registry();
    PkMutexLocker lock(&r.mutex);
    r.queues[target].push_back(std::move(fn));
}

void PkThreadCallQueue::postBlocking(PkThreadId target, std::function<void()> fn)
{
    // release() 在 fn() 之后调用：调用线程 acquire() 返回时，目标线程一定
    // 已经跑完 fn（happens-before 由信号量的 acquire/release 保证）。
    auto done = std::make_shared<PkSemaphore>(0);
    post(target, [fn = std::move(fn), done]() mutable {
        fn();
        done->release();
    });
    done->acquire();
}

int PkThreadCallQueue::processPendingCalls()
{
    PkThreadId me = PkThread::currentThreadId();
    std::deque<std::function<void()>> snapshot;
    {
        Registry& r = registry();
        PkMutexLocker lock(&r.mutex);
        auto it = r.queues.find(me);
        if (it == r.queues.end()) return 0;
        snapshot.swap(it->second);
    }
    int n = 0;
    for (auto& fn : snapshot) { fn(); ++n; }
    return n;
}

std::size_t PkThreadCallQueue::pendingCount()
{
    Registry& r = registry();
    PkMutexLocker lock(&r.mutex);
    auto it = r.queues.find(PkThread::currentThreadId());
    return it == r.queues.end() ? 0 : it->second.size();
}
