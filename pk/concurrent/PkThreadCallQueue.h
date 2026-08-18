#pragma once
#include <functional>
#include <cstddef>
#include "PkThread.h"

// 跨线程投递原语（Q-8 的核心交付，R-24）：不建通用事件循环/应用对象抽象，
// 只提供"把一个待执行调用投递到目标线程，由目标线程显式 pump 才真正执行"
// 的最小机制。对应 Qt 的 QCoreApplication::postEvent + 元调用排队，但没有
// 隐式后台循环——目标线程必须自己调用 processPendingCalls() 才会执行队列里
// 的调用。语义边界见 docs/superpowers/plans/R-24.md「实测优先」的四个探针：
// - post() 即使 target 是调用者自己所在的线程，也不会立即执行（探针实验6：
//   显式 Queued 不因同线程而折叠成 Direct）
// - postBlocking() 阻塞调用线程直到 target 线程 pump 执行完这次调用为止
//   （探针实验3）；如果 target 线程从此再也不调用 processPendingCalls()，
//   调用方永久阻塞——这与 Qt 同线程 BlockingQueuedConnection 会死锁是
//   同一类风险，本原语不做特殊检测，见 PkObject.h 里 activateSignal 的
//   dispatch 说明（「设计决定 4」）
//
// ⚠ 投递到某个线程的调用不会自动执行，该线程必须自己调用
// processPendingCalls()（或未来某个封装它的机制）来抽干队列——不这么做，
// 投递的调用会永远停在队列里，不报错、不崩溃、不打日志，是一个纯静默的
// 行为缺失（final whole-branch review I-3）。全仓目前没有任何 pump 调用点
// （pk/ 之外零调用），第一个把跨线程 Auto/Queued 连接搬进来的消费方必须
// 自己在目标线程装 pump。
class PkThreadCallQueue {
public:
    // 投递一个待执行调用到 target 线程的队列，立即返回，不等待执行。
    static void post(PkThreadId target, std::function<void()> fn);

    // 投递并阻塞调用线程，直到 target 线程通过 processPendingCalls() 把这次
    // 调用真正执行完为止。
    static void postBlocking(PkThreadId target, std::function<void()> fn);

    // 执行"调用它的线程"（PkThread::currentThreadId()）队列里现存的全部
    // 待执行调用，返回执行的个数。先拍一份快照再执行——pump 过程中被别的
    // 线程新 post 进来的调用不保证在本次 pump 内被执行（对齐 Qt
    // processEvents() "不保证处理执行期间新到的事件"这条语义）。
    static int processPendingCalls();

    // 仅供测试/试接观测：调用它的线程的队列里还有多少待执行调用未处理。
    static std::size_t pendingCount();
};
