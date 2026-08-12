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
    std::lock_guard<std::mutex> lock(g_mutex);
    for (const auto &pair : g_sinks) {
        const Entry &entry = pair.second;
        entry.fn(level, ctx, message, entry.userData);
    }
}
