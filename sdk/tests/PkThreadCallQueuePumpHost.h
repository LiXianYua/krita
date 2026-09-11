/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef PKTHREADCALLQUEUEPUMPHOST_H
#define PKTHREADCALLQUEUEPUMPHOST_H

#include <PkThreadCallQueue.h>

#include <chrono>
#include <thread>

namespace KritaTestSdk
{

/**
 * 主线程队列 pump 宿主（驱动侧，显式调用）。
 *
 * `PkThreadCallQueue::post()` 只排队、不执行：投到某个线程的调用，要等该线程
 * **自己**调 `processPendingCalls()` 才会被抽干；不 pump 的表现是纯静默的行为
 * 缺失——不报错、不崩溃、不打日志。内核里没有任何隐式后台 pump，也不该有
 * （调度 owner 归驱动侧：CLI 的 `paint_smoke`、Flutter 宿主、以及这里的测试）。
 *
 * 宿主的两条义务（真源：`pk/concurrent/PkThreadCallQueue.h` 类头注释、
 * `libs/image/KisSafeBlockingQueueConnectionProxy.cpp:20-32`）：
 *
 *  1. **预热先于发布 id**：目标线程在把自己的线程 id 交给任何消费方之前，必须先在该
 *     线程上调用一次 `PkThreadCallQueue::warmUpCurrentThread()`，**并只发布它的返回值**。
 *     目标线程的第一次 pump 会把该 id 名下此刻已排队的条目当成「上个用过这个 id 的
 *     线程留下的陈旧调用」丢弃——预热把这个一次性判定提前消耗在一个必然为空的队列上。
 *     测试入口的这条义务已由 `sdk/tests/simpletest.h` 的 `SIMPLE_MAIN_IMPL` 履行
 *     （`registerMainThread()` 之后、构造 fixture 之前）。
 *  2. **持续 pump**：事件循环/等待期间要不断调用 `processPendingCalls()`，否则排队中的
 *     调用（Queued 信号、`PkTimer` 回调、`deleteLater`）永远不执行。`waitFor()` 是
 *     这条义务在本文件里的实现——它**替代** Qt 的 `QTest::qWait()`，因为 Qt 的事件
 *     循环不驱动 pk 队列。
 *
 * 这条义务只对「目标线程」成立；`post()` 本身不触碰调用者自己线程的状态。
 */
inline void waitFor(int milliseconds)
{
    // 分片等待并在每片后 pump：PkTimer 的回调同样是投递进队列的，只有 pump 才会
    // 被投递/触发，所以「睡一次再 pump 一次」会让定时器在等待结束时才猛然前进。
    constexpr int SliceMs = 5;

    int elapsed = 0;
    while (elapsed < milliseconds) {
        const int slice = (milliseconds - elapsed) < SliceMs ? (milliseconds - elapsed) : SliceMs;
        std::this_thread::sleep_for(std::chrono::milliseconds(slice));
        elapsed += slice;
        PkThreadCallQueue::processPendingCalls();
    }
    PkThreadCallQueue::processPendingCalls();
}

} // namespace KritaTestSdk

#endif // PKTHREADCALLQUEUEPUMPHOST_H
