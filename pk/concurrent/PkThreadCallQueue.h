#pragma once
#include <atomic>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include "PkThread.h"

// 跨线程投递原语（Q-8 的核心交付，R-24）：不建通用事件循环/应用对象抽象，
// 只提供"把一个待执行调用投递到目标线程，由目标线程显式 pump 才真正执行"
// 的最小机制。对应 Qt 的 QCoreApplication::postEvent + 元调用排队，但没有
// 隐式后台循环——目标线程必须自己调用 processPendingCalls() 才会执行队列里
// 的调用。语义边界见 docs/superpowers/plans/R-24.md「实测优先」的四个探针：
// - post() 即使 target 是调用者自己所在的线程，也不会立即执行（探针实验6：
//   显式 Queued 不因同线程而折叠成 Direct）
// - postBlocking() 阻塞调用线程直到 target 线程执行完这次调用、或者这次
//   调用被丢弃（未曾执行）为止（探针实验3）——这次调用在 target 线程第一次
//   触达队列系统时被当成陈旧条目清空（见下方 ⚠ 段落），或者 target 线程在
//   至少 pump 过一次之后正常退出（析构清理登记），这两条路径都保证 release
//   发射线程：调用方随后会收到 PkCallAbandonedException 而不是"装作正常
//   执行完成"（final whole-branch review NEW-C1，修复前存在真实的永久挂起
//   bug，这两条路径已修好）。
//   ⚠ **仍然会永久挂起的一条路径**：target 线程如果从头到尾一次都没调用过
//   `processPendingCalls()`/`post()`/`pendingCount()` 就退出（不是"调用过
//   一次之后不再调用"，是"从来没碰过这套队列系统的任何入口"），投给它的
//   `postBlocking()` 调用没有任何登记可以触发清理，会永久阻塞发射线程——
//   这与 Qt 同线程 BlockingQueuedConnection 会死锁是同一类风险（把调用
//   投给一个根本不打算处理它的线程，是调用方的误用，不是原语的缺陷），
//   本原语同样不做防护，见 PkObject.h 里 activateSignal 的 dispatch 说明
//   （「设计决定 4」）。这条与"预热"契约（见下方 ⚠ 段落）是同一个要求：
//   target 线程必须先调用过一次 processPendingCalls() 才安全。
//
// ⚠ 投递到某个线程的调用不会自动执行，该线程必须自己调用
// processPendingCalls()（或未来某个封装它的机制）来抽干队列——不这么做，
// 投递的调用会永远停在队列里，不报错、不崩溃、不打日志，是一个纯静默的
// 行为缺失（final whole-branch review I-3）。全仓目前没有任何 pump 调用点
// （pk/ 之外零调用），第一个把跨线程 Auto/Queued 连接搬进来的消费方必须
// 自己在目标线程装 pump。
//
// ⚠ 目标线程第一次调用 processPendingCalls() 时，会先把它自己 id 名下、
// 此刻已经排队的全部条目原样丢弃（不执行）——这是识别"线程 id 被 OS 复用、
// 队列里可能是上一个用过这个 id 的线程留下的陈旧调用"的唯一手段（见
// PkThreadCallQueue.cpp 里 ThreadRegistryGuard 的注释，final whole-branch
// review C-1）。这个判定天生分不清"陈旧调用"和"合法投给我、只是我还没
// 来得及第一次 pump"，二者形状完全一样。
// 结果：任何人在目标线程第一次 processPendingCalls() 之前投给它的调用，
// 都有被当成陈旧条目一并丢弃的风险——**不是必然发生，是一个启动期竞态**
// （final whole-branch review NEW-I2）。要保证"第一批投递一定送达"，目标
// 线程应该在把自己的线程 id 发布给任何人之前，先调用一次 processPendingCalls()
// 完成"预热"（此时队列必然为空，是无害 no-op，但会把"第一次触达"这个
// 一次性判定提前消耗掉，后续真正投来的调用不会再被当成陈旧条目）。不预热
// 的后果因入口不同而不同：
// - post() 投的调用如果被这样丢弃：静默丢弃，不报错、不崩溃（与既有的
//   "排队期间 disconnect 导致静默丢弃"是同一类可接受的降级）。
// - postBlocking() 投的调用如果被这样丢弃：发射线程会被正常唤醒（不会永久
//   挂起），但会收到下面这个 PkCallAbandonedException——调用方能感知到
//   "这次调用被丢弃、从未真正执行"，不会装作执行成功（final whole-branch
//   review NEW-C1）。
class PkCallAbandonedException : public std::runtime_error {
public:
    PkCallAbandonedException()
        : std::runtime_error(
              "PkThreadCallQueue::postBlocking: call was discarded by the "
              "target thread before it was ever executed (discarded as a "
              "stale entry on the target's first touch of the queue system, "
              "or the target thread exited without pumping it)") {}
};

// R-34 Task 4：post(target, fn, lt) 三参重载的对象存活保护句柄。
// - claim：与目标对象的析构共享的 recursive_mutex。执行侧在 pump 时先锁它，
//   与析构串行化（析构持同一把锁），保证「对象正在析构 / 已析构」不会与
//   排队调用并发触碰对象。
// - alive：目标对象的存活标志（PkObject::m_alive 的别名视图）。执行侧在
//   claim 下重查它，对象已析构（false）则静默丢弃本次调用——对齐 Qt「析构
//   时清除已投递的 posted events」的语义。
// 由 PkObject::callLifetime() 产出；消费方接线（KisSynchronizedConnection、
// activateSignal 的 queued 投递）是 S 线的活，本任务只提供机制。
struct PkCallLifetime {
    std::shared_ptr<std::recursive_mutex> claim;
    std::shared_ptr<std::atomic<bool>> alive;
};

class PkThreadCallQueue {
public:
    // 投递一个待执行调用到 target 线程的队列，立即返回，不等待执行。
    // 只把 fn 排进 target 的队列——不触碰调用者自己线程的任何状态（final
    // whole-branch review NEW-I1：早期实现曾经错误地在这里顺带给调用者
    // 自己的线程做"首次触达"判定，导致一个线程只要调用过一次 post()，
    // 就可能把它自己尚未 pump 过的入站队列当成陈旧条目清空——那个判定的
    // 唯一合适位置是 processPendingCalls()，因为只有那里才是"消费自己的
    // 队列"，见该方法的注释）。
    static void post(PkThreadId target, std::function<void()> fn);

    // R-34 Task 4：带对象存活保护的重载。语义与两参 post() 相同，差别只在
    // 执行侧——目标线程 pump 时先在 lt.claim 下重查 lt.alive：对象已析构
    // （alive==false）则静默丢弃，不执行 fn；否则照常执行。与 PkObject 析构
    // 串行化（析构持同一 claim），关闭「对象先死 + 目标线程后 pump」的悬垂
    // UB。PkCallLifetime 见上方 struct。
    static void post(PkThreadId target, std::function<void()> fn, PkCallLifetime lt);

    // 投递并阻塞调用线程，直到 target 线程通过 processPendingCalls() 把这次
    // 调用真正执行完、或者这次调用被丢弃为止。前者：fn() 若抛出异常，
    // postBlocking 在调用线程上重新抛出（不吞掉，调用方能感知目标线程执行
    // 失败）。后者：抛 PkCallAbandonedException（见上方类注释），不会让
    // 调用方误以为 fn() 正常执行过。
    static void postBlocking(PkThreadId target, std::function<void()> fn);

    // 执行"调用它的线程"（PkThread::currentThreadId()）队列里现存的全部
    // 待执行调用，返回执行的个数。先拍一份快照再执行——pump 过程中被别的
    // 线程新 post 进来的调用不保证在本次 pump 内被执行（对齐 Qt
    // processEvents() "不保证处理执行期间新到的事件"这条语义）。
    static int processPendingCalls();

    // Consume this thread's one-time stale-entry check, then return its id.
    // Publish only the returned value: the ordering makes it impossible for a
    // caller following this pattern to expose an id before queue warm-up.
    static PkThreadId warmUpCurrentThread();

    // 仅供测试/试接观测：调用它的线程的队列里还有多少待执行调用未处理。
    static std::size_t pendingCount();
};
