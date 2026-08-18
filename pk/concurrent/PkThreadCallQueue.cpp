#include "PkThreadCallQueue.h"
#include "PkMutex.h"
#include "PkSemaphore.h"
#include <deque>
#include <map>
#include <memory>
#include <exception>

namespace {
struct Registry {
    PkMutex mutex;
    std::map<PkThreadId, std::deque<std::function<void()>>> queues;
};
Registry& registry() {
    static Registry r;
    return r;
}

// C-1 修复（线程 id 复用导致陈旧调用被无关新线程执行）：registry 按裸
// std::thread::id 分桶，而 OS 线程 id 在线程退出后会被复用——没有本机制，
// 投给线程 T 的调用可能在 T 退出后被"恰好拿到同一个 id"的新线程执行。
//
// 用一个 thread_local 哨兵给每个"调用它的线程"配一个隐式代（generation）：
// - 构造时（这个线程第一次触达 PkThreadCallQueue 的任意入口）：把 registry
//   里当前这个线程 id 现存的队列条目整个丢弃（不执行）——"我从来没触达过
//   这个队列系统，但队列里已经有条目"只可能是上一个用过这个 id 的线程留下
//   的陈旧调用，不属于我。丢弃是可接受的降级，与既有的"排队期间 disconnect
//   导致静默丢弃"是同一类语义，不是新引入的风险。
// - 析构时（线程退出）：把这个线程 id 在 registry 里的条目彻底删除，避免
//   死亡队列占位（同时解决 M-3：登记表无限增长）。
//
// 触发点选在 post()/postBlocking()（发起调用的线程）与 processPendingCalls()
// （pump 的线程）两处——都作用于"调用它的那个线程自己的 id"，不是 target
// 参数。这样"同线程自投自 pump"（post(me, ...) 后立即 processPendingCalls()）
// 这种合法用法在 post() 那一步就已经把该线程标记为"已触达"，随后的
// processPendingCalls() 不会重复触发丢弃、误删刚投进去的合法条目；而一个
// 从未主动 post 过、只是被动 pump 的新线程，第一次 processPendingCalls()
// 仍然会先自证"清白"，丢弃任何不属于自己的陈旧条目。
struct ThreadRegistryGuard {
    ThreadRegistryGuard() {
        Registry& r = registry();
        PkMutexLocker lock(&r.mutex);
        r.queues.erase(PkThread::currentThreadId());
    }
    ~ThreadRegistryGuard() {
        Registry& r = registry();
        PkMutexLocker lock(&r.mutex);
        r.queues.erase(PkThread::currentThreadId());
    }
};

void touchThreadFreshness()
{
    thread_local ThreadRegistryGuard guard;
    (void)guard;
}
}

void PkThreadCallQueue::post(PkThreadId target, std::function<void()> fn)
{
    touchThreadFreshness();
    Registry& r = registry();
    PkMutexLocker lock(&r.mutex);
    r.queues[target].push_back(std::move(fn));
}

void PkThreadCallQueue::postBlocking(PkThreadId target, std::function<void()> fn)
{
    // C-2 修复：release() 必须无论 fn() 是否抛异常都执行——用 RAII 守卫
    // （ReleaseGuard），不是"fn() 之后裸调 release()"的顺序写法。原实现里
    // fn() 抛出会跳过 release()，done->acquire() 永久不返回（探针复现
    // exit=124）。
    //
    // 异常语义：捕获 fn() 抛出的异常，postBlocking 在 done->acquire() 之后
    // 于*发射线程*上 std::rethrow_exception 重新抛出——调用方能感知目标线程
    // 执行失败，不是吞掉异常装作成功。这比 Qt（QMetaCallEvent 析构 release，
    // 异常至少不挂起）更进一步：异常本身也带回调用方，不会掩盖真实 bug。
    auto done = std::make_shared<PkSemaphore>(0);
    auto caught = std::make_shared<std::exception_ptr>();
    post(target, [fn = std::move(fn), done, caught]() mutable {
        struct ReleaseGuard {
            PkSemaphore& sem;
            ~ReleaseGuard() { sem.release(); }
        } guard{*done};
        try {
            fn();
        } catch (...) {
            *caught = std::current_exception();
        }
    });
    done->acquire();
    if (*caught) {
        std::rethrow_exception(*caught);
    }
}

int PkThreadCallQueue::processPendingCalls()
{
    touchThreadFreshness();
    PkThreadId me = PkThread::currentThreadId();
    std::deque<std::function<void()>> snapshot;
    {
        Registry& r = registry();
        PkMutexLocker lock(&r.mutex);
        auto it = r.queues.find(me);
        if (it == r.queues.end()) return 0;
        snapshot.swap(it->second);
    }
    // C-2 同源修复：单个调用抛异常不应该导致本次 pump 剩余的调用被跳过——
    // 每个调用单独 try/catch。不重新往外抛（processPendingCalls() 签名不
    // 变，见 PkThreadCallQueue.h 头注释的公开签名约束），最低要求是"不能让
    // 一个调用的异常吞掉同批次其余调用"；n 统计的是"尝试执行过的调用数"
    // （与既有语义一致：disconnect-during-queued 场景 n 也算"已出队"而非
    // "已送达"，见 pk/signal/tests/test_thread.cpp 的
    // test_disconnected_queued_call_is_dropped）。
    int n = 0;
    for (auto& fn : snapshot) {
        try {
            fn();
        } catch (...) {
            // 吞掉：同批次其余调用必须继续执行，不能被这一个异常连带跳过。
        }
        ++n;
    }
    return n;
}

std::size_t PkThreadCallQueue::pendingCount()
{
    Registry& r = registry();
    PkMutexLocker lock(&r.mutex);
    auto it = r.queues.find(PkThread::currentThreadId());
    return it == r.queues.end() ? 0 : it->second.size();
}
