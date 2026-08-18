#include "PkThread.h"
#include <thread>
#include <algorithm>
#include <atomic>
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
    std::atomic<bool> registered{false};
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
    bool wasRegistered = s.registered.exchange(true);
    if (wasRegistered) {
        assert(s.id == me && "registerMainThread() called from a different thread than the first registration");
        return;
    }
    s.id = me;
}

PkThreadId PkThread::mainThreadId() {
    return mainThreadState().id;
}
