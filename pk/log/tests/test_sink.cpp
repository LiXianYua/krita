#include "test_sink.h"
#include "../PkLogBackend.h"
#include "../PkLogSink.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include <unistd.h>

static std::vector<std::string> g_captured;

static void capture(PkLogLevel level, const PkLogContext &ctx,
                    const char *message, void *)
{
    g_captured.push_back(std::string(ctx.category) + "|" +
                         std::to_string(static_cast<int>(level)) + "|" + message);
}

void PkLogSinkTest::testSinkReceivesMessage()
{
    g_captured.clear();
    const int h = PkLogAddSink(capture, nullptr);
    PkLogEnsureLogger("krita.general", PkLogDebug);
    PkLogEmit(PkLogWarning, PkLogContext{"f.cpp", 7, "fn", "krita.general"}, "hello");
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_captured.size()), 1);
    PK_COMPARE(g_captured[0], std::string("krita.general|2|hello"));
}

void PkLogSinkTest::testRemovedSinkStopsReceiving()
{
    g_captured.clear();
    const int h = PkLogAddSink(capture, nullptr);
    PkLogRemoveSink(h);
    PkLogEmit(PkLogWarning, PkLogContext{"f.cpp", 7, "fn", "krita.general"}, "hello");
    PK_COMPARE(static_cast<int>(g_captured.size()), 0);
}

// ---- 评审 Critical 项：sink 回调体内重入 PkLogRemoveSink / PkLogEmit 不应死锁 ----
//
// std::mutex 非递归。旧实现的 PkLogDispatchToSinks 在持有 g_mutex 的情况下
// 逐个调用 entry.fn(...)——回调体内任何再次获取同一把锁的路径（自注销、
// 或间接经 PkLogEmit 二次进入分发）都会在同一线程对非递归锁二次加锁，
// 未定义行为，实测挂起。这两个用例如果在修复前的代码上跑，会直接挂死，
// 不会走到任何 PK_COMPARE。

namespace {

int g_selfUnregisterHandle = 0;
int g_selfUnregisterCallCount = 0;

void selfUnregisterCallback(PkLogLevel, const PkLogContext &, const char *, void *)
{
    ++g_selfUnregisterCallCount;
    // 在自己的回调体内注销自己。
    PkLogRemoveSink(g_selfUnregisterHandle);
}

} // namespace

void PkLogSinkTest::testSelfUnregisterDuringDispatchDoesNotDeadlock()
{
    g_selfUnregisterCallCount = 0;
    g_selfUnregisterHandle = PkLogAddSink(selfUnregisterCallback, nullptr);

    PkLogEmit(PkLogWarning, PkLogContext{"f.cpp", 7, "fn", "krita.general"}, "first");
    PK_COMPARE(g_selfUnregisterCallCount, 1);

    // 第二次 emit：sink 应该已经在上一次回调里把自己注销掉了。
    PkLogEmit(PkLogWarning, PkLogContext{"f.cpp", 7, "fn", "krita.general"}, "second");
    PK_COMPARE(g_selfUnregisterCallCount, 1);
}

namespace {

int g_reentrantEmitDepth = 0;
std::vector<std::string> g_reentrantEmitLog;

void reentrantEmitCallback(PkLogLevel, const PkLogContext &ctx, const char *message, void *)
{
    g_reentrantEmitLog.push_back(std::string(ctx.category) + "|" + message);
    if (g_reentrantEmitDepth == 0) {
        // 在自己的回调体内再发一条日志——间接二次进入 PkLogDispatchToSinks。
        g_reentrantEmitDepth = 1;
        PkLogEmit(PkLogWarning, PkLogContext{"f.cpp", 8, "fn", "krita.general"}, "nested");
        g_reentrantEmitDepth = 0;
    }
}

} // namespace

void PkLogSinkTest::testEmitDuringDispatchDoesNotDeadlock()
{
    g_reentrantEmitLog.clear();
    g_reentrantEmitDepth = 0;
    const int h = PkLogAddSink(reentrantEmitCallback, nullptr);
    PkLogEnsureLogger("krita.general", PkLogDebug);

    PkLogEmit(PkLogWarning, PkLogContext{"f.cpp", 7, "fn", "krita.general"}, "outer");
    PkLogRemoveSink(h);

    PK_COMPARE(static_cast<int>(g_reentrantEmitLog.size()), 2);
    PK_COMPARE(g_reentrantEmitLog[0], std::string("krita.general|outer"));
    PK_COMPARE(g_reentrantEmitLog[1], std::string("krita.general|nested"));
}

// ---- 评审 Important 项：没 PkLogEnsureLogger 就 PkLogEmit，不应静默兜底 ----
//
// 旧实现在 spdlog::get(ctx.category) 落空时会静默 spdlog::stderr_color_mt()
// 建一个默认级别（info）的 logger 并正常打一行 spdlog 格式的日志（形如
// "[时间戳] [分类] [warning] 消息"，见 task-1-report.md 里贴的原始输出）。
// 新实现应该只打印自定义诊断行、不把消息交给 spdlog。用 dup2 把 stderr
// 重定向到临时文件来抓这行输出，而不直接碰 spdlog（pklog 的使用者对 spdlog
// 一无所知这条设计原则，测试代码也不例外）。
void PkLogSinkTest::testEmitWithoutEnsureLoggerSkipsSilentFallback()
{
    g_captured.clear();
    const int h = PkLogAddSink(capture, nullptr);

    fflush(stderr);
    char path[] = "/tmp/pklog_test_stderr_XXXXXX";
    const int tmpFd = mkstemp(path);
    PK_VERIFY(tmpFd >= 0);
    const int savedStderr = dup(STDERR_FILENO);
    PK_VERIFY(savedStderr >= 0);
    dup2(tmpFd, STDERR_FILENO);
    close(tmpFd);

    // 这个分类名在整个测试进程里从未经过 PkLogEnsureLogger。
    PkLogEmit(PkLogWarning,
              PkLogContext{"f.cpp", 7, "fn", "krita.never_ensured_category"},
              "should not silently fall back");

    fflush(stderr);
    dup2(savedStderr, STDERR_FILENO);
    close(savedStderr);

    // sink 通道不受影响：即使 spdlog 侧跳过了，PkLogAddSink 订阅者仍然收到。
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_captured.size()), 1);

    std::string capturedStderr;
    FILE *f = fopen(path, "rb");
    PK_VERIFY(f != nullptr);
    char buf[4096];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        capturedStderr.append(buf, n);
    }
    fclose(f);
    std::remove(path);

    PK_VERIFY(capturedStderr.find("PkLogEmit:") != std::string::npos);
    PK_VERIFY(capturedStderr.find("krita.never_ensured_category") != std::string::npos);
    // 旧实现走 spdlog 的彩色 stderr sink，打出的一行带 "[warning]" 字样；
    // 新实现完全没把这条消息交给 spdlog，这个词不应该出现。
    PK_VERIFY(capturedStderr.find("[warning]") == std::string::npos);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_sink.inc"

int run_sink_tests(int argc, char **argv)
{
    PkLogSinkTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
