#include "PkLogSink.h"

#include <mutex>
#include <utility>
#include <vector>

namespace {

struct Entry {
    PkLogSinkFn fn;
    void *userData;
};

// Krita 多线程日志是常态，注册表本身要能并发读写。
std::mutex g_mutex;
std::vector<std::pair<int, Entry>> g_sinks;
int g_nextHandle = 1;

} // namespace

int PkLogAddSink(PkLogSinkFn fn, void *userData)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    const int handle = g_nextHandle++;
    g_sinks.push_back(std::make_pair(handle, Entry{fn, userData}));
    return handle;
}

void PkLogRemoveSink(int handle)
{
    std::lock_guard<std::mutex> lock(g_mutex);
    for (auto it = g_sinks.begin(); it != g_sinks.end(); ++it) {
        if (it->first == handle) {
            g_sinks.erase(it);
            return;
        }
    }
}

void PkLogDispatchToSinks(PkLogLevel level, const PkLogContext &ctx, const char *message)
{
    // 评审 Critical 项：std::mutex 不可重入。sink 回调体内如果调用
    // PkLogAddSink/PkLogRemoveSink（自注销）或 PkLogEmit（间接二次进入本函数），
    // 持锁期间回调就会在同一线程对 g_mutex 二次加锁——未定义行为，实测挂起。
    // 修法：在锁内拷一份快照，解锁之后再逐个调用，回调体内可以随意再次
    // 加/解注册表的锁而不会自我重入。
    std::vector<Entry> snapshot;
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        snapshot.reserve(g_sinks.size());
        for (const auto &pair : g_sinks) {
            snapshot.push_back(pair.second);
        }
    }
    for (const auto &entry : snapshot) {
        entry.fn(level, ctx, message, entry.userData);
    }
}
