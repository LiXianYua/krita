#include "PkThreadCallQueue.h"
#include "PkMutex.h"
#include "PkSemaphore.h"
#include <atomic>
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
    // Calls can be posted by workers owned by static objects while C++ static
    // destruction is in progress.  Deliberately retain the process registry:
    // the OS reclaims it, and no worker can observe a destructed mutex/map.
    static Registry* r = new Registry;
    return *r;
}

// C-1 修复（线程 id 复用导致陈旧调用被无关新线程执行）：registry 按裸
// std::thread::id 分桶，而 OS 线程 id 在线程退出后会被复用——没有本机制，
// 投给线程 T 的调用可能在 T 退出后被"恰好拿到同一个 id"的新线程执行。
//
// 用一个 thread_local 哨兵给每个"调用它的线程"配一个隐式代（generation）：
// - 构造时（这个线程第一次调用 processPendingCalls()）：把 registry 里
//   当前这个线程 id 现存的队列条目整个丢弃（不执行）——"我从来没消费过
//   这个队列，但队列里已经有条目"只可能是上一个用过这个 id 的线程留下
//   的陈旧调用，不属于我。丢弃是可接受的降级，与既有的"排队期间 disconnect
//   导致静默丢弃"是同一类语义，不是新引入的风险；`postBlocking()` 额外
//   保证被丢弃的调用也会 release 发射线程并让它感知到"被丢弃"（见下方
//   `postBlocking()` 的注释，final whole-branch review NEW-C1）。
// - 析构时（线程退出）：把这个线程 id 在 registry 里的条目彻底删除，避免
//   死亡队列占位（同时解决 M-3：登记表无限增长）——同一个 erase()，
//   同样经过 postBlocking() 的丢弃感知机制，不是构造时那条路径之外另开
//   一套。
//
// 触发点只在 processPendingCalls()（pump 的线程）一处，作用于"调用它的
// 那个线程自己的 id"，不是 target 参数——这是"消费自己的队列"唯一发生
// 的地方，只有这里才能回答"这批条目是不是属于我这一世"。**不要在 post()/
// postBlocking() 里也触发这个判定**：那两个方法只是"我要往某个 target 投
// 一次调用"，跟"调用它们的这个线程自己的入站队列新不新鲜"毫无关系——
// 早期实现在 post() 里也调用过 touchThreadFreshness()（作用于调用者自己
// 的 id），后果是一个线程只要调用过一次 post()（哪怕投给别的线程），就
// 会把它自己尚未 pump 过的入站队列错当成陈旧条目清空，双向跨线程通信在
// 启动期因此会静默丢消息（final whole-branch review NEW-I1，已移除）。
//
// 因为判定只在 processPendingCalls() 里发生，"目标线程在把自己的线程 id
// 发布给任何人之前先 pump 一次（预热）"仍然是保证"第一批投递必达"的
// 唯一手段——预热本身把这个一次性判定提前消耗在一个必然为空的队列上，
// 后续真正投来的调用就不会再被这里的丢弃逻辑波及。这条契约写在
// PkThreadCallQueue.h 的类头注释里（final whole-branch review NEW-I2）。
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
    // NEW-I1 修复：不在这里调用 touchThreadFreshness()——理由见上方
    // ThreadRegistryGuard 的大注释块。post() 只做一件事：把 fn 排进
    // target 的队列，不触碰调用者自己线程的任何状态。
    Registry& r = registry();
    PkMutexLocker lock(&r.mutex);
    r.queues[target].push_back(std::move(fn));
}

void PkThreadCallQueue::postBlocking(PkThreadId target, std::function<void()> fn)
{
    // NEW-C1 修复：C-1 的 ThreadRegistryGuard 丢弃陈旧条目时，做法是直接
    // 析构排队的闭包（r.queues.erase(...)），不会调用它——而 C-2 的异常
    // 安全修复曾经把信号量 release() 放在闭包体*内部*的一个局部 RAII 守卫
    // 里：闭包被析构而不是被调用时，release() 永远不会执行，done->acquire()
    // 永不返回（这个仓库的历史 bug，两轮评审各修了一半）。
    //
    // 现在的做法：release 是否发生，跟"闭包对象本身"的生命周期绑定，而
    // 不是跟"闭包体有没有跑过"绑定——BlockingState::releaseOnce() 用一个
    // 原子 CAS 保证 PkSemaphore::release() 全局只被调用一次，两条路径都
    // 会触发它：
    // 1. 闭包体正常跑完（无论 fn() 是否抛异常）：body 末尾显式调用
    //    releaseOnce()，第一时间释放发射线程，不用等到整批 pump 结束、
    //    snapshot 析构时闭包才被回收——否则本次调用之后排在同一批的其余
    //    调用会把发射线程多拖住一截。
    // 2. 闭包对象被直接析构、从未跑过 body：闭包按值捕获一个
    //    std::shared_ptr<void>，析构器（deleter）调 releaseOnce()。
    //    闭包对象析构时——无论是正常执行完之后被回收，还是被
    //    ThreadRegistryGuard 的 erase() 直接析构丢弃、从未执行——这份
    //    shared_ptr 的引用计数都会跟着递减；真正触发析构器的是"最后一个
    //    持有者消失"那一刻。
    //
    // ⚠ 这份 shared_ptr<void> 的构造式必须**直接写在闭包的 init-capture
    // 里**，不能先在 postBlocking() 自己的栈帧上声明一个同名局部变量、
    // 再捕获它——那样 postBlocking() 自己这份局部引用会在整个 acquire()
    // 阻塞期间一直存活，"最后一个持有者"永远轮不到闭包那份，变成自己等
    // 自己的死锁（试接踩过：捕获一个"手写析构函数的 RAII 守卫类型"按值
    // 存进闭包也会踩坑，原因不同——用户声明的析构函数会隐式抑制移动构造，
    // std::function 内部把闭包挪来挪去时退化成拷贝构造，导致一个转瞬即逝
    // 的中间拷贝提前把信号量释放掉；shared_ptr 天生按引用计数设计，可以
    // 被随意拷贝/移动而不提前触发析构器，天然规避这个问题，是这里选它而
    // 不是手写 RAII 类型的原因）。
    //
    // state->executed 只在闭包体真正跑起来时才被置 true；被丢弃的路径上
    // 它保持默认值 false。postBlocking() 在 acquire() 之后据此区分"正常
    // 执行完（可能带 fn() 自己的异常）"与"从未执行、被丢弃"——后者抛
    // PkCallAbandonedException，不装作跟正常执行一样（调用方能感知到）。
    struct BlockingState {
        PkSemaphore sem{0};
        std::atomic<bool> released{false};
        bool executed = false;
        std::exception_ptr caught;
        void releaseOnce() {
            bool expected = false;
            if (released.compare_exchange_strong(expected, true)) {
                sem.release();
            }
        }
    };
    auto state = std::make_shared<BlockingState>();

    post(target,
         [fn = std::move(fn), state,
          releaseOnDestroy = std::shared_ptr<void>(
              static_cast<void*>(nullptr),
              [state](void*) { state->releaseOnce(); })]() mutable {
             (void)releaseOnDestroy; // 只为生命周期存在，见上方大注释。
             state->executed = true;
             try {
                 fn();
             } catch (...) {
                 state->caught = std::current_exception();
             }
             // 正常执行路径：立即释放，不等闭包对象本身被回收。
             state->releaseOnce();
         });

    state->sem.acquire();

    if (!state->executed) {
        throw PkCallAbandonedException();
    }
    if (state->caught) {
        std::rethrow_exception(state->caught);
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

PkThreadId PkThreadCallQueue::warmUpCurrentThread()
{
    processPendingCalls();
    return PkThread::currentThreadId();
}

std::size_t PkThreadCallQueue::pendingCount()
{
    Registry& r = registry();
    PkMutexLocker lock(&r.mutex);
    auto it = r.queues.find(PkThread::currentThreadId());
    return it == r.queues.end() ? 0 : it->second.size();
}
