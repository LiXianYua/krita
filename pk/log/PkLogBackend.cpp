// 唯一 #include <spdlog/spdlog.h> 的文件——pklog 的使用者对 spdlog 一无所知
// （CMakeLists.txt 里 target_link_libraries(pklog PRIVATE spdlog::spdlog)）。
// 换后端只用改这一个翻译单元。
#include "PkLogBackend.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include <spdlog/spdlog.h>
#ifdef _WIN32
#include <spdlog/sinks/wincolor_sink-inl.h>
#else
#include <spdlog/sinks/ansicolor_sink-inl.h>
#endif

#include "PkLogSink.h"

namespace {

// PkLogLevel 的取值刻意与 spdlog::level::level_enum 无关（见 PkLogLevel.h），
// 这里做一次性映射。5 档对 5 档，单调递增，PkLogFatal 落 critical——
// abort 由调用方 PkLogEmit 在 log() 之后另行触发。
spdlog::level::level_enum mapLevel(PkLogLevel level)
{
    switch (level) {
    case PkLogDebug:
        return spdlog::level::debug;
    case PkLogInfo:
        return spdlog::level::info;
    case PkLogWarning:
        return spdlog::level::warn;
    case PkLogCritical:
        return spdlog::level::err;
    case PkLogFatal:
        return spdlog::level::critical;
    }
    // 兜底：level 是普通 enum，理论上能被越界的 int 强转进来。
    return spdlog::level::info;
}

// spdlog 的 registry 和默认 console mutex 都是普通函数局部静态对象。
// 跨 DSO 的析构顺序无法保证：其他静态对象的析构函数仍然可能记日志，
// 此时再进 spdlog::get()/stderr_color_mt() 就会访问已析构的 registry。
//
// pk/log 因此保有自己的具名 logger 表，并让这个表、它的锁以及彩色
// stderr sink 的锁都持续到进程结束。这些对象故意不析构：它们正是
// 静态析构日志所依赖的最后一层基础设施。直接构造 spdlog::logger 与
// stderr color sink 仍保留原有的级别过滤、源位置、格式化和彩色 stderr
// 输出语义，只不再把生命周期交给 spdlog 的全局 registry。
struct ProcessLifetimeConsoleMutex
{
    using mutex_t = std::mutex;

    static mutex_t &mutex()
    {
        static auto *const mutex = new mutex_t;
        return *mutex;
    }
};

#ifdef _WIN32
using ProcessLifetimeStderrSink =
    spdlog::sinks::wincolor_stderr_sink<ProcessLifetimeConsoleMutex>;
#else
using ProcessLifetimeStderrSink =
    spdlog::sinks::ansicolor_stderr_sink<ProcessLifetimeConsoleMutex>;
#endif

struct BackendState
{
    std::mutex mutex;
    std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> loggers;
};

BackendState &backendState()
{
    static auto *const state = new BackendState;
    return *state;
}

std::shared_ptr<spdlog::logger> ensureLogger(const char *categoryName,
                                             PkLogLevel minLevel)
{
    BackendState &state = backendState();
    std::lock_guard<std::mutex> lock(state.mutex);

    auto it = state.loggers.find(categoryName);
    if (it == state.loggers.end()) {
        auto sink = std::make_shared<ProcessLifetimeStderrSink>(
            spdlog::color_mode::automatic);
        auto logger = std::make_shared<spdlog::logger>(categoryName, std::move(sink));
        it = state.loggers.emplace(categoryName, std::move(logger)).first;
    }
    it->second->set_level(mapLevel(minLevel));
    return it->second;
}

std::shared_ptr<spdlog::logger> findLogger(const char *categoryName)
{
    BackendState &state = backendState();
    std::lock_guard<std::mutex> lock(state.mutex);
    const auto it = state.loggers.find(categoryName);
    return it == state.loggers.end() ? nullptr : it->second;
}

} // namespace

void PkLogEnsureLogger(const char *categoryName, PkLogLevel minLevel)
{
    ensureLogger(categoryName, minLevel);
}

void PkLogEmit(PkLogLevel level, const PkLogContext &ctx, const std::string &message)
{
    // 先 sink 后 spdlog：Flutter 侧订阅不受 spdlog 的 minLevel 过滤影响，
    // 语义由 tests/test_sink.cpp 钉住。
    PkLogDispatchToSinks(level, ctx, message.c_str());

    auto logger = findLogger(ctx.category);
    if (!logger && std::strcmp(ctx.category, "default") == 0) {
        // 评审 Critical 项：无分类日志族一条都不落盘。
        //
        // "default" 是 PkMessageLogger 在 cat==nullptr 时落的字面分类
        // （PkMessageLogger.cpp:39，qDebug()/qInfo()/qWarning()/qCritical()/
        // qFatal() 这五个无分类自由函数宏的落点，约 1301 处真实调用点）。
        // 它不经过 PkLoggingCategory 那条"构造即注册"路径（PkLoggingCategory.cpp
        // 的构造函数才会调 PkLogEnsureLogger），所以第一次用到时这里懒建一次。
        //
        // 这不是把 Task 1 修复轮去掉的"任意未知分类都静默兜底"恢复回来——
        // 那条针对的是调用方拼错/忘记 PkLogEnsureLogger 的分类名，恢复兜底会
        // 悄悄绕开调用方本该设的 minLevel。这里只认 "default" 这一个我们自己
        // 定义、语义明确的字面量；除它之外的任何分类名（包括拼写近似的）都
        // 仍然落进下面的诊断分支，不会被这条特判捞走。
        // minLevel 给最低档 PkLogDebug：无分类版本本身不受 QLoggingCategory
        // 管（PkMessageLogger.h 顶部注释），这里给最低档等价于"不过滤"。
        logger = ensureLogger(ctx.category, PkLogDebug);
    }
    if (!logger) {
        // 评审 Important 项：不再静默兜底创建 logger。旧行为是
        // spdlog::stderr_color_mt(ctx.category)，用 spdlog 默认级别（info），
        // 会绕开调用方本该经 PkLogEnsureLogger 设的 minLevel，让过滤策略悄悄
        // 失真而不报错——brief 也没有要求这条路径，多出来就是负债。
        //
        // 选择「去掉兜底」（brief 倾向 a），但不用 assert/abort 让整个宿主进程
        // （Krita 是交互式绘画应用）因为一次日志调用点配置遗漏就崩掉：
        // - sink 通道（上面 PkLogDispatchToSinks 已经分发）不受影响，
        //   Flutter 侧订阅仍然收到这条消息；
        // - spdlog 侧显式打一行诊断到 stderr 再跳过，不产出一条误导性的、
        //   级别过滤失真的日志行，也不假装什么都没发生；
        // - PkLogFatal 的"终止进程"契约独立于 logger 是否建好，照样 abort，
        //   不因为这条防御路径悄悄失效。
        std::fprintf(stderr,
                     "PkLogEmit: no logger for category \"%s\" -- call "
                     "PkLogEnsureLogger first (message dropped from spdlog "
                     "sink, still delivered to PkLogAddSink subscribers)\n",
                     ctx.category);
        if (level == PkLogFatal) {
            std::abort();
        }
        return;
    }
    logger->log(spdlog::source_loc{ctx.file, ctx.line, ctx.function},
                mapLevel(level), message);

    if (level == PkLogFatal) {
        std::abort();
    }
}
