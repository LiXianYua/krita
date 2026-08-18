#include "PkThread.h"
#include "PkMutex.h"
#include <thread>
#include <algorithm>
#include <cassert>

int PkThread::idealThreadCount() {
    unsigned int n = std::thread::hardware_concurrency();
    // std::thread::hardware_concurrency() 允许返回 0（无法探测时）；
    // Qt 的 QThread::idealThreadCount() 同样在探测失败时返回 -1，
    // 但 kis_updater_context.cpp:21-24 的唯一调用点已经自己兜底：
    //   threadCount = QThread::idealThreadCount();
    //   threadCount = threadCount > 0 ? threadCount : 1;
    // 所以这里直接返回 0（触发调用点的兜底分支），不必额外造一个 -1 值。
    return static_cast<int>(n);
}

namespace {
struct MainThreadState {
    // registered/id 由 mutex 统一保护，不用 std::atomic<bool> + 裸字段——
    // 那个写法会让 registered 对其它线程可见的时刻早于 id 写完的时刻，
    // 在这个窗口期读 id 是对非原子字段的数据竞争（UB）。这个函数不是热
    // 路径（进程生命周期内至多写一次），加锁是最简单且明确正确的修法，
    // 不需要为它设计无锁方案。
    PkMutex mutex;
    bool registered = false;
    PkThreadId id{};
};
MainThreadState& mainThreadState() {
    static MainThreadState s;
    return s;
}
}

PkThreadId PkThread::currentThreadId() {
    return std::this_thread::get_id();
}

void PkThread::registerMainThread() {
    auto& s = mainThreadState();
    PkThreadId me = std::this_thread::get_id();
    PkMutexLocker lock(&s.mutex);
    if (s.registered) {
        assert(s.id == me && "registerMainThread() called from a different thread than the first registration");
        return;
    }
    s.registered = true;
    s.id = me;
}

PkThreadId PkThread::mainThreadId() {
    auto& s = mainThreadState();
    // mainThreadId() 与 registerMainThread() 同一把锁读写 s.id——
    // 否则这里的读仍然可能跟 registerMainThread() 里的写产生同样的
    // 数据竞争，只是把窗口从 registered/id 之间挪到了 id 本身的写/读之间。
    PkMutexLocker lock(&s.mutex);
    return s.id;
}
